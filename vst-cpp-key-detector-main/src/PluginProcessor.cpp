#include "PluginProcessor.h"
#include "PluginEditor.h"

KeyDetectorProcessor::KeyDetectorProcessor()
    : AudioProcessor(BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(modeParam  = new juce::AudioParameterChoice(
        "mode", "Input Mode",
        juce::StringArray { "MIDI Only", "Audio Only", "Blend" },
        0 /* default: MIDI Only */));

    addParameter(blendParam = new juce::AudioParameterFloat(
        "blend", "MIDI/Audio Blend",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f /* default: equal mix */));

    // Locate markov_transitions.json at run-time.
    // In development builds PLUGIN_DEV_DATA_DIR is set by CMake to the
    // project's data/ directory; in release builds a proper bundle path
    // will be resolved in Sprint 12.
#ifdef PLUGIN_DEV_DATA_DIR
    const std::string jsonPath = PLUGIN_DEV_DATA_DIR "/markov_transitions.json";
#else
    const std::string jsonPath;   // TODO Sprint 12: resolve bundle asset path
#endif
    _progressionSuggester = std::make_unique<ProgressionSuggester>(jsonPath);
}

void KeyDetectorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    _sampleRate = sampleRate;
    _midiAnalyzer.prepare(sampleRate, samplesPerBlock);
    _audioAnalyzer = std::make_unique<AudioAnalyzer>(sampleRate);
}

void KeyDetectorProcessor::scheduleChord(const std::array<int, 4>& midiNotes)
{
    const juce::SpinLock::ScopedLockType lock(_chordScheduleLock);
    _pendingChord.notes  = midiNotes;
    _pendingChord.active = true;
}

void KeyDetectorProcessor::releaseResources()
{
    _audioAnalyzer.reset();
}

void KeyDetectorProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    _midiAnalyzer.processMidiBuffer(midi);

    if (buffer.getNumChannels() > 0 && _audioAnalyzer)
        _audioAnalyzer->processAudioBlock(buffer.getReadPointer(0), buffer.getNumSamples());

    // ── Fuse MIDI and audio profiles according to current parameter values ────
    const auto midiPCP  = _midiAnalyzer.getNormalizedProfile();
    const auto audioPCP = _audioAnalyzer ? _audioAnalyzer->getChromaVector()
                                         : std::array<float, 12>{};

    const int modeIdx = modeParam->getIndex();
    const auto mode   = (modeIdx == 1) ? ProfileFuser::Mode::AudioOnly
                      : (modeIdx == 2) ? ProfileFuser::Mode::Blend
                                       : ProfileFuser::Mode::MidiOnly;

    // blendParam = 0 → full MIDI; = 1 → full Audio → midiWeight = 1 - blend
    const float midiWeight = 1.0f - blendParam->get();
    const auto  pcp        = ProfileFuser::fuse(midiPCP, audioPCP, mode, midiWeight);

    // ── Estimate key and scale from fused PCP ─────────────────────────────────
    const auto keyResult   = _keyEstimator.estimate(pcp);
    const auto scaleResult = _scaleClassifier.classify(pcp, keyResult.root);
    const auto activeNotes = _midiAnalyzer.getActiveNotes();

    // ── Chord inference + progression suggestions ─────────────────────────────
    std::string              roman = "?";
    std::vector<ChordSuggestion> suggestions;
    if (_progressionSuggester)
    {
        roman       = _progressionSuggester->inferCurrentRoman(
                          activeNotes, keyResult.root, keyResult.mode);
        suggestions = _progressionSuggester->suggest(
                          roman, keyResult.mode, keyResult.root, 3);
    }

    // TryLock: skip update if UI is reading rather than blocking the audio thread
    juce::SpinLock::ScopedTryLockType tryLock(_resultLock);
    if (tryLock.isLocked())
    {
        _lastKeyResult   = keyResult;
        _lastScaleResult = scaleResult;
        _activeNotes     = activeNotes;
        _lastRoman       = roman;
        _lastSuggestions = std::move(suggestions);
    }

    // ── MIDI chord playback (UI button → audio thread) ────────────────────────
    // Note injection goes after analysis so playback notes don't skew detection.
    {
        bool               pending = false;
        std::array<int, 4> pnotes  {};
        {
            const juce::SpinLock::ScopedTryLockType lk(_chordScheduleLock);
            if (lk.isLocked() && _pendingChord.active)
            {
                pending              = true;
                pnotes               = _pendingChord.notes;
                _pendingChord.active = false;
            }
        }

        if (pending)
        {
            // Cancel any notes still ringing from a previous click
            for (auto& sn : _scheduledNotes)
                if (sn.on) { midi.addEvent(juce::MidiMessage::noteOff(1, sn.midi), 0); sn.on = false; }

            const int durSamples = static_cast<int>(_sampleRate * 0.5);  // 500 ms
            for (size_t i = 0; i < _scheduledNotes.size(); ++i)
            {
                const int m = pnotes[i];
                if (m >= 0 && m < 128)
                {
                    midi.addEvent(juce::MidiMessage::noteOn(1, m, (juce::uint8) 90), 0);
                    _scheduledNotes[i] = { m, durSamples, true };
                }
            }
        }

        // Count down active notes and send note-offs when they expire
        for (auto& sn : _scheduledNotes)
        {
            if (!sn.on) continue;
            sn.samplesLeft -= buffer.getNumSamples();
            if (sn.samplesLeft <= 0)
            {
                midi.addEvent(juce::MidiMessage::noteOff(1, sn.midi), 0);
                sn.on = false;
            }
        }
    }
}

juce::AudioProcessorEditor* KeyDetectorProcessor::createEditor()
{
    return new KeyDetectorEditor(*this);
}

void KeyDetectorProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save all parameter values as XML
    if (auto xml = std::unique_ptr<juce::XmlElement>(new juce::XmlElement("Params")))
    {
        xml->setAttribute("mode",  modeParam->getIndex());
        xml->setAttribute("blend", static_cast<double>(blendParam->get()));
        copyXmlToBinary(*xml, destData);
    }
}

void KeyDetectorProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = std::unique_ptr<juce::XmlElement>(getXmlFromBinary(data, sizeInBytes)))
    {
        if (xml->hasTagName("Params"))
        {
            *modeParam  = xml->getIntAttribute("mode",  0);
            *blendParam = static_cast<float>(xml->getDoubleAttribute("blend", 0.5));
        }
    }
}

KeyResult KeyDetectorProcessor::getLastKeyResult() const
{
    const juce::SpinLock::ScopedLockType lock(_resultLock);
    return _lastKeyResult;
}

ScaleResult KeyDetectorProcessor::getLastScaleResult() const
{
    const juce::SpinLock::ScopedLockType lock(_resultLock);
    return _lastScaleResult;
}

std::array<bool, 12> KeyDetectorProcessor::getActiveNotes() const
{
    const juce::SpinLock::ScopedLockType lock(_resultLock);
    return _activeNotes;
}

std::string KeyDetectorProcessor::getLastRoman() const
{
    const juce::SpinLock::ScopedLockType lock(_resultLock);
    return _lastRoman;
}

std::vector<ChordSuggestion> KeyDetectorProcessor::getLastSuggestions() const
{
    const juce::SpinLock::ScopedLockType lock(_resultLock);
    return _lastSuggestions;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KeyDetectorProcessor();
}
