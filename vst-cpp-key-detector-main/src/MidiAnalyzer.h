#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

class MidiAnalyzer {
public:
    // Call from prepareToPlay — precomputes block-size-aware decay factor
    void prepare(double sampleRate, int samplesPerBlock);

    void processMidiBuffer(const juce::MidiBuffer& midi);
    void reset();

    std::array<float, 12> getNormalizedProfile() const;
    std::array<bool, 12>  getActiveNotes() const;

private:
    std::array<float, 12> _histogram {};  // duration-weighted pitch class accumulator
    std::array<bool,  12> _heldNotes {};  // notes currently with note-on active
    float _blockDecay { 1.0f };           // precomputed once in prepare()

    // 50% decay in this many seconds regardless of block size
    static constexpr float DECAY_HALF_LIFE_SECONDS = 4.0f;
};
