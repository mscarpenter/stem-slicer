#include "KeyEstimator.h"
#include <cmath>

KeyResult KeyEstimator::estimate(const std::array<float, 12>& pcp) const
{
    KeyResult best { 0, "major", -2.0f };

    for (int root = 0; root < 12; ++root)
    {
        float corrMajor = pearsonCorrelation(pcp, MAJOR_PROFILE, root);
        float corrMinor = pearsonCorrelation(pcp, MINOR_PROFILE, root);

        if (corrMajor > best.confidence)
            best = { root, "major", corrMajor };
        if (corrMinor > best.confidence)
            best = { root, "minor", corrMinor };
    }
    return best;
}

float KeyEstimator::pearsonCorrelation(
    const std::array<float, 12>& x,
    const std::array<float, 12>& y,
    int shift) const
{
    float meanX = 0.0f, meanY = 0.0f;
    for (int i = 0; i < 12; ++i)
    {
        meanX += x[i];
        meanY += y[(i - shift + 12) % 12];
    }
    meanX /= 12.0f;
    meanY /= 12.0f;

    float num = 0.0f, denX = 0.0f, denY = 0.0f;
    for (int i = 0; i < 12; ++i)
    {
        float dx = x[i] - meanX;
        float dy = y[(i - shift + 12) % 12] - meanY;
        num  += dx * dy;
        denX += dx * dx;
        denY += dy * dy;
    }

    float denom = std::sqrt(denX * denY);
    return (denom > 1e-6f) ? num / denom : 0.0f;
}
