#include "MidiAnalyzer.h"
#include <cmath>

void MidiAnalyzer::prepare(double sampleRate, int samplesPerBlock)
{
    // 0.5^(blockSize / (sampleRate * halfLife)) → consistent decay regardless of block size
    _blockDecay = std::pow(0.5f,
        static_cast<float>(samplesPerBlock) /
        static_cast<float>(sampleRate * static_cast<double>(DECAY_HALF_LIFE_SECONDS)));
}

void MidiAnalyzer::processMidiBuffer(const juce::MidiBuffer& midi)
{
    for (auto& v : _histogram)
        v *= _blockDecay;

    for (const auto metadata : midi)
    {
        const auto msg        = metadata.getMessage();
        const int  pitchClass = msg.getNoteNumber() % 12;

        if (msg.isNoteOn())
        {
            _heldNotes[pitchClass]  = true;
            _histogram[pitchClass] += static_cast<float>(msg.getVelocity()) / 127.0f;
        }
        else if (msg.isNoteOff())
        {
            _heldNotes[pitchClass] = false;
        }
    }
}

void MidiAnalyzer::reset()
{
    _histogram.fill(0.0f);
    _heldNotes.fill(false);
}

std::array<float, 12> MidiAnalyzer::getNormalizedProfile() const
{
    float sum = 0.0f;
    for (float v : _histogram) sum += v;

    std::array<float, 12> normalized {};
    if (sum > 1e-6f)
        for (int i = 0; i < 12; ++i)
            normalized[i] = _histogram[i] / sum;

    return normalized;
}

std::array<bool, 12> MidiAnalyzer::getActiveNotes() const
{
    return _heldNotes;
}
