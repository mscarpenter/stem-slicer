#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

struct ChordSuggestion {
    std::string roman {};        // "V7", "IV", "bVII"
    std::string name {};         // "G7", "F", "Bb" (computed from root)
    float probability { 0.0f };
};

class ProgressionSuggester {
public:
    explicit ProgressionSuggester(const std::string& jsonPath);

    std::vector<ChordSuggestion> suggest(
        const std::string& currentRoman,
        const std::string& mode,
        int rootClass,
        int topN = 3
    ) const;

    std::string inferCurrentRoman(
        const std::array<bool, 12>& activeNotes,
        int rootClass,
        const std::string& mode
    ) const;

private:
    // mode → currentRoman → nextRoman → probability
    std::map<std::string,
        std::map<std::string,
            std::map<std::string, float>>> _table;

    std::string romanToChordName(
        const std::string& roman,
        int rootClass,
        const std::string& mode
    ) const;
};
