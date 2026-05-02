#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "MidiAnalyzer.h"
#include "KeyEstimator.h"
#include "ScaleClassifier.h"
#include "ProfileFuser.h"
#include "ProgressionSuggester.h"

// JSON used by ProgressionSuggester tests (matches data/markov_transitions.json)
static const char* E2E_JSON = R"({
    "major": {
        "I":   {"IV": 0.30, "V": 0.25, "vi": 0.22, "ii": 0.12, "iii": 0.07, "vii": 0.04},
        "ii":  {"V": 0.45, "IV": 0.25, "I": 0.15, "vii": 0.15},
        "iii": {"vi": 0.35, "IV": 0.25, "V": 0.25, "I": 0.15},
        "IV":  {"V": 0.40, "I": 0.30, "ii": 0.15, "vi": 0.15},
        "V":   {"I": 0.52, "vi": 0.25, "IV": 0.10, "ii": 0.08, "iii": 0.05},
        "vi":  {"ii": 0.30, "IV": 0.25, "V": 0.25, "I": 0.20},
        "vii": {"I": 0.70, "V": 0.30}
    },
    "minor": {
        "i":   {"iv": 0.30, "v": 0.25, "VI": 0.25, "III": 0.15, "VII": 0.05},
        "ii":  {"v": 0.45, "i": 0.25, "VII": 0.20, "VI": 0.10},
        "III": {"VII": 0.35, "VI": 0.35, "i": 0.20, "iv": 0.10},
        "iv":  {"v": 0.35, "i": 0.30, "VII": 0.20, "VI": 0.15},
        "v":   {"i": 0.55, "VI": 0.20, "VII": 0.15, "iv": 0.10},
        "VI":  {"III": 0.30, "iv": 0.25, "VII": 0.20, "v": 0.15, "i": 0.10},
        "VII": {"III": 0.35, "i": 0.30, "v": 0.20, "iv": 0.15}
    }
})";

static std::string g_jsonPath;

// ─── helpers ──────────────────────────────────────────────────────────────────

// Build a normalised PCP from {pitchClass, weight} pairs
static std::array<float, 12> makePCP(
    std::initializer_list<std::pair<int, float>> pairs)
{
    std::array<float, 12> pcp {};
    float sum = 0.0f;
    for (auto [pc, w] : pairs) { pcp[static_cast<size_t>(pc)] += w; sum += w; }
    if (sum > 1e-6f)
        for (auto& v : pcp) v /= sum;
    return pcp;
}

// Prepare a fresh MidiAnalyzer (44100 Hz / 512 samples per block)
static MidiAnalyzer freshMa()
{
    MidiAnalyzer ma;
    ma.prepare(44100.0, 512);
    return ma;
}

// Feed {midiNote, velocity} pairs to a MidiAnalyzer for numBlocks blocks.
// Velocities proportional to the Krumhansl-Kessler weights give a profile that
// closely matches the template, yielding reliable key-detection in tests.
static void feedMidi(MidiAnalyzer& ma,
                     std::initializer_list<std::pair<int, int>> notesVels,
                     int numBlocks = 300)
{
    juce::MidiBuffer buf;
    for (auto [note, vel] : notesVels)
        buf.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(vel)), 0);
    for (int i = 0; i < numBlocks; ++i)
        ma.processMidiBuffer(buf);
}

// Active-notes bitmap
static std::array<bool, 12> pitches(std::initializer_list<int> pcs)
{
    std::array<bool, 12> a {};
    for (int pc : pcs) a[static_cast<size_t>(pc)] = true;
    return a;
}

// ─── Section 1: Key detection via direct PCP ──────────────────────────────────
// Using Krumhansl-Kessler weights directly guarantees near-perfect correlation
// with the estimator's own templates — reliable ground truth for downstream tests.

TEST_CASE("E2E key — C major PCP → root=0 major, conf>0.8", "[e2e][key]")
{
    // C D E F G A B weighted by K-K major profile
    auto pcp = makePCP({{0,6.35f},{2,3.48f},{4,4.38f},{5,4.09f},
                         {7,5.19f},{9,3.66f},{11,2.88f}});
    auto kr = KeyEstimator{}.estimate(pcp);
    CHECK(kr.root == 0);
    CHECK(kr.mode == "major");
    CHECK(kr.confidence > 0.8f);
}

TEST_CASE("E2E key — G major PCP → root=7 major", "[e2e][key]")
{
    // G A B C D E F# weighted by K-K major profile (root shifted by 7)
    auto pcp = makePCP({{7,6.35f},{9,3.48f},{11,4.38f},{0,4.09f},
                         {2,5.19f},{4,3.66f},{6,2.88f}});
    auto kr = KeyEstimator{}.estimate(pcp);
    CHECK(kr.root == 7);
    CHECK(kr.mode == "major");
}

