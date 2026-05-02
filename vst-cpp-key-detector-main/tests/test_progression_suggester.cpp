#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <juce_core/juce_core.h>
#include "ProgressionSuggester.h"

// ─── test JSON content (matches data/markov_transitions.json) ─────────────────

static const char* TEST_JSON = R"({
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

// Shared temp-file path, created once in main() and cleaned up at exit.
static std::string g_jsonPath;

// ─── loading ──────────────────────────────────────────────────────────────────

TEST_CASE("ProgressionSuggester — missing file: no crash, empty results", "[progression]")
{
    REQUIRE_NOTHROW(ProgressionSuggester("/nonexistent/path_xyz.json"));
    ProgressionSuggester ps("/nonexistent/path_xyz.json");
    CHECK(ps.suggest("I", "major", 0, 3).empty());
}

TEST_CASE("ProgressionSuggester — loads valid JSON without error", "[progression]")
{
    REQUIRE_NOTHROW(ProgressionSuggester(g_jsonPath));
    ProgressionSuggester ps(g_jsonPath);
    CHECK(!ps.suggest("I", "major", 0, 1).empty());
}

// ─── suggest() — major mode ───────────────────────────────────────────────────

TEST_CASE("ProgressionSuggester — I major C → top-3 are IV, V, vi", "[progression][major]")
{
    ProgressionSuggester ps(g_jsonPath);
    auto results = ps.suggest("I", "major", 0, 3);

    REQUIRE(results.size() == 3);
    CHECK(results[0].roman == "IV");
    CHECK(results[1].roman == "V");
    CHECK(results[2].roman == "vi");

    CHECK(results[0].probability == Catch::Approx(0.30f));
    CHECK(results[1].probability == Catch::Approx(0.25f));
    CHECK(results[2].probability == Catch::Approx(0.22f));
}

TEST_CASE("ProgressionSuggester — I major C → chord names F, G, Am", "[progression][major]")
{
    ProgressionSuggester ps(g_jsonPath);
    auto results = ps.suggest("I", "major", 0, 3);

    REQUIRE(results.size() == 3);
    CHECK(results[0].name == "F");    // IV of C major
    CHECK(results[1].name == "G");    // V  of C major
    CHECK(results[2].name == "Am");   // vi of C major
}

TEST_CASE("ProgressionSuggester — V major G → top-1 is I with p≈0.52", "[progression][major]")
{
    ProgressionSuggester ps(g_jsonPath);
    auto results = ps.suggest("V", "major", 7, 1);  // root=7 → G major

    REQUIRE(results.size() == 1);
    CHECK(results[0].roman == "I");
    CHECK(results[0].probability == Catch::Approx(0.52f));
    CHECK(results[0].name == "G");   // I of G major
}

TEST_CASE("ProgressionSuggester — topN=0 returns empty", "[progression]")
{
    ProgressionSuggester ps(g_jsonPath);
    CHECK(ps.suggest("I", "major", 0, 0).empty());
}

TEST_CASE("ProgressionSuggester — unknown Roman returns empty", "[progression]")
{
    ProgressionSuggester ps(g_jsonPath);
    CHECK(ps.suggest("bVII", "major", 0, 3).empty());
}

TEST_CASE("ProgressionSuggester — unknown mode returns empty", "[progression]")
{
    ProgressionSuggester ps(g_jsonPath);
    CHECK(ps.suggest("I", "dorian", 0, 3).empty());
}

TEST_CASE("ProgressionSuggester — results sorted descending by probability", "[progression]")
{
    ProgressionSuggester ps(g_jsonPath);
    auto results = ps.suggest("I", "major", 0, 6);

    for (size_t i = 1; i < results.size(); ++i)
        CHECK(results[i - 1].probability >= results[i].probability);
}

TEST_CASE("ProgressionSuggester — topN larger than available: returns all", "[progression]")
{
    ProgressionSuggester ps(g_jsonPath);
    // "vii" only has 2 entries in the table
    auto results = ps.suggest("vii", "major", 0, 99);
    CHECK(results.size() == 2);
}

// ─── suggest() — minor mode ───────────────────────────────────────────────────

TEST_CASE("ProgressionSuggester — i minor A → top-1 is iv", "[progression][minor]")
{
    ProgressionSuggester ps(g_jsonPath);
    auto results = ps.suggest("i", "minor", 9, 1);  // root=9 → A minor

    REQUIRE(results.size() == 1);
    CHECK(results[0].roman == "iv");
    CHECK(results[0].probability == Catch::Approx(0.30f));
    CHECK(results[0].name == "Dm");  // iv of A minor
}

TEST_CASE("ProgressionSuggester — v minor A → top-1 is i with p=0.55", "[progression][minor]")
{
    ProgressionSuggester ps(g_jsonPath);
    auto results = ps.suggest("v", "minor", 9, 1);

    REQUIRE(results.size() == 1);
    CHECK(results[0].roman == "i");
    CHECK(results[0].probability == Catch::Approx(0.55f));
    CHECK(results[0].name == "Am");  // i of A minor
}

