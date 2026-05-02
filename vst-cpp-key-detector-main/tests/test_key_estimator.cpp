#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "KeyEstimator.h"

TEST_CASE("KeyEstimator — C major profile → root=0 major", "[key_estimator]")
{
    KeyEstimator est;
    std::array<float, 12> profile {};
    for (int pc : {0, 2, 4, 5, 7, 9, 11})
        profile[pc] = 1.0f;

    auto r = est.estimate(profile);
    CHECK(r.root == 0);
    CHECK(r.mode == "major");
    CHECK(r.confidence > 0.0f);
}

TEST_CASE("KeyEstimator — A natural minor profile → root=9 minor", "[key_estimator]")
{
    KeyEstimator est;
    std::array<float, 12> profile {};
    // Realistic tonic emphasis: A played most often, E as dominant
    profile[9]  = 3.0f;  // A — tonic
    profile[4]  = 2.0f;  // E — 5th
    profile[0]  = 1.0f;  // C — minor 3rd
    profile[2]  = 1.0f;  // D
    profile[5]  = 1.0f;  // F
    profile[7]  = 1.0f;  // G
    profile[11] = 1.0f;  // B

    auto r = est.estimate(profile);
    CHECK(r.root == 9);
    CHECK(r.mode == "minor");
}

TEST_CASE("KeyEstimator — empty profile returns valid (non-NaN) result", "[key_estimator]")
{
    KeyEstimator est;
    std::array<float, 12> empty {};
    auto r = est.estimate(empty);

    CHECK(r.confidence >= -1.0f);
    CHECK(r.confidence <= 1.0f);
    CHECK(!r.mode.empty());
}

TEST_CASE("KeyEstimator — confidence in [-1, 1] for any input", "[key_estimator]")
{
    KeyEstimator est;
    std::array<float, 12> profile {};
    profile[0] = 10.0f;
    profile[4] = 5.0f;
    profile[7] = 8.0f;

    auto r = est.estimate(profile);
    CHECK(r.confidence >= -1.0f);
    CHECK(r.confidence <=  1.0f);
}
