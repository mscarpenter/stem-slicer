#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MidiAnalyzer.h"
#include "AudioAnalyzer.h"
#include "KeyEstimator.h"
#include "ScaleClassifier.h"
#include "ProfileFuser.h"
#include "ProgressionSuggester.h"

class KeyDetectorProcessor : public juce::AudioProcessor
{
public:
    KeyDetectorProcessor();
    ~KeyDetectorProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Thread-safe accessors called from the UI timer (message thread)
    KeyResult                    getLastKeyResult()   const;
    ScaleResult                  getLastScaleResult() const;
    std::array<bool, 12>         getActiveNotes()     const;
    std::string                  getLastRoman()       const;
    std::vector<ChordSuggestion> getLastSuggestions() const;

    // Called from the UI thread (button click) to schedule a chord for MIDI output.
    // Notes outside [0,127] are ignored (use -1 as a "no note" sentinel).
    void scheduleChord(const std::array<int, 4>& midiNotes);

    // Parameters — owned by AudioProcessor, raw pointers are safe to read
    juce::AudioParameterChoice* modeParam  { nullptr };  // 0=MIDI, 1=Audio, 2=Blend
    juce::AudioParameterFloat*  blendParam { nullptr };  // 0=full-MIDI … 1=full-Audio

private:
    MidiAnalyzer                       _midiAnalyzer;
    std::unique_ptr<AudioAnalyzer>     _audioAnalyzer;
    KeyEstimator                       _keyEstimator;
    ScaleClassifier                    _scaleClassifier;
    std::unique_ptr<ProgressionSuggester> _progressionSuggester;

    // Shared state between audio thread and UI
    // TODO Fase 5: replace with lock-free AbstractFifo transfer
    KeyResult                    _lastKeyResult {};
    ScaleResult                  _lastScaleResult {};
    std::array<bool, 12>         _activeNotes {};
    std::string                  _lastRoman { "?" };
    std::vector<ChordSuggestion> _lastSuggestions;
    mutable juce::SpinLock       _resultLock;

    // ── MIDI chord playback ───────────────────────────────────────────────────
    double _sampleRate { 44100.0 };

    // Pending chord from UI (SpinLock-protected; set on message thread)
    struct PendingChord { bool active { false }; std::array<int, 4> notes {}; };
    PendingChord       _pendingChord;
    juce::SpinLock     _chordScheduleLock;

    // Active note-offs tracked on audio thread only (no lock needed)
    struct ScheduledNote { int midi { 0 }; int samplesLeft { 0 }; bool on { false }; };
    std::array<ScheduledNote, 4> _scheduledNotes {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyDetectorProcessor)
};