TEST_CASE("E2E key — D major PCP → root=2 major", "[e2e][key]")
{
    // D E F# G A B C# weighted by K-K major profile
    auto pcp = makePCP({{2,6.35f},{4,3.48f},{6,4.38f},{7,4.09f},
                         {9,5.19f},{11,3.66f},{1,2.88f}});
    auto kr = KeyEstimator{}.estimate(pcp);
    CHECK(kr.root == 2);
    CHECK(kr.mode == "major");
}

TEST_CASE("E2E key — A minor PCP → root=9 minor", "[e2e][key]")
{
    // A B C D E F G weighted by K-K minor profile (root shifted by 9)
    auto pcp = makePCP({{9,6.33f},{11,3.52f},{0,5.38f},{2,3.53f},
                         {4,4.75f},{5,3.98f},{7,3.34f}});
    auto kr = KeyEstimator{}.estimate(pcp);
    CHECK(kr.root == 9);
    CHECK(kr.mode == "minor");
    CHECK(kr.confidence > 0.8f);
}

TEST_CASE("E2E key — E minor PCP → root=4 minor", "[e2e][key]")
{
    // E F# G A B C D weighted by K-K minor profile (root shifted by 4)
    auto pcp = makePCP({{4,6.33f},{6,3.52f},{7,5.38f},{9,3.53f},
                         {11,4.75f},{0,3.98f},{2,3.34f}});
    auto kr = KeyEstimator{}.estimate(pcp);
    CHECK(kr.root == 4);
    CHECK(kr.mode == "minor");
}

// ─── Section 2: Key detection via MidiAnalyzer ───────────────────────────────
// Velocities match K-K weights so the accumulated histogram closely reproduces
// the template profile after enough blocks.

TEST_CASE("E2E MIDI — C major notes → root=0 major", "[e2e][midi]")
{
    auto ma = freshMa();
    // C D E F G A B; velocities ∝ K-K major weights (C=127, G=104, E=88, F=82, A=73, D=70, B=58)
    feedMidi(ma, {{60,127},{62,70},{64,88},{65,82},{67,104},{69,73},{71,58}});
    const auto pcp = ProfileFuser::fuse(ma.getNormalizedProfile(), {},
                                        ProfileFuser::Mode::MidiOnly);
    const auto kr  = KeyEstimator{}.estimate(pcp);
    CHECK(kr.root == 0);
    CHECK(kr.mode == "major");
}

TEST_CASE("E2E MIDI — A minor notes → root=9 minor", "[e2e][midi]")
{
    auto ma = freshMa();
    // A B C D E F G; velocities ∝ K-K minor weights (A=127, C=108, E=95, F=80, B=71, D=71, G=67)
    feedMidi(ma, {{69,127},{71,71},{60,108},{62,71},{64,95},{65,80},{67,67}});
    const auto pcp = ProfileFuser::fuse(ma.getNormalizedProfile(), {},
                                        ProfileFuser::Mode::MidiOnly);
    const auto kr  = KeyEstimator{}.estimate(pcp);
    CHECK(kr.root == 9);
    CHECK(kr.mode == "minor");
}

TEST_CASE("E2E MIDI — reset clears history, next key wins", "[e2e][midi]")
{
    auto ma = freshMa();
    // Establish C major
    feedMidi(ma, {{60,127},{62,70},{64,88},{65,82},{67,104},{69,73},{71,58}}, 400);
    // Reset and feed G major
    ma.reset();
    feedMidi(ma, {{67,127},{69,70},{71,88},{60,82},{62,104},{64,73},{66,58}}, 300);
    const auto pcp = ProfileFuser::fuse(ma.getNormalizedProfile(), {},
                                        ProfileFuser::Mode::MidiOnly);
    const auto kr  = KeyEstimator{}.estimate(pcp);
    CHECK(kr.root == 7);
    CHECK(kr.mode == "major");
}

// ─── Section 3: Key → Scale classification ───────────────────────────────────

TEST_CASE("E2E scale — C major PCP → 'Major (Ionian)'", "[e2e][scale]")
{
    auto pcp = makePCP({{0,6.35f},{2,3.48f},{4,4.38f},{5,4.09f},
                         {7,5.19f},{9,3.66f},{11,2.88f}});
    auto kr  = KeyEstimator{}.estimate(pcp);
    auto sc  = ScaleClassifier{}.classify(pcp, kr.root);
    REQUIRE(kr.root == 0);
    CHECK(sc.name == "Major (Ionian)");
    CHECK(!sc.formula.empty());
    CHECK(sc.confidence > 0.5f);
}

