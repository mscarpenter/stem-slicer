#include "StemSeparator.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace audio {

namespace {

constexpr int kFFTOrder = 12;                   // 2^12 = STFT_N_FFT = 4096
constexpr int kNBins    = STFT_N_FFT / 2 + 1;  // 2049 — full one-sided spectrum
constexpr int kNumStems = 4;

std::vector<float> buildHannWindow(int size)
{
    std::vector<float> w(static_cast<size_t>(size));
    const float twoPi = 2.0f * juce::MathConstants<float>::pi;
    for (int i = 0; i < size; ++i)
        w[static_cast<size_t>(i)] = 0.5f * (1.0f - std::cos(twoPi * i / size));
    return w;
}

// Returns forward STFT frames, each of length 2*STFT_N_FFT (interleaved complex).
// Uses onlyCalculateNonNegativeFrequencies=true; bins kNBins..STFT_N_FFT-1 are zeroed.
std::vector<std::vector<float>> computeSTFT(
    juce::dsp::FFT& fft,
    const float* samples, int numSamples,
    const std::vector<float>& window)
{
    const int bufLen = STFT_N_FFT * 2;

    std::vector<std::vector<float>> frames;
    frames.reserve(static_cast<size_t>((numSamples - STFT_N_FFT) / STFT_HOP_LENGTH + 1));

    for (int pos = 0; pos + STFT_N_FFT <= numSamples; pos += STFT_HOP_LENGTH)
    {
        std::vector<float> buf(static_cast<size_t>(bufLen), 0.0f);
        for (int i = 0; i < STFT_N_FFT; ++i)
            buf[static_cast<size_t>(i)] = samples[pos + i] * window[static_cast<size_t>(i)];
        fft.performRealOnlyForwardTransform(buf.data(), true);
        frames.push_back(std::move(buf));
    }
    return frames;
}

// Overlap-add ISTFT. Frames are modified in place (Hermitian symmetry restored before IFFT).
// Returns a single-channel AudioBuffer of length targetNumSamples.
juce::AudioBuffer<float> computeISTFT(
    juce::dsp::FFT& fft,
    std::vector<std::vector<float>>& frames,
    const std::vector<float>& window,
    int targetNumSamples)
{
    const int nFrames = static_cast<int>(frames.size());
    if (nFrames == 0)
        return juce::AudioBuffer<float>(1, 0);

    const int outLen = (nFrames - 1) * STFT_HOP_LENGTH + STFT_N_FFT;

    std::vector<float> outBuf(static_cast<size_t>(outLen), 0.0f);
    std::vector<float> winSum(static_cast<size_t>(outLen), 0.0f);

    for (int f = 0; f < nFrames; ++f)
    {
        auto& frame = frames[static_cast<size_t>(f)];

        // Restore Hermitian symmetry for bins above MODEL_F_BINS up to kNBins (mirror of 1..kNBins-1).
        // Bins MODEL_F_BINS..kNBins-1 were zeroed when building masked frames (mask_extension="zeros").
        // Bins kNBins..STFT_N_FFT-1 must satisfy conjugate symmetry for real IFFT.
        for (int b = kNBins; b < STFT_N_FFT; ++b)
        {
            const int mirror = STFT_N_FFT - b;
            frame[static_cast<size_t>(b * 2)]     =  frame[static_cast<size_t>(mirror * 2)];
            frame[static_cast<size_t>(b * 2 + 1)] = -frame[static_cast<size_t>(mirror * 2 + 1)];
        }

        fft.performRealOnlyInverseTransform(frame.data());

        const int start = f * STFT_HOP_LENGTH;
        for (int i = 0; i < STFT_N_FFT; ++i)
        {
            const int idx = start + i;
            if (idx >= outLen) break;
            const float w = window[static_cast<size_t>(i)];
            outBuf[static_cast<size_t>(idx)] += frame[static_cast<size_t>(i)] * w;
            winSum[static_cast<size_t>(idx)] += w * w;
        }
    }

    for (int i = 0; i < outLen; ++i)
        if (winSum[static_cast<size_t>(i)] > 1e-8f)
            outBuf[static_cast<size_t>(i)] /= winSum[static_cast<size_t>(i)];

    // Sempre aloca targetNumSamples — garante que o caller pode ler até [padLen + numSamples - 1]
    juce::AudioBuffer<float> result(1, targetNumSamples);
    result.clear();
    result.copyFrom(0, 0, outBuf.data(), std::min(outLen, targetNumSamples));
    return result;
}

} // namespace