// ─── romanToChordName (tested indirectly via suggest.name) ────────────────────

TEST_CASE("ProgressionSuggester — chord names correct for D major (root=2)", "[progression][names]")
{
    ProgressionSuggester ps(g_jsonPath);
    // D major: I=D, IV=G, V=A, vi=Bm
    auto r1 = ps.suggest("I", "major", 2, 1);   // V comes second; let's ask for IV
    // Actually ask for all and check names
    auto all = ps.suggest("I", "major", 2, 6);
    REQUIRE(!all.empty());

    for (const auto& s : all)
    {
        if (s.roman == "IV")  CHECK(s.name == "G");    // D+5=G
        if (s.roman == "V")   CHECK(s.name == "A");    // D+7=A
        if (s.roman == "vi")  CHECK(s.name == "Bm");   // D+9=B
        if (s.roman == "ii")  CHECK(s.name == "Em");   // D+2=E
    }
    (void)r1;
}

TEST_CASE("ProgressionSuggester — probability values are in (0, 1]", "[progression]")
{
    ProgressionSuggester ps(g_jsonPath);
    for (const auto& roman : std::vector<std::string>{"I", "ii", "iii", "IV", "V", "vi", "vii"})
    {
        for (const auto& s : ps.suggest(roman, "major", 0, 10))
        {
            CHECK(s.probability > 0.0f);
            CHECK(s.probability <= 1.001f);
        }
    }
}

// ─── inferCurrentRoman ────────────────────────────────────────────────────────
// These tests use ProgressionSuggester("/nonexistent") so the _table is empty,
// but inferCurrentRoman() relies only on static chord tables — not _table.

static std::array<bool, 12> notes(std::initializer_list<int> pcs)
{
    std::array<bool, 12> a {};
    for (int pc : pcs) a[static_cast<size_t>(pc)] = true;
    return a;
}

TEST_CASE("inferCurrentRoman — no active notes returns '?'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    std::array<bool, 12> empty {};
    CHECK(ps.inferCurrentRoman(empty, 0, "major") == "?");
}

TEST_CASE("inferCurrentRoman — single note returns '?'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    CHECK(ps.inferCurrentRoman(notes({0}), 0, "major") == "?");
}

TEST_CASE("inferCurrentRoman — C+E+G (root=0 major) → 'I'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    CHECK(ps.inferCurrentRoman(notes({0, 4, 7}), 0, "major") == "I");
}

TEST_CASE("inferCurrentRoman — D+F+A (root=0 major) → 'ii'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    CHECK(ps.inferCurrentRoman(notes({2, 5, 9}), 0, "major") == "ii");
}

TEST_CASE("inferCurrentRoman — F+A+C (root=0 major) → 'IV'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    CHECK(ps.inferCurrentRoman(notes({5, 9, 0}), 0, "major") == "IV");
}

TEST_CASE("inferCurrentRoman — G+B+D (root=0 major) → 'V'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    CHECK(ps.inferCurrentRoman(notes({7, 11, 2}), 0, "major") == "V");
}

TEST_CASE("inferCurrentRoman — A+C+E (root=0 major) → 'vi'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    CHECK(ps.inferCurrentRoman(notes({9, 0, 4}), 0, "major") == "vi");
}

TEST_CASE("inferCurrentRoman — different root: G+B+D in G major (root=7) → 'I'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    // I of G major = G(7)+B(11)+D(2)
    CHECK(ps.inferCurrentRoman(notes({7, 11, 2}), 7, "major") == "I");
}

TEST_CASE("inferCurrentRoman — A+C+E in A minor (root=9) → 'i'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    // i of A minor = A(9)+C(0)+E(4)
    CHECK(ps.inferCurrentRoman(notes({9, 0, 4}), 9, "minor") == "i");
}

TEST_CASE("inferCurrentRoman — E+G+B in A minor (root=9) → 'v'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    // v of A minor = E(4)+G(7)+B(11) → offsets {7,10,2} from root=9: 4,7,11
    CHECK(ps.inferCurrentRoman(notes({4, 7, 11}), 9, "minor") == "v");
}

TEST_CASE("inferCurrentRoman — ambiguous dyad E+G (root=0 major) → '?'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    // E+G is shared equally by I (0,4,7) and iii (4,7,11): both score 2/3
    CHECK(ps.inferCurrentRoman(notes({4, 7}), 0, "major") == "?");
}

TEST_CASE("inferCurrentRoman — chromatic cluster (all 12 notes) → '?'", "[infer]")
{
    ProgressionSuggester ps("/nonexistent");
    std::array<bool, 12> all;
    all.fill(true);
    CHECK(ps.inferCurrentRoman(all, 0, "major") == "?");
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    // Write the test JSON to a temp file once for all test cases
    juce::File tmpFile = juce::File::createTempFile(".json");
    tmpFile.replaceWithText(TEST_JSON);
    g_jsonPath = tmpFile.getFullPathName().toStdString();

    int result = Catch::Session().run(argc, argv);

    tmpFile.deleteFile();
    return result;
}
