#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "ScaleClassifier.h"

// Build a pitch-class profile from intervals relative to a root.
// The first interval (tonic) gets tonicWeight; all others get 1.0.
static std::array<float, 12> buildProfile(
    int root,
    std::initializer_list<int> intervals,
    float tonicWeight = 2.0f)
{
    std::array<float, 12> p {};
    bool first = true;
    for (int interval : intervals)
    {
        p[(root + interval) % 12] = first ? tonicWeight : 1.0f;
        first = false;
    }
    return p;
}

// ─── Basic API tests ──────────────────────────────────────────────────────────

TEST_CASE("ScaleClassifier — C major profile → top result is Major (Ionian)", "[scale_classifier]")
{
    ScaleClassifier clf;
    auto profile = buildProfile(0, {0,2,4,5,7,9,11});
    auto r = clf.classify(profile, 0);
    CHECK(r.name == "Major (Ionian)");
    CHECK(r.confidence > 0.0f);
    CHECK(r.confidence <= 1.0f);
}

TEST_CASE("ScaleClassifier — cosine similarity: perfect match scores 1.0", "[scale_classifier]")
{
    ScaleClassifier clf;
    // C major with equal weights (no tonic emphasis) — still a perfect match
    std::array<float, 12> profile {};
    for (int pc : {0, 2, 4, 5, 7, 9, 11}) profile[pc] = 1.0f;
    auto r = clf.classify(profile, 0);
    CHECK(r.name == "Major (Ionian)");
    CHECK(r.confidence == Catch::Approx(1.0f).margin(0.001f));
}

TEST_CASE("ScaleClassifier — rankAll returns at most topN entries", "[scale_classifier]")
{
    ScaleClassifier clf;
    std::array<float, 12> profile {};
    CHECK(clf.rankAll(profile, 0, 3).size() <= 3);
    CHECK(clf.rankAll(profile, 0, 1).size() <= 1);
}

TEST_CASE("ScaleClassifier — rankAll with topN=0 returns empty", "[scale_classifier]")
{
    ScaleClassifier clf;
    std::array<float, 12> profile {};
    CHECK(clf.rankAll(profile, 0, 0).empty());
}

TEST_CASE("ScaleClassifier — results are sorted descending by confidence", "[scale_classifier]")
{
    ScaleClassifier clf;
    auto profile = buildProfile(0, {0,2,4,5,7,9,11});
    auto results = clf.rankAll(profile, 0, 5);
    for (size_t i = 1; i < results.size(); ++i)
        CHECK(results[i - 1].confidence >= results[i].confidence);
}

TEST_CASE("ScaleClassifier — formula is non-empty for every built-in profile", "[scale_classifier]")
{
    ScaleClassifier clf;
    std::array<float, 12> profile {};
    for (const auto& r : clf.rankAll(profile, 0, 15))
        CHECK(!r.formula.empty());
}

// ─── Mode detection tests ─────────────────────────────────────────────────────

TEST_CASE("ScaleClassifier — D Dorian → Dorian at top-1", "[scale_classifier][modes]")
{
    ScaleClassifier clf;
    // D Dorian: D E F G A B C  (raised 6th vs natural minor)
    auto profile = buildProfile(2, {0,2,3,5,7,9,10});
    auto r = clf.classify(profile, 2);
    CHECK(r.name == "Dorian");
}

TEST_CASE("ScaleClassifier — G Mixolydian → Mixolydian at top-1", "[scale_classifier][modes]")
{
    ScaleClassifier clf;
    // G Mixolydian: G A B C D E F  (flat 7th vs major)
    auto profile = buildProfile(7, {0,2,4,5,7,9,10});
    auto r = clf.classify(profile, 7);
    CHECK(r.name == "Mixolydian");
}

TEST_CASE("ScaleClassifier — A Blues → Blues at top-1", "[scale_classifier][modes]")
{
    ScaleClassifier clf;
    // A Blues: A C D Eb E G
    auto profile = buildProfile(9, {0,3,5,6,7,10});
    auto r = clf.classify(profile, 9);
    CHECK(r.name == "Blues");
}

