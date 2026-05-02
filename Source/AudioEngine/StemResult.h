#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace audio {

struct StemResult {
    juce::AudioBuffer<float> vocals;
    juce::AudioBuffer<float> drums;
    juce::AudioBuffer<float> bass;
    juce::AudioBuffer<float> other;
};

} // namespace audio
