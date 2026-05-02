#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <filesystem>

namespace audio {

// Writes buffer to a 32-bit float WAV file at the given path.
// Creates parent directories if they don't exist.
// Throws std::runtime_error on failure.
void writeAudioFile(
    const std::filesystem::path& path,
    const juce::AudioBuffer<float>& buffer,
    double sampleRate = 44100.0);

} // namespace audio