TEST_CASE("E2E scale — A minor PCP → 'Natural Minor'", "[e2e][scale]")
{
    auto pcp = makePCP({{9,6.33f},{11,3.52f},{0,5.38f},{2,3.53f},
                         {4,4.75f},{5,3.98f},{7,3.34f}});
    auto kr  = KeyEstimator{}.estimate(pcp);
    auto sc  = ScaleClassifier{}.classify(pcp, kr.root);
    REQUIRE(kr.root == 9);
    CHECK(sc.name == "Natural Minor");
}

TEST_CASE("E2E scale — formula covers correct pitch classes", "[e2e][scale]")
{
    // C major formula should include {0,2,4,5,7,9,11}
    auto pcp = makePCP({{0,6.35f},{2,3.48f},{4,4.38f},{5,4.09f},
                         {7,5.19f},{9,3.66f},{11,2.88f}});
    auto sc = ScaleClassifier{}.classify(pcp, 0);

    std::array<bool, 12> inScale {};
    for (int interval : sc.formula)
        inScale[(0 + interval + 12) % 12] = true;

    for (int pc : {0,2,4,5,7,9,11})  CHECK(inScale[static_cast<size_t>(pc)]);
    for (int pc : {1,3,6,8,10})      CHECK_FALSE(inScale[static_cast<size_t>(pc)]);
}

// ─── Section 4: ProfileFuser integration ─────────────────────────────────────

TEST_CASE("E2E fuser — MidiOnly returns MIDI profile unchanged", "[e2e][fuser]")
{
    std::array<float, 12> midi  { 0.20f,0,0.15f,0,0.10f,0.10f,0,0.15f,0,0.10f,0,0.10f };
    std::array<float, 12> audio {};
    const auto fused = ProfileFuser::fuse(midi, audio, ProfileFuser::Mode::MidiOnly);
    for (int i = 0; i < 12; ++i)
        CHECK(fused[i] == Catch::Approx(midi[i]));
}

TEST_CASE("E2E fuser — AudioOnly returns audio profile unchanged", "[e2e][fuser]")
{
    std::array<float, 12> midi {};
    std::array<float, 12> audio { 0.15f,0,0.10f,0,0.10f,0.15f,0,0.10f,0,0.10f,0,0.10f };
    const auto fused = ProfileFuser::fuse(midi, audio, ProfileFuser::Mode::AudioOnly);
    for (int i = 0; i < 12; ++i)
        CHECK(fused[i] == Catch::Approx(audio[i]));
}

TEST_CASE("E2E fuser — Blend output is L1-normalised and contains both sources", "[e2e][fuser]")
{
    std::array<float, 12> midi  {}; midi[0]  = 1.0f;  // only C
    std::array<float, 12> audio {}; audio[7] = 1.0f;  // only G
    const auto fused = ProfileFuser::fuse(midi, audio, ProfileFuser::Mode::Blend, 0.5f);

    CHECK(fused[0] > 0.0f);   // C present
    CHECK(fused[7] > 0.0f);   // G present

    float sum = 0.0f;
    for (float v : fused) sum += v;
    CHECK(sum == Catch::Approx(1.0f).margin(0.01f));
}

TEST_CASE("E2E fuser — Blend with midi=C, audio=C still detects C major", "[e2e][fuser]")
{
    auto audioPCP = makePCP({{0,6.35f},{2,3.48f},{4,4.38f},{5,4.09f},
                              {7,5.19f},{9,3.66f},{11,2.88f}});
    auto ma = freshMa();
    feedMidi(ma, {{60,127},{62,70},{64,88},{65,82},{67,104},{69,73},{71,58}});

    const auto fused = ProfileFuser::fuse(
        ma.getNormalizedProfile(), audioPCP, ProfileFuser::Mode::Blend, 0.5f);
    const auto kr = KeyEstimator{}.estimate(fused);
    CHECK(kr.root == 0);
    CHECK(kr.mode == "major");
}

// ─── Section 5: inferCurrentRoman ────────────────────────────────────────────

TEST_CASE("E2E roman — G+B+D in C major context → V", "[e2e][roman]")
{
    ProgressionSuggester ps(g_jsonPath);
    CHECK(ps.inferCurrentRoman(pitches({7,11,2}), 0, "major") == "V");
}

TEST_CASE("E2E roman — G+B+D in G major context → I", "[e2e][roman]")
{
    ProgressionSuggester ps(g_jsonPath);
    CHECK(ps.inferCurrentRoman(pitches({7,11,2}), 7, "major") == "I");
}

TEST_CASE("E2E roman — A+C+E in A minor context → i", "[e2e][roman]")
{
    ProgressionSuggester ps(g_jsonPath);
    CHECK(ps.inferCurrentRoman(pitches({9,0,4}), 9, "minor") == "i");
}

