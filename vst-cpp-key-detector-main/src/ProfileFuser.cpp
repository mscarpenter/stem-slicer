#include "ProfileFuser.h"
#include <algorithm>

std::array<float, 12> ProfileFuser::fuse(
    const std::array<float, 12>& midiPCP,
    const std::array<float, 12>& audioPCP,
    Mode  mode,
    float midiWeight)
{
    switch (mode)
    {
        case Mode::MidiOnly:  return midiPCP;
        case Mode::AudioOnly: return audioPCP;

        case Mode::Blend:
        {
            midiWeight = std::max(0.0f, std::min(1.0f, midiWeight));
            float audioWeight = 1.0f - midiWeight;

            std::array<float, 12> result {};
            float sum = 0.0f;
            for (int i = 0; i < 12; ++i)
            {
                result[i] = midiWeight * midiPCP[i] + audioWeight * audioPCP[i];
                sum += result[i];
            }
            // L1-normalise so the result behaves like the input profiles
            if (sum > 1e-6f)
                for (float& v : result) v /= sum;
            return result;
        }
    }
    return midiPCP;  // unreachable, silences compiler warning
}
