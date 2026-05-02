#include "ProgressionSuggester.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>

// ─── static lookup tables ─────────────────────────────────────────────────────

namespace {

struct RomanInfo { int semitones; const char* suffix; };

// semitone offset from key root + chord-quality suffix per Roman numeral
const std::map<std::string, RomanInfo> MAJOR_ROMAN = {
    {"I",   {0,  ""}},
    {"ii",  {2,  "m"}},
    {"iii", {4,  "m"}},
    {"IV",  {5,  ""}},
    {"V",   {7,  ""}},
    {"vi",  {9,  "m"}},
    {"vii", {11, "dim"}},
};

const std::map<std::string, RomanInfo> MINOR_ROMAN = {
    {"i",   {0,  "m"}},
    {"ii",  {2,  "dim"}},
    {"III", {3,  ""}},
    {"iv",  {5,  "m"}},
    {"v",   {7,  "m"}},
    {"VI",  {8,  ""}},
    {"VII", {10, ""}},
};

const std::array<const char*, 12> NOTE_NAMES =
    {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Chord definition tables for inferCurrentRoman().
// Each entry: { romanNumeral, { semitone offsets from the key root } }
// Offsets are already mod-12; apply (root + offset) % 12 to get pitch class.
using ChordDef = std::pair<std::string, std::vector<int>>;

const std::vector<ChordDef> MAJOR_CHORDS = {
    {"I",   {0, 4, 7}},
    {"ii",  {2, 5, 9}},
    {"iii", {4, 7, 11}},
    {"IV",  {5, 9, 0}},
    {"V",   {7, 11, 2}},
    {"vi",  {9, 0, 4}},
    {"vii", {11, 2, 5}},
};

const std::vector<ChordDef> MINOR_CHORDS = {
    {"i",   {0, 3, 7}},
    {"ii",  {2, 5, 8}},
    {"III", {3, 7, 10}},
    {"iv",  {5, 8, 0}},
    {"v",   {7, 10, 2}},
    {"VI",  {8, 0, 3}},
    {"VII", {10, 2, 5}},
};

} // namespace

// ─── construction ─────────────────────────────────────────────────────────────

ProgressionSuggester::ProgressionSuggester(const std::string& jsonPath)
{
    auto file = juce::File(juce::String(jsonPath));
    if (!file.existsAsFile())
    {
        juce::Logger::writeToLog("ProgressionSuggester: file not found: "
                                 + juce::String(jsonPath));
        return;
    }

    juce::String content = file.loadFileAsString();
    juce::var   parsed   = juce::JSON::parse(content);

    if (parsed.isVoid())
    {
        juce::Logger::writeToLog("ProgressionSuggester: JSON parse error in: "
                                 + juce::String(jsonPath));
        return;
    }

    auto* root = parsed.getDynamicObject();
    if (!root) return;

    // Iterate mode-level keys ("major", "minor", …)
    for (int mi = 0; mi < root->getProperties().size(); ++mi)
    {
        std::string modeName =
            root->getProperties().getName(mi).toString().toStdString();
        const juce::var& modeVar = root->getProperties().getValueAt(mi);

        auto* modeObj = modeVar.getDynamicObject();
        if (!modeObj) continue;

        // Iterate current-Roman keys ("I", "V", …)
        for (int ri = 0; ri < modeObj->getProperties().size(); ++ri)
        {
            std::string romanName =
                modeObj->getProperties().getName(ri).toString().toStdString();
            const juce::var& romanVar = modeObj->getProperties().getValueAt(ri);

            auto* romanObj = romanVar.getDynamicObject();
            if (!romanObj) continue;

            // Iterate next-Roman keys ("IV", "I", …) → probability
            for (int ni = 0; ni < romanObj->getProperties().size(); ++ni)
            {
                std::string nextRoman =
                    romanObj->getProperties().getName(ni).toString().toStdString();
                float prob = static_cast<float>(
                    static_cast<double>(romanObj->getProperties().getValueAt(ni)));

                _table[modeName][romanName][nextRoman] = prob;
            }
        }
    }
}

// ─── suggest ──────────────────────────────────────────────────────────────────

std::vector<ChordSuggestion> ProgressionSuggester::suggest(
    const std::string& currentRoman,
    const std::string& mode,
    int rootClass,
    int topN) const
{
    if (topN <= 0) return {};

    auto modeIt = _table.find(mode);
    if (modeIt == _table.end()) return {};

    auto romanIt = modeIt->second.find(currentRoman);
    if (romanIt == modeIt->second.end()) return {};

    std::vector<ChordSuggestion> results;
    results.reserve(romanIt->second.size());
    for (const auto& [nextRoman, prob] : romanIt->second)
    {
        ChordSuggestion cs;
        cs.roman       = nextRoman;
        cs.probability = prob;
        cs.name        = romanToChordName(nextRoman, rootClass, mode);
        results.push_back(std::move(cs));
    }

    std::sort(results.begin(), results.end(),
        [](const ChordSuggestion& a, const ChordSuggestion& b) {
            return a.probability > b.probability;
        });

    if (static_cast<int>(results.size()) > topN)
        results.resize(static_cast<size_t>(topN));

    return results;
}

// ─── inferCurrentRoman ────────────────────────────────────────────────────────
//
// Identifies which diatonic chord degree is currently being played by
// comparing the active pitch-class set against each triad in the key.
//
// Scoring: Jaccard similarity = |active ∩ chord| / |active ∪ chord|.
// A chord must share at least 2 notes with the active set.
// Returns "?" when no chord clears the threshold (0.49) or when two chords
// tie at the top score (ambiguous voicing).

std::string ProgressionSuggester::inferCurrentRoman(
    const std::array<bool, 12>& activeNotes,
    int rootClass,
    const std::string& mode) const
{
    // Count active pitch classes
    int activeCount = 0;
    for (bool b : activeNotes) if (b) ++activeCount;
    if (activeCount == 0) return "?";

    const auto& chords = (mode == "minor") ? MINOR_CHORDS : MAJOR_CHORDS;

    std::string bestRoman = "?";
    float       bestScore = 0.49f;   // minimum Jaccard to consider a match
    int         topCount  = 0;       // how many chords share the top score

    for (const auto& [roman, offsets] : chords)
    {
        // Count how many chord tones are present in the active set
        int matches = 0;
        for (int off : offsets)
        {
            int pc = (rootClass + off + 12) % 12;
            if (activeNotes[static_cast<size_t>(pc)]) ++matches;
        }
        if (matches < 2) continue;   // require at least a dyad overlap

        int   chordSize = static_cast<int>(offsets.size());
        int   unionSize = activeCount + chordSize - matches;
        float score     = static_cast<float>(matches) / static_cast<float>(unionSize);

        if (score > bestScore + 1e-4f)
        {
            bestScore = score;
            bestRoman = roman;
            topCount  = 1;
        }
        else if (score > 0.49f && score >= bestScore - 1e-4f)
        {
            ++topCount;   // another chord matches equally well → ambiguous
        }
    }

    // Return the unique best match; "?" if tied or nothing qualified
    return (topCount == 1) ? bestRoman : "?";
}

// ─── romanToChordName ─────────────────────────────────────────────────────────

std::string ProgressionSuggester::romanToChordName(
    const std::string& roman,
    int rootClass,
    const std::string& mode) const
{
    const auto* lookup = (mode == "minor") ? &MINOR_ROMAN : &MAJOR_ROMAN;
    auto it = lookup->find(roman);
    if (it == lookup->end())
        return roman;  // unknown Roman: return as-is

    int notePC = (rootClass + it->second.semitones) % 12;
    return std::string(NOTE_NAMES[static_cast<size_t>(notePC)])
         + it->second.suffix;
}
