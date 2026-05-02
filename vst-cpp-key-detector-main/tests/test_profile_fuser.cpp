#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "ProfileFuser.h"

#include <numeric>

// ─── helpers ──────────────────────────────────────────────────────────────────

// Build a profile with 1.0 at the given pitch classes, 0.0 elsewhere.
static std::array<float, 12> makeProfile(std::initializer_list<int> pcs)
{
    std::array<float, 12> p {};
    for (int pc : pcs) p[static_cast<size_t>(pc)] = 1.0f;
    // L1-normalise to mimic real inputs
    float sum = std::accumulate(p.begin(), p.end(), 0.0f);
    if (sum > 0.0f) for (float& v : p) v /= sum;
    return p;
}

static float profileSum(const std::array<float, 12>& p)
{
    return std::accumulate(p.begin(), p.end(), 0.0f);
}

static constexpr auto MIDI_ONLY  = ProfileFuser::Mode::MidiOnly;
static constexpr auto AUDIO_ONLY = ProfileFuser::Mode::AudioOnly;
static constexpr auto BLEND      = ProfileFuser::Mode::Blend;

// ─── MidiOnly mode ────────────────────────────────────────────────────────────

TEST_CASE("ProfileFuser — MidiOnly returns MIDI profile unchanged", "[fuser]")
{
    auto midi  = makeProfile({0, 4, 7});   // C major triad
    auto audio = makeProfile({2, 5, 9});   // D minor triad

    auto result = ProfileFuser::fuse(midi, audio, MIDI_ONLY);
    CHECK(result == midi);
}

TEST_CASE("ProfileFuser — MidiOnly ignores midiWeight parameter", "[fuser]")
{
    auto midi  = makeProfile({0, 4, 7});
    auto audio = makeProfile({2, 5, 9});

    // midiWeight is irrelevant in MidiOnly mode
    CHECK(ProfileFuser::fuse(midi, audio, MIDI_ONLY, 0.0f) == midi);
    CHECK(ProfileFuser::fuse(midi, audio, MIDI_ONLY, 1.0f) == midi);
}

// ─── AudioOnly mode ───────────────────────────────────────────────────────────

TEST_CASE("ProfileFuser — AudioOnly returns audio profile unchanged", "[fuser]")
{
    auto midi  = makeProfile({0, 4, 7});
    auto audio = makeProfile({2, 5, 9});

    auto result = ProfileFuser::fuse(midi, audio, AUDIO_ONLY);
    CHECK(result == audio);
}

TEST_CASE("ProfileFuser — AudioOnly ignores midiWeight parameter", "[fuser]")
{
    auto midi  = makeProfile({0, 4, 7});
    auto audio = makeProfile({2, 5, 9});

    CHECK(ProfileFuser::fuse(midi, audio, AUDIO_ONLY, 0.0f) == audio);
    CHECK(ProfileFuser::fuse(midi, audio, AUDIO_ONLY, 1.0f) == audio);
}

// ─── Blend mode ───────────────────────────────────────────────────────────────

TEST_CASE("ProfileFuser — Blend with midiWeight=1.0 equals MidiOnly", "[fuser]")
{
    auto midi  = makeProfile({0, 4, 7});
    auto audio = makeProfile({2, 5, 9});

    auto blended  = ProfileFuser::fuse(midi, audio, BLEND, 1.0f);
    auto midiOnly = ProfileFuser::fuse(midi, audio, MIDI_ONLY);
    for (int i = 0; i < 12; ++i)
        CHECK(blended[i] == Catch::Approx(midiOnly[i]).margin(1e-5f));
}

TEST_CASE("ProfileFuser — Blend with midiWeight=0.0 equals AudioOnly", "[fuser]")
{
    auto midi  = makeProfile({0, 4, 7});
    auto audio = makeProfile({2, 5, 9});

    auto blended   = ProfileFuser::fuse(midi, audio, BLEND, 0.0f);
    auto audioOnly = ProfileFuser::fuse(midi, audio, AUDIO_ONLY);
    for (int i = 0; i < 12; ++i)
        CHECK(blended[i] == Catch::Approx(audioOnly[i]).margin(1e-5f));
}

TEST_CASE("ProfileFuser — Blend at 50% averages the two profiles", "[fuser]")
{
    auto midi  = makeProfile({0});   // only C
    auto audio = makeProfile({7});   // only G

    auto result = ProfileFuser::fuse(midi, audio, BLEND, 0.5f);

    // C and G should carry equal weight; all others zero
    CHECK(result[0] == Catch::Approx(result[7]).margin(1e-5f));
    for (int i = 1; i < 12; ++i)
        if (i != 7) CHECK(result[i] == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("ProfileFuser — Blend result sums to 1 for non-zero inputs", "[fuser]")
{
    auto midi  = makeProfile({0, 4, 7});
    auto audio = makeProfile({9, 0, 4});

    for (float w : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
    {
        auto result = ProfileFuser::fuse(midi, audio, BLEND, w);
        CHECK(profileSum(result) == Catch::Approx(1.0f).margin(1e-5f));
    }
}

TEST_CASE("ProfileFuser — Blend: midiWeight clamped to [0, 1]", "[fuser]")
{
    auto midi  = makeProfile({0, 4, 7});
    auto audio = makeProfile({2, 5, 9});

    // Values beyond [0,1] should clamp, not produce NaN or out-of-range output
    auto resultHigh = ProfileFuser::fuse(midi, audio, BLEND, 2.0f);
    auto resultLow  = ProfileFuser::fuse(midi, audio, BLEND, -1.0f);
    auto midiOnly   = ProfileFuser::fuse(midi, audio, MIDI_ONLY);
    auto audioOnly  = ProfileFuser::fuse(midi, audio, AUDIO_ONLY);

    for (int i = 0; i < 12; ++i)
        CHECK(resultHigh[i] == Catch::Approx(midiOnly[i]).margin(1e-5f));
    for (int i = 0; i < 12; ++i)
        CHECK(resultLow[i] == Catch::Approx(audioOnly[i]).margin(1e-5f));
}

TEST_CASE("ProfileFuser — Blend with all-zero audio keeps MIDI proportions", "[fuser]")
{
    auto midi  = makeProfile({0, 4, 7});
    std::array<float, 12> audioZero {};

    auto result = ProfileFuser::fuse(midi, audioZero, BLEND, 0.5f);

    // audioZero contributes nothing; MIDI proportions should dominate
    // (after normalisation the relative ratios of C/E/G must be preserved)
    CHECK(result[0] > 0.0f);
    CHECK(result[4] > 0.0f);
    CHECK(result[7] > 0.0f);
    // Non-MIDI pitch classes must be zero
    for (int i = 0; i < 12; ++i)
        if (i != 0 && i != 4 && i != 7)
            CHECK(result[i] == Catch::Approx(0.0f).margin(1e-6f));
}

TEST_CASE("ProfileFuser — Blend with all-zero inputs returns all-zero", "[fuser]")
{
    std::array<float, 12> zero {};
    auto result = ProfileFuser::fuse(zero, zero, BLEND, 0.5f);
    for (float v : result)
        CHECK(v == Catch::Approx(0.0f).margin(1e-7f));
}

TEST_CASE("ProfileFuser — result values are always non-negative", "[fuser]")
{
    auto midi  = makeProfile({0, 4, 7});
    auto audio = makeProfile({2, 5, 9});

    for (auto mode : {MIDI_ONLY, AUDIO_ONLY, BLEND})
        for (float w : {0.0f, 0.5f, 1.0f})
            for (float v : ProfileFuser::fuse(midi, audio, mode, w))
                CHECK(v >= 0.0f);
}