TEST_CASE("E2E roman — F+A+C in C major context → IV", "[e2e][roman]")
{
    ProgressionSuggester ps(g_jsonPath);
    CHECK(ps.inferCurrentRoman(pitches({5,9,0}), 0, "major") == "IV");
}

// ─── Section 6: Full progression chain ───────────────────────────────────────

TEST_CASE("E2E chain — MIDI C major → I → top-3 suggestions = IV, V, vi", "[e2e][chain]")
{
    auto ma = freshMa();
    feedMidi(ma, {{60,127},{62,70},{64,88},{65,82},{67,104},{69,73},{71,58}});

    const auto pcp   = ProfileFuser::fuse(ma.getNormalizedProfile(), {},
                                          ProfileFuser::Mode::MidiOnly);
    const auto kr    = KeyEstimator{}.estimate(pcp);
    const auto sc    = ScaleClassifier{}.classify(pcp, kr.root);

    REQUIRE(kr.root == 0);
    REQUIRE(kr.mode == "major");
    CHECK(sc.name == "Major (Ionian)");

    ProgressionSuggester ps(g_jsonPath);
    const auto suggs = ps.suggest("I", kr.mode, kr.root, 3);

    REQUIRE(suggs.size() == 3);
    CHECK(suggs[0].roman == "IV");
    CHECK(suggs[1].roman == "V");
    CHECK(suggs[2].roman == "vi");
    CHECK(suggs[0].name  == "F");
    CHECK(suggs[1].name  == "G");
    CHECK(suggs[2].name  == "Am");
}

TEST_CASE("E2E chain — V in C major → top suggestion is I=C at p≈0.52", "[e2e][chain]")
{
    auto ma = freshMa();
    feedMidi(ma, {{60,127},{62,70},{64,88},{65,82},{67,104},{69,73},{71,58}});

    const auto pcp = ProfileFuser::fuse(ma.getNormalizedProfile(), {},
                                        ProfileFuser::Mode::MidiOnly);
    const auto kr  = KeyEstimator{}.estimate(pcp);
    REQUIRE(kr.root == 0);

    ProgressionSuggester ps(g_jsonPath);
    const auto suggs = ps.suggest("V", "major", 0, 1);

    REQUIRE(suggs.size() == 1);
    CHECK(suggs[0].roman      == "I");
    CHECK(suggs[0].name       == "C");
    CHECK(suggs[0].probability == Catch::Approx(0.52f));
}

TEST_CASE("E2E chain — MIDI A minor → scale formula covers A minor pitch classes", "[e2e][chain]")
{
    auto ma = freshMa();
    feedMidi(ma, {{69,127},{71,71},{60,108},{62,71},{64,95},{65,80},{67,67}});

    const auto pcp = ProfileFuser::fuse(ma.getNormalizedProfile(), {},
                                        ProfileFuser::Mode::MidiOnly);
    const auto kr  = KeyEstimator{}.estimate(pcp);
    const auto sc  = ScaleClassifier{}.classify(pcp, kr.root);

    REQUIRE(kr.root == 9);
    REQUIRE(kr.mode == "minor");
    CHECK(sc.name == "Natural Minor");

    std::array<bool, 12> inScale {};
    for (int interval : sc.formula)
        inScale[(kr.root + interval + 12) % 12] = true;

    for (int pc : {9,11,0,2,4,5,7})  CHECK(inScale[static_cast<size_t>(pc)]);  // A B C D E F G
    for (int pc : {1,3,6,8,10})      CHECK_FALSE(inScale[static_cast<size_t>(pc)]);
}

TEST_CASE("E2E chain — detect chord in key → suggest next chord", "[e2e][chain]")
{
    // In G major, G+B+D is I; next chord suggestions should be IV=C, V=D, vi=Em
    ProgressionSuggester ps(g_jsonPath);

    const std::string roman = ps.inferCurrentRoman(pitches({7,11,2}), 7, "major");
    REQUIRE(roman == "I");

    const auto suggs = ps.suggest(roman, "major", 7, 3);
    REQUIRE(suggs.size() == 3);
    CHECK(suggs[0].roman == "IV");
    CHECK(suggs[0].name  == "C");   // IV of G major
    CHECK(suggs[1].roman == "V");
    CHECK(suggs[1].name  == "D");   // V  of G major
    CHECK(suggs[2].roman == "vi");
    CHECK(suggs[2].name  == "Em");  // vi of G major
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    juce::File tmpFile = juce::File::createTempFile(".json");
    tmpFile.replaceWithText(E2E_JSON);
    g_jsonPath = tmpFile.getFullPathName().toStdString();

    int result = Catch::Session().run(argc, argv);

    tmpFile.deleteFile();
    return result;
}