TEST_CASE("ScaleClassifier — E Phrygian → Phrygian at top-1", "[scale_classifier][modes]")
{
    ScaleClassifier clf;
    // E Phrygian: E F G A B C D  (flat 2nd)
    auto profile = buildProfile(4, {0,1,3,5,7,8,10});
    auto r = clf.classify(profile, 4);
    CHECK(r.name == "Phrygian");
}

TEST_CASE("ScaleClassifier — F Lydian → Lydian at top-1", "[scale_classifier][modes]")
{
    ScaleClassifier clf;
    // F Lydian: F G A B C D E  (raised 4th)
    auto profile = buildProfile(5, {0,2,4,6,7,9,11});
    auto r = clf.classify(profile, 5);
    CHECK(r.name == "Lydian");
}

TEST_CASE("ScaleClassifier — B Locrian → Locrian at top-1", "[scale_classifier][modes]")
{
    ScaleClassifier clf;
    // B Locrian: B C D E F G A  (flat 2nd and 5th)
    auto profile = buildProfile(11, {0,1,3,5,6,8,10});
    auto r = clf.classify(profile, 11);
    CHECK(r.name == "Locrian");
}

TEST_CASE("ScaleClassifier — A Harmonic Minor → Harmonic Minor at top-1", "[scale_classifier][modes]")
{
    ScaleClassifier clf;
    // A Harmonic Minor: A B C D E F G#  (raised 7th)
    auto profile = buildProfile(9, {0,2,3,5,7,8,11});
    auto r = clf.classify(profile, 9);
    CHECK(r.name == "Harmonic Minor");
}

TEST_CASE("ScaleClassifier — C Major Pentatonic → Major Pentatonic at top-1", "[scale_classifier][modes]")
{
    ScaleClassifier clf;
    // C Major Pentatonic: C D E G A  (no 4th or 7th)
    std::array<float, 12> profile {};
    for (int pc : {0, 2, 4, 7, 9}) profile[pc] = 1.0f;  // equal weights
    auto r = clf.classify(profile, 0);
    CHECK(r.name == "Major Pentatonic");
}

TEST_CASE("ScaleClassifier — A Minor Pentatonic (only) → Minor Pentatonic beats Natural Minor", "[scale_classifier][modes]")
{
    ScaleClassifier clf;
    // Only the 5 pentatonic notes — cosine similarity should favour the smaller template
    std::array<float, 12> profile {};
    for (int pc : {9, 0, 2, 4, 7}) profile[pc] = 1.0f;  // A C D E G
    auto r = clf.classify(profile, 9);
    CHECK(r.name == "Minor Pentatonic");
    // Confidence must be 1.0 (perfect match to 5-note template)
    CHECK(r.confidence == Catch::Approx(1.0f).margin(0.001f));
}

TEST_CASE("ScaleClassifier — Whole Tone → Whole Tone at top-1", "[scale_classifier][modes]")
{
    ScaleClassifier clf;
    auto profile = buildProfile(0, {0,2,4,6,8,10});
    auto r = clf.classify(profile, 0);
    CHECK(r.name == "Whole Tone");
}

TEST_CASE("ScaleClassifier — confidence in [0, 1] for any input", "[scale_classifier]")
{
    ScaleClassifier clf;
    auto profile = buildProfile(3, {0,2,4,5,7,9,11});
    for (const auto& r : clf.rankAll(profile, 3, 15))
    {
        CHECK(r.confidence >= 0.0f);
        CHECK(r.confidence <= 1.001f);  // small margin for float rounding
    }
}

TEST_CASE("ScaleClassifier — all 15 profiles are present in rankAll", "[scale_classifier]")
{
    ScaleClassifier clf;
    std::array<float, 12> profile {};
    profile[0] = 1.0f;  // non-zero so norms work
    auto results = clf.rankAll(profile, 0, 15);
    CHECK(results.size() == 15);
}
