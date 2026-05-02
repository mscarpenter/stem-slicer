#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <filesystem>

namespace audio {

// Loads an audio file into a stereo AudioBuffer<float> resampled to targetSampleRate.
// Throws std::runtime_error on file-not-found, unsupported format, or read failure.
// Returns a 2-channel buffer — mono input is duplicated to both channels.
juce::AudioBuffer<float> loadAudioFile(
    const std::filesystem::path& path,
    double targetSampleRate = 44100.0);

} // namespace audio
