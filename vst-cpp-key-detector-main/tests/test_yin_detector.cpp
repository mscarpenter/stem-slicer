#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "YINDetector.h"

#include <cmath>
#include <vector>

// ─── helpers ──────────────────────────────────────────────────────────────────

static constexpr double PI     = 3.14159265358979323846;
static constexpr double SR     = 44100.0;
static constexpr int    WIN    = 2048;

static std::vector<float> sineWave(double freqHz, int numSamples, double sr = SR)
{
    std::vector<float> buf(static_cast<size_t>(numSamples));
    for (int i = 0; i < numSamples; ++i)
        buf[static_cast<size_t>(i)] =
            static_cast<float>(std::sin(2.0 * PI * freqHz * i / sr));
    return buf;
}

// Feed samples in chunks of chunkSize; return last pitch estimate.
static float processChunked(YINDetector& yin,
                             const std::vector<float>& samples,
                             int chunkSize)
{
    float pitch = 0.0f;
    int total   = static_cast<int>(samples.size());
    for (int offset = 0; offset < total; offset += chunkSize)
    {
        int n = std::min(chunkSize, total - offset);
        pitch = yin.process(samples.data() + offset, n);
    }
    return pitch;
}

// ─── YINDetector unit tests ───────────────────────────────────────────────────

TEST_CASE("YINDetector — silence returns 0 Hz", "[yin]")
{
    YINDetector yin(SR, WIN);
    std::vector<float> silence(WIN, 0.0f);
    CHECK(yin.process(silence.data(), WIN) == Catch::Approx(0.0f));
}

TEST_CASE("YINDetector — no result before first full window", "[yin]")
{
    YINDetector yin(SR, WIN);
    // Only half a window: detector hasn't fired yet → returns 0
    auto samples = sineWave(440.0, WIN / 2);
    float pitch = yin.process(samples.data(), WIN / 2);
    CHECK(pitch == Catch::Approx(0.0f));
}

TEST_CASE("YINDetector — A4 (440 Hz) detected within ±2 Hz", "[yin]")
{
    YINDetector yin(SR, WIN);
    auto samples = sineWave(440.0, WIN);
    float pitch = yin.process(samples.data(), WIN);
    CHECK(pitch == Catch::Approx(440.0f).margin(2.0f));
}

TEST_CASE("YINDetector — A3 (220 Hz) detected within ±2 Hz", "[yin]")
{
    YINDetector yin(SR, WIN);
    auto samples = sineWave(220.0, WIN);
    float pitch = yin.process(samples.data(), WIN);
    CHECK(pitch == Catch::Approx(220.0f).margin(2.0f));
}

TEST_CASE("YINDetector — C4 (261.63 Hz) detected within ±3 Hz", "[yin]")
{
    YINDetector yin(SR, WIN);
    auto samples = sineWave(261.63, WIN);
    float pitch = yin.process(samples.data(), WIN);
    CHECK(pitch == Catch::Approx(261.63f).margin(3.0f));
}

TEST_CASE("YINDetector — E4 (329.63 Hz) detected within ±3 Hz", "[yin]")
{
    YINDetector yin(SR, WIN);
    auto samples = sineWave(329.63, WIN);
    float pitch = yin.process(samples.data(), WIN);
    CHECK(pitch == Catch::Approx(329.63f).margin(3.0f));
}

TEST_CASE("YINDetector — G4 (392 Hz) detected within ±3 Hz", "[yin]")
{
    YINDetector yin(SR, WIN);
    auto samples = sineWave(392.0, WIN);
    float pitch = yin.process(samples.data(), WIN);
    CHECK(pitch == Catch::Approx(392.0f).margin(3.0f));
}

TEST_CASE("YINDetector — pitch out of range (30 Hz) returns 0", "[yin]")
{
    YINDetector yin(SR, WIN);
    auto samples = sineWave(30.0, WIN);
    float pitch = yin.process(samples.data(), WIN);
    // 30 Hz is below minFrequency (50 Hz), so detector should not latch
    CHECK(pitch == Catch::Approx(0.0f));
}

TEST_CASE("YINDetector — incremental 512-sample chunks: A4 detected", "[yin]")
{
    YINDetector yin(SR, WIN);
    // Need at least WIN samples to trigger first analysis
    auto samples = sineWave(440.0, WIN * 2);
    float pitch = processChunked(yin, samples, 512);
    CHECK(pitch == Catch::Approx(440.0f).margin(2.0f));
}

TEST_CASE("YINDetector — multi-frame stability: A4 stays near 440 Hz", "[yin]")
{
    YINDetector yin(SR, WIN);
    // 4 full windows → 7 analysis hops (50% overlap)
    auto samples = sineWave(440.0, WIN * 4);
    float pitch = yin.process(samples.data(), WIN * 4);
    CHECK(pitch == Catch::Approx(440.0f).margin(2.0f));
}

TEST_CASE("YINDetector — empty block does not crash", "[yin]")
{
    YINDetector yin(SR, WIN);
    REQUIRE_NOTHROW(yin.process(nullptr, 0));
}
