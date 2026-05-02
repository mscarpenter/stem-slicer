#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <juce_dsp/juce_dsp.h>
#include "AudioAnalyzer.h"

#include <cmath>
#include <vector>

// ─── helpers ──────────────────────────────────────────────────────────────────

static constexpr double PI = 3.14159265358979323846;

static std::vector<float> sineWave(double freqHz, double sampleRate, int numSamples)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] = static_cast<float>(std::sin(2.0 * PI * freqHz * i / sampleRate));
    return buf;
}

static int dominantPC(const std::array<float, 12>& chroma)
{
    int best = 0;
    for (int i = 1; i < 12; ++i)
        if (chroma[i] > chroma[best]) best = i;
    return best;
}

// fftSize = 4096 at sr = 44100; one window = 4096 samples
static constexpr int   FFT_ORDER = 12;
static constexpr int   FFT_SIZE  = 1 << FFT_ORDER;   // 4096
static constexpr double SR       = 44100.0;

// ─── AudioAnalyzer unit tests ─────────────────────────────────────────────────

TEST_CASE("AudioAnalyzer — initial state returns zero chroma", "[audio_analyzer]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    for (float v : aa.getChromaVector())
        CHECK(v == Catch::Approx(0.0f));
}

TEST_CASE("AudioAnalyzer — silence produces zero chroma", "[audio_analyzer]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    std::vector<float> silence(FFT_SIZE, 0.0f);
    aa.processAudioBlock(silence.data(), FFT_SIZE);
    for (float v : aa.getChromaVector())
        CHECK(v == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("AudioAnalyzer — A4 (440 Hz) sine → dominant pitch class 9 (A)", "[audio_analyzer]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    auto samples = sineWave(440.0, SR, FFT_SIZE);
    aa.processAudioBlock(samples.data(), FFT_SIZE);
    CHECK(dominantPC(aa.getChromaVector()) == 9);
}

TEST_CASE("AudioAnalyzer — C4 (261.63 Hz) sine → dominant pitch class 0 (C)", "[audio_analyzer]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    auto samples = sineWave(261.63, SR, FFT_SIZE);
    aa.processAudioBlock(samples.data(), FFT_SIZE);
    CHECK(dominantPC(aa.getChromaVector()) == 0);
}

TEST_CASE("AudioAnalyzer — E4 (329.63 Hz) sine → dominant pitch class 4 (E)", "[audio_analyzer]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    auto samples = sineWave(329.63, SR, FFT_SIZE);
    aa.processAudioBlock(samples.data(), FFT_SIZE);
    CHECK(dominantPC(aa.getChromaVector()) == 4);
}

TEST_CASE("AudioAnalyzer — chroma values are in [0, 1]", "[audio_analyzer]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    auto samples = sineWave(440.0, SR, FFT_SIZE);
    aa.processAudioBlock(samples.data(), FFT_SIZE);
    for (float v : aa.getChromaVector())
    {
        CHECK(v >= 0.0f);
        CHECK(v <= 1.001f);
    }
}

TEST_CASE("AudioAnalyzer — empty block does not crash", "[audio_analyzer]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    REQUIRE_NOTHROW(aa.processAudioBlock(nullptr, 0));
}

TEST_CASE("AudioAnalyzer — multiple hops: dominant PC remains stable", "[audio_analyzer]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    // Feed 4 full windows of A4 (triggers 7 FFT hops via 50% overlap)
    auto samples = sineWave(440.0, SR, FFT_SIZE * 4);
    aa.processAudioBlock(samples.data(), FFT_SIZE * 4);
    CHECK(dominantPC(aa.getChromaVector()) == 9);
}

TEST_CASE("AudioAnalyzer — small block fed incrementally triggers FFT correctly", "[audio_analyzer]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    auto samples = sineWave(440.0, SR, FFT_SIZE);
    // Feed in 512-sample chunks (simulates real-time DAW callback)
    for (int offset = 0; offset < FFT_SIZE; offset += 512)
        aa.processAudioBlock(samples.data() + offset, 512);
    CHECK(dominantPC(aa.getChromaVector()) == 9);
}

// ─── Pitch detection integration (AudioAnalyzer ← YINDetector) ──────────────
// YIN window = 2048 samples, so we need at least that many samples fed before
// the first pitch result is available.

TEST_CASE("AudioAnalyzer — A4 sine → getDetectedPitch ≈ 440 Hz", "[audio_analyzer][pitch]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    // Feed 2 * YIN window (4096 samples) to guarantee at least one analysis hop
    auto samples = sineWave(440.0, SR, 4096);
    aa.processAudioBlock(samples.data(), 4096);
    CHECK(aa.getDetectedPitch() == Catch::Approx(440.0f).margin(2.0f));
}

TEST_CASE("AudioAnalyzer — C4 sine → getDetectedPitch ≈ 261.63 Hz", "[audio_analyzer][pitch]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    auto samples = sineWave(261.63, SR, 4096);
    aa.processAudioBlock(samples.data(), 4096);
    CHECK(aa.getDetectedPitch() == Catch::Approx(261.63f).margin(3.0f));
}

TEST_CASE("AudioAnalyzer — silence → getDetectedPitch == 0", "[audio_analyzer][pitch]")
{
    AudioAnalyzer aa(SR, FFT_ORDER);
    std::vector<float> silence(4096, 0.0f);
    aa.processAudioBlock(silence.data(), 4096);
    CHECK(aa.getDetectedPitch() == Catch::Approx(0.0f));
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    return Catch::Session().run(argc, argv);
}
