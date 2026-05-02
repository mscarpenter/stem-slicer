#pragma once
#include <array>
#include <string>
#include <vector>

struct ScaleResult {
    std::string name { "Unknown" };
    float confidence { 0.0f };
    std::vector<int> formula {};  // intervals in semitones from root
};

class ScaleClassifier {
public:
    ScaleResult classify(
        const std::array<float, 12>& pitchProfile,
        int rootClass
    ) const;

    std::vector<ScaleResult> rankAll(
        const std::array<float, 12>& pitchProfile,
        int rootClass,
        int topN = 3
    ) const;

    struct ScaleProfile {
        std::string name;
        std::array<float, 12> weights;  // 1.0 = scale degree, 0.0 = out-of-scale
        std::vector<int> formula;
        float norm { 0.0f };            // precomputed L2 norm for cosine similarity
    };

private:
    static const std::vector<ScaleProfile> PROFILES;  // 15 built-in modes
};
