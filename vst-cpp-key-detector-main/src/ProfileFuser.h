#pragma once
#include <array>

// Combines MIDI and audio pitch-class profiles into a single PCP.
// All methods are stateless; call fuse() directly.
class ProfileFuser {
public:
    enum class Mode { MidiOnly, AudioOnly, Blend };

    // Returns a weighted mixture of midiPCP and audioPCP.
    //   midiWeight  — used only in Blend mode [0..1]; clamped internally.
    //                 0.0 = full audio, 1.0 = full MIDI, 0.5 = equal mix.
    //   MidiOnly / AudioOnly modes ignore midiWeight entirely.
    // The returned profile is L1-normalised (sums to 1) unless both inputs
    // are all-zero, in which case it is all-zero.
    static std::array<float, 12> fuse(
        const std::array<float, 12>& midiPCP,
        const std::array<float, 12>& audioPCP,
        Mode  mode,
        float midiWeight = 0.5f);
};
