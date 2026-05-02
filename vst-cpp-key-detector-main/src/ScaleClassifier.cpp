#include "ScaleClassifier.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

static ScaleClassifier::ScaleProfile makeProfile(
    const std::string& name,
    std::initializer_list<int> intervals)
{
    ScaleClassifier::ScaleProfile p;
    p.name = name;
    p.weights.fill(0.0f);
    p.formula = std::vector<int>(intervals);
    for (int i : intervals)
        p.weights[i % 12] = 1.0f;

    // Precompute L2 norm for cosine similarity in rankAll
    float sq = 0.0f;
    for (float w : p.weights) sq += w * w;
    p.norm = std::sqrt(sq);

    return p;
}

const std::vector<ScaleClassifier::ScaleProfile> ScaleClassifier::PROFILES = {
    makeProfile("Major (Ionian)",   {0,2,4,5,7,9,11}),
    makeProfile("Natural Minor",    {0,2,3,5,7,8,10}),
    makeProfile("Dorian",           {0,2,3,5,7,9,10}),
    makeProfile("Phrygian",         {0,1,3,5,7,8,10}),
    makeProfile("Lydian",           {0,2,4,6,7,9,11}),
    makeProfile("Mixolydian",       {0,2,4,5,7,9,10}),
    makeProfile("Locrian",          {0,1,3,5,6,8,10}),
    makeProfile("Harmonic Minor",   {0,2,3,5,7,8,11}),
    makeProfile("Melodic Minor",    {0,2,3,5,7,9,11}),
    makeProfile("Major Pentatonic", {0,2,4,7,9}),
    makeProfile("Minor Pentatonic", {0,3,5,7,10}),
    makeProfile("Blues",            {0,3,5,6,7,10}),
    makeProfile("Whole Tone",       {0,2,4,6,8,10}),
    makeProfile("Diminished (HW)",  {0,1,3,4,6,7,9,10}),
    makeProfile("Lydian Dominant",  {0,2,4,6,7,9,10}),
};

ScaleResult ScaleClassifier::classify(
    const std::array<float, 12>& pitchProfile,
    int rootClass) const
{
    auto results = rankAll(pitchProfile, rootClass, 1);
    return results.empty() ? ScaleResult{} : results.front();
}

std::vector<ScaleResult> ScaleClassifier::rankAll(
    const std::array<float, 12>& pitchProfile,
    int rootClass,
    int topN) const
{
    // L2 norm of the input profile
    float normInput = 0.0f;
    for (float v : pitchProfile) normInput += v * v;
    normInput = std::sqrt(normInput);

    std::vector<ScaleResult> results;
    results.reserve(PROFILES.size());

    for (const auto& profile : PROFILES)
    {
        float dot = 0.0f;
        for (int i = 0; i < 12; ++i)
            dot += pitchProfile[i] * profile.weights[(i - rootClass + 12) % 12];

        // Cosine similarity: perfect match = 1.0, independent of template size
        float denom      = normInput * profile.norm;
        float similarity = (denom > 1e-6f) ? dot / denom : 0.0f;

        ScaleResult r;
        r.name       = profile.name;
        r.confidence = similarity;
        r.formula    = profile.formula;
        results.push_back(std::move(r));
    }

    std::sort(results.begin(), results.end(),
        [](const ScaleResult& a, const ScaleResult& b) {
            return a.confidence > b.confidence;
        });

    if (static_cast<int>(results.size()) > topN)
        results.resize(static_cast<size_t>(topN));

    return results;
}
