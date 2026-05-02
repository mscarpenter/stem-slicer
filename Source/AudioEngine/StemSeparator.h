#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "AudioFileReader.h"
#include "AudioFileWriter.h"
#include "StemResult.h"
#include "../ML/InferenceBackend.h"

namespace audio {

// STFT constants aligned with Spleeter 4stems — see spleeter/spleeter/audio/stft.py
static constexpr int STFT_N_FFT       = 4096;
static constexpr int STFT_HOP_LENGTH  = 1024;
static constexpr int STFT_SAMPLE_RATE = 44100;
static constexpr int MODEL_F_BINS     = 1024;  // frequency bins consumed by model (lower half)
static constexpr int MODEL_T_FRAMES   = 512;   // time frames per chunk
static constexpr int MODEL_CHANNELS   = 2;     // stereo (L and R processed independently)

class StemSeparator {
public:
    explicit StemSeparator(std::unique_ptr<ml::InferenceBackend> backend);
    ~StemSeparator() = default;

    StemSeparator(const StemSeparator&)            = delete;
    StemSeparator& operator=(const StemSeparator&) = delete;

    // Separates mixture into 4 stereo stems at sampleRate.
    // mixture must be float PCM mono or stereo; if mono, L and R are duplicated.
    // STFT uses center=True (N_FFT/2 zero-padding on each side), matching librosa default.
    // Input tensor shape per chunk: [1, MODEL_T_FRAMES, MODEL_F_BINS, MODEL_CHANNELS].
    // Returns empty StemResult if backend is null or model not loaded.
    // chunkProgress, if set, is called with [0,1] progress after each inference chunk.
    StemResult separate(const juce::AudioBuffer<float>& mixture,
                        double sampleRate,
                        std::function<void(float)> chunkProgress = nullptr);

    // Reads inputPath, separates into 4 stems, writes vocals/drums/bass/other.wav to outputDir.
    // progressCallback is called with values in [0.0, 1.0] as processing advances.
    // Throws std::runtime_error on any failure (file not found, model not loaded, write error).
    void separateFile(
        const std::filesystem::path& inputPath,
        const std::filesystem::path& outputDir,
        std::function<void(float)> progressCallback = nullptr);

private:
    std::unique_ptr<ml::InferenceBackend> backend_;
    juce::dsp::FFT fft_;
};

} // namespace audio
