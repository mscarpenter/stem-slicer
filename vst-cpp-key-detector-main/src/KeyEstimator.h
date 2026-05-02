#pragma once
#include <array>
#include <string>

struct KeyResult {
    int root { 0 };         // 0–11 (C=0, C#=1, ..., B=11)
    std::string mode { "major" };  // "major" | "minor"
    float confidence { 0.0f };     // Pearson correlation (0.0–1.0)
};

class KeyEstimator {
public:
    KeyResult estimate(const std::array<float, 12>& pitchClassProfile) const;

private:
    // Krumhansl-Kessler perceptual profiles (1982)
    static constexpr std::array<float, 12> MAJOR_PROFILE = {
        6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
        2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
    };
    static constexpr std::array<float, 12> MINOR_PROFILE = {
        6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
        2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
    };

    float pearsonCorrelation(
        const std::array<float, 12>& x,
        const std::array<float, 12>& y,
        int shift
    ) const;
};