StemSeparator::StemSeparator(std::unique_ptr<ml::InferenceBackend> backend)
    : backend_(std::move(backend)), fft_(kFFTOrder)
{}

StemResult StemSeparator::separate(const juce::AudioBuffer<float>& mixture,
                                    double sampleRate,
                                    std::function<void(float)> chunkProgress)
{
    if (!backend_)
        return {};

    if (std::abs(sampleRate - static_cast<double>(STFT_SAMPLE_RATE)) > 0.5)
        throw std::runtime_error(
            "StemSeparator: sampleRate " + std::to_string(static_cast<int>(sampleRate)) +
            " not supported; expected " + std::to_string(STFT_SAMPLE_RATE) +
            ". Resample before calling separate().");

    const int numSamples  = mixture.getNumSamples();
    const int numChannels = mixture.getNumChannels();

    if (numSamples < STFT_N_FFT)
        throw std::runtime_error(
            "StemSeparator: audio too short (" + std::to_string(numSamples) +
            " samples); minimum is " + std::to_string(STFT_N_FFT) +
            " samples (~93ms at 44100Hz).");

    // center=True: pad N_FFT/2 zeros on each side of each channel.
    // This matches librosa.stft(center=True), ensuring the first frame centres on sample 0.
    const int padLen    = STFT_N_FFT / 2;
    const int paddedLen = numSamples + STFT_N_FFT;  // pad + signal + pad

    auto buildPaddedChannel = [&](int ch) -> std::vector<float> {
        std::vector<float> padded(static_cast<size_t>(paddedLen), 0.0f);
        const float* src = mixture.getReadPointer(ch < numChannels ? ch : 0);
        for (int i = 0; i < numSamples; ++i)
            padded[static_cast<size_t>(padLen + i)] = src[i];
        return padded;
    };

    const std::vector<float> paddedL = buildPaddedChannel(0);
    const std::vector<float> paddedR = buildPaddedChannel(numChannels > 1 ? 1 : 0);

    const auto window = buildHannWindow(STFT_N_FFT);

    auto complexFramesL = computeSTFT(fft_, paddedL.data(), paddedLen, window);
    auto complexFramesR = computeSTFT(fft_, paddedR.data(), paddedLen, window);

    const int nFrames = static_cast<int>(complexFramesL.size());
    if (nFrames == 0)
        return {};

    // Build stereo magnitude spectrogram: layout [nFrames, MODEL_F_BINS, MODEL_CHANNELS].
    // Only the lower MODEL_F_BINS bins are sent to the model (matching Spleeter input shape).
    const size_t specSize = static_cast<size_t>(nFrames * MODEL_F_BINS * MODEL_CHANNELS);
    std::vector<float> magSpec(specSize);
    for (int f = 0; f < nFrames; ++f)
    {
        const auto& fL = complexFramesL[static_cast<size_t>(f)];
        const auto& fR = complexFramesR[static_cast<size_t>(f)];
        for (int b = 0; b < MODEL_F_BINS; ++b)
        {
            const float reL = fL[static_cast<size_t>(b * 2)];
            const float imL = fL[static_cast<size_t>(b * 2 + 1)];
            const float reR = fR[static_cast<size_t>(b * 2)];
            const float imR = fR[static_cast<size_t>(b * 2 + 1)];
            const size_t base = static_cast<size_t>(f * MODEL_F_BINS * MODEL_CHANNELS + b * MODEL_CHANNELS);
            magSpec[base + 0] = std::sqrt(reL * reL + imL * imL);
            magSpec[base + 1] = std::sqrt(reR * reR + imR * imR);
        }
    }

    // Chunking: split into blocks of MODEL_T_FRAMES frames; zero-pad last chunk if needed.
    const int nChunks = (nFrames + MODEL_T_FRAMES - 1) / MODEL_T_FRAMES;
    const size_t chunkTensorSize =
        static_cast<size_t>(MODEL_T_FRAMES * MODEL_F_BINS * MODEL_CHANNELS);

    // stemMasksFull[stemIdx]: shape [nFrames, MODEL_F_BINS, MODEL_CHANNELS].
    const size_t fullMaskSize = static_cast<size_t>(nFrames * MODEL_F_BINS * MODEL_CHANNELS);
    std::vector<std::vector<float>> stemMasksFull(static_cast<size_t>(kNumStems),
                                                   std::vector<float>(fullMaskSize, 0.0f));

    std::vector<float> chunkTensor(chunkTensorSize);

    for (int k = 0; k < nChunks; ++k)
    {
        const int startFrame  = k * MODEL_T_FRAMES;
        const int endFrame    = std::min(startFrame + MODEL_T_FRAMES, nFrames);
        const int chunkFrames = endFrame - startFrame;

        std::fill(chunkTensor.begin(), chunkTensor.end(), 0.0f);
        for (int f = 0; f < chunkFrames; ++f)
        {
            const size_t srcBase =
                static_cast<size_t>((startFrame + f) * MODEL_F_BINS * MODEL_CHANNELS);
            const size_t dstBase =
                static_cast<size_t>(f * MODEL_F_BINS * MODEL_CHANNELS);
            std::copy(magSpec.begin() + static_cast<std::ptrdiff_t>(srcBase),
                      magSpec.begin() + static_cast<std::ptrdiff_t>(srcBase + static_cast<size_t>(MODEL_F_BINS * MODEL_CHANNELS)),
                      chunkTensor.begin() + static_cast<std::ptrdiff_t>(dstBase));
        }

        const std::vector<int64_t> inputShape = {
            1,
            static_cast<int64_t>(MODEL_T_FRAMES),
            static_cast<int64_t>(MODEL_F_BINS),
            static_cast<int64_t>(MODEL_CHANNELS)
        };

        const auto chunkOutputs = backend_->runInference(chunkTensor, inputShape);

        if (static_cast<int>(chunkOutputs.size()) < kNumStems)
            throw std::runtime_error(
                "StemSeparator: model returned " + std::to_string(chunkOutputs.size()) +
                " outputs for chunk " + std::to_string(k) +
                ", expected " + std::to_string(kNumStems));

        for (int stemIdx = 0; stemIdx < kNumStems; ++stemIdx)
        {
            const auto& chunkMask = chunkOutputs[static_cast<size_t>(stemIdx)];
            for (int f = 0; f < chunkFrames; ++f)
            {
                const size_t srcBase =
                    static_cast<size_t>(f * MODEL_F_BINS * MODEL_CHANNELS);
                const size_t dstBase =
                    static_cast<size_t>((startFrame + f) * MODEL_F_BINS * MODEL_CHANNELS);
                std::copy(chunkMask.begin() + static_cast<std::ptrdiff_t>(srcBase),
                          chunkMask.begin() + static_cast<std::ptrdiff_t>(srcBase + static_cast<size_t>(MODEL_F_BINS * MODEL_CHANNELS)),
                          stemMasksFull[static_cast<size_t>(stemIdx)].begin() + static_cast<std::ptrdiff_t>(dstBase));
            }
        }

        // Report progress only after validating and copying chunk outputs.
        if (chunkProgress)
            chunkProgress(static_cast<float>(k + 1) / static_cast<float>(nChunks));
    }

    // Pre-allocate masked frame buffers (reused across stems).
    std::vector<std::vector<float>> maskedFramesL(static_cast<size_t>(nFrames));
    std::vector<std::vector<float>> maskedFramesR(static_cast<size_t>(nFrames));
    for (int f = 0; f < nFrames; ++f)
    {
        maskedFramesL[static_cast<size_t>(f)].resize(static_cast<size_t>(STFT_N_FFT * 2));
        maskedFramesR[static_cast<size_t>(f)].resize(static_cast<size_t>(STFT_N_FFT * 2));
    }

    StemResult result;

    auto stemBuffer = [&result](int i) -> juce::AudioBuffer<float>& {
        switch (i) {
            case 0: return result.vocals;
            case 1: return result.drums;
            case 2: return result.bass;
            default: return result.other;
        }
    };

    for (int stemIdx = 0; stemIdx < kNumStems; ++stemIdx)
    {
        const auto& mask = stemMasksFull[static_cast<size_t>(stemIdx)];

        for (int f = 0; f < nFrames; ++f)
        {
            auto& dstL = maskedFramesL[static_cast<size_t>(f)];
            auto& dstR = maskedFramesR[static_cast<size_t>(f)];
            std::fill(dstL.begin(), dstL.end(), 0.0f);
            std::fill(dstR.begin(), dstR.end(), 0.0f);

            const auto& srcL = complexFramesL[static_cast<size_t>(f)];
            const auto& srcR = complexFramesR[static_cast<size_t>(f)];

            for (int b = 0; b < MODEL_F_BINS; ++b)
            {
                const size_t maskIdx =
                    static_cast<size_t>(f * MODEL_F_BINS * MODEL_CHANNELS + b * MODEL_CHANNELS);
                const float mL = mask[maskIdx + 0];
                const float mR = mask[maskIdx + 1];

                dstL[static_cast<size_t>(b * 2)]     = srcL[static_cast<size_t>(b * 2)]     * mL;
                dstL[static_cast<size_t>(b * 2 + 1)] = srcL[static_cast<size_t>(b * 2 + 1)] * mL;
                dstR[static_cast<size_t>(b * 2)]     = srcR[static_cast<size_t>(b * 2)]     * mR;
                dstR[static_cast<size_t>(b * 2 + 1)] = srcR[static_cast<size_t>(b * 2 + 1)] * mR;
                // Bins MODEL_F_BINS..kNBins-1 stay zero (mask_extension="zeros").
                // Bins kNBins..STFT_N_FFT-1 are filled in computeISTFT via Hermitian symmetry.
            }
        }

        // ISTFT operates on the full padded length; trim center-padding afterwards.
        auto outBufL = computeISTFT(fft_, maskedFramesL, window, paddedLen);
        auto outBufR = computeISTFT(fft_, maskedFramesR, window, paddedLen);

        // Remove center-pad: outBuf[padLen .. padLen+numSamples-1] is the original signal.
        juce::AudioBuffer<float> stemBuf(2, numSamples);
        stemBuf.copyFrom(0, 0, outBufL.getReadPointer(0) + padLen, numSamples);
        stemBuf.copyFrom(1, 0, outBufR.getReadPointer(0) + padLen, numSamples);

        stemBuffer(stemIdx) = std::move(stemBuf);
    }

    return result;
}

void StemSeparator::separateFile(
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputDir,
    std::function<void(float)> progressCallback)
{
    auto mixture = loadAudioFile(inputPath, STFT_SAMPLE_RATE);

    if (progressCallback) progressCallback(0.05f);

    // Map chunk progress [0,1] → overall progress [0.1, 0.88].
    // Capture progressCallback by value — separateFile's stack frame must not be
    // assumed to outlive the lambda if separate() is ever made asynchronous.
    auto result = separate(mixture, STFT_SAMPLE_RATE,
        [progressCallback](float p) {
            if (progressCallback)
                progressCallback(0.1f + p * 0.78f);
        });

    if (progressCallback) progressCallback(0.90f);  // separation done, starting write

    // Create a subfolder named after the input file (without extension) so that
    // processing multiple tracks into the same output dir stays organised.
    const auto stemOutputDir = outputDir / inputPath.stem();
    std::filesystem::create_directories(stemOutputDir);

    const std::array<std::pair<std::string, const juce::AudioBuffer<float>*>, 4> stems = {{
        { "vocals", &result.vocals },
        { "drums",  &result.drums  },
        { "bass",   &result.bass   },
        { "other",  &result.other  }
    }};

    for (size_t i = 0; i < stems.size(); ++i)
    {
        writeAudioFile(stemOutputDir / (stems[i].first + ".wav"), *stems[i].second, STFT_SAMPLE_RATE);
        if (progressCallback)
            progressCallback(0.90f + static_cast<float>(i + 1) * 0.025f);  // 0.925, 0.95, 0.975, 1.0
    }
}

} // namespace audio
