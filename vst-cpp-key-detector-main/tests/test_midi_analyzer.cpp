#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <juce_audio_basics/juce_audio_basics.h>
#include "MidiAnalyzer.h"
#include "KeyEstimator.h"
#include "ScaleClassifier.h"

// ─── helpers ────────────────────────────────────────────────────────────────

static juce::MidiBuffer makeNoteOn(int channel, int note, int velocity, int samplePos = 0)
{
    juce::MidiBuffer buf;
    buf.addEvent(juce::MidiMessage::noteOn(channel, note, (juce::uint8)velocity), samplePos);
    return buf;
}

static juce::MidiBuffer makeNoteOff(int channel, int note, int samplePos = 0)
{
    juce::MidiBuffer buf;
    buf.addEvent(juce::MidiMessage::noteOff(channel, note, (juce::uint8)0), samplePos);
    return buf;
}

// ─── MidiAnalyzer unit tests ─────────────────────────────────────────────────

TEST_CASE("MidiAnalyzer — note-on accumulates histogram", "[midi_analyzer]")
{
    MidiAnalyzer ma;
    ma.prepare(44100.0, 512);

    auto buf = makeNoteOn(1, 60, 100);  // C4 → pitch class 0
    ma.processMidiBuffer(buf);

    auto profile = ma.getNormalizedProfile();
    CHECK(profile[0] > 0.0f);
    CHECK(profile[1] == Catch::Approx(0.0f));
}

TEST_CASE("MidiAnalyzer — getActiveNotes reflects held notes", "[midi_analyzer]")
{
    MidiAnalyzer ma;
    ma.prepare(44100.0, 512);

    juce::MidiBuffer buf;
    buf.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);  // C4
    buf.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8)100), 1);  // E4
    ma.processMidiBuffer(buf);

    auto active = ma.getActiveNotes();
    CHECK(active[0] == true);   // C
    CHECK(active[4] == true);   // E
    CHECK(active[2] == false);  // D not active
}

TEST_CASE("MidiAnalyzer — note-off clears active note", "[midi_analyzer]")
{
    MidiAnalyzer ma;
    ma.prepare(44100.0, 512);

    ma.processMidiBuffer(makeNoteOn(1, 60, 100));
    CHECK(ma.getActiveNotes()[0] == true);

    ma.processMidiBuffer(makeNoteOff(1, 60));
    CHECK(ma.getActiveNotes()[0] == false);
}

TEST_CASE("MidiAnalyzer — note-off does not clear histogram", "[midi_analyzer]")
{
    MidiAnalyzer ma;
    ma.prepare(44100.0, 512);

    ma.processMidiBuffer(makeNoteOn(1, 60, 100));
    float before = ma.getNormalizedProfile()[0];
    CHECK(before > 0.0f);

    ma.processMidiBuffer(makeNoteOff(1, 60));
    // histogram keeps accumulation even after note-off
    CHECK(ma.getNormalizedProfile()[0] > 0.0f);
}

TEST_CASE("MidiAnalyzer — reset clears histogram and active notes", "[midi_analyzer]")
{
    MidiAnalyzer ma;
    ma.prepare(44100.0, 512);

    ma.processMidiBuffer(makeNoteOn(1, 60, 100));
    ma.reset();

    auto profile = ma.getNormalizedProfile();
    for (float v : profile) CHECK(v == Catch::Approx(0.0f));
    for (bool b : ma.getActiveNotes()) CHECK(b == false);
}

TEST_CASE("MidiAnalyzer — empty buffer does not crash", "[midi_analyzer]")
{
    MidiAnalyzer ma;
    ma.prepare(44100.0, 512);

    juce::MidiBuffer empty;
    REQUIRE_NOTHROW(ma.processMidiBuffer(empty));
}

TEST_CASE("MidiAnalyzer — velocity is weighted in histogram", "[midi_analyzer]")
{
    MidiAnalyzer ma;
    ma.prepare(44100.0, 512);

    // Same pitch class (C), two octaves: velocity 127 then 64
    ma.processMidiBuffer(makeNoteOn(1, 60, 127));  // C4, full velocity
    float highVelWeight = ma.getNormalizedProfile()[0];

    ma.reset();
    ma.processMidiBuffer(makeNoteOn(1, 60, 64));   // C4, half velocity
    float lowVelWeight = ma.getNormalizedProfile()[0];

    // Both are the only pitch class, so normalized to 1.0 — same ratio
    // Test absolute histogram values instead via multiple pitch classes
    ma.reset();
    juce::MidiBuffer buf;
    buf.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)127), 0);  // C, vel=127
    buf.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8)64),  1);  // D, vel=64
    ma.processMidiBuffer(buf);

    auto profile = ma.getNormalizedProfile();
    CHECK(profile[0] > profile[2]);  // C should have more weight than D
    (void)highVelWeight; (void)lowVelWeight;
}

TEST_CASE("MidiAnalyzer — same pitch class across octaves accumulates correctly", "[midi_analyzer]")
{
    MidiAnalyzer ma;
    ma.prepare(44100.0, 512);

    // C4=60, C5=72, C6=84 — all pitch class 0
    juce::MidiBuffer buf;
    buf.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
    buf.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8)100), 1);
    buf.addEvent(juce::MidiMessage::noteOn(1, 84, (juce::uint8)100), 2);
    ma.processMidiBuffer(buf);

    CHECK(ma.getNormalizedProfile()[0] == Catch::Approx(1.0f));  // 100% C
}

// ─── Integration: MidiAnalyzer → KeyEstimator ────────────────────────────────

TEST_CASE("Integration — C major scale with C emphasized → C major", "[midi_analyzer][integration]")
{
    MidiAnalyzer ma;
    KeyEstimator ke;
    ma.prepare(44100.0, 512);

    // C(60) played 3x as tonic, scale tones once each
    juce::MidiBuffer buf;
    int t = 0;
    for (int i = 0; i < 3; ++i)
        buf.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), t++);  // C
    for (int note : {62, 64, 65, 67, 69, 71})                                    // D E F G A B
        buf.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)80), t++);

    ma.processMidiBuffer(buf);
    auto r = ke.estimate(ma.getNormalizedProfile());

    CHECK(r.root == 0);
    CHECK(r.mode == "major");
    CHECK(r.confidence > 0.5f);
}

TEST_CASE("Integration — A minor with A emphasized → A minor", "[midi_analyzer][integration]")
{
    MidiAnalyzer ma;
    KeyEstimator ke;
    ma.prepare(44100.0, 512);

    juce::MidiBuffer buf;
    int t = 0;
    for (int i = 0; i < 3; ++i)
        buf.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), t++);  // A (x3)
    for (int i = 0; i < 2; ++i)
        buf.addEvent(juce::MidiMessage::noteOn(1, 76, (juce::uint8)90),  t++);  // E (x2, dominant)
    for (int note : {71, 72, 74, 77, 79})                                         // B C D F G
        buf.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)70), t++);

    ma.processMidiBuffer(buf);
    auto r = ke.estimate(ma.getNormalizedProfile());

    CHECK(r.root == 9);
    CHECK(r.mode == "minor");
}

TEST_CASE("Integration — reset returns to neutral (no key bias)", "[midi_analyzer][integration]")
{
    MidiAnalyzer ma;
    KeyEstimator ke;
    ma.prepare(44100.0, 512);

    // Play C major strongly, then reset
    juce::MidiBuffer buf;
    for (int note : {60, 62, 64, 65, 67, 69, 71})
        buf.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)100), 0);
    ma.processMidiBuffer(buf);

    ma.reset();

    // After reset, profile is all zeros — confidence should be ~0
    auto r = ke.estimate(ma.getNormalizedProfile());
    CHECK(r.confidence == Catch::Approx(0.0f).margin(0.01f));
}

// ─── Pipeline: MidiAnalyzer → KeyEstimator → ScaleClassifier ─────────────────

TEST_CASE("Pipeline — G Mixolydian MIDI → root=7, Mixolydian mode", "[pipeline]")
{
    MidiAnalyzer    ma;
    KeyEstimator    ke;
    ScaleClassifier sc;
    ma.prepare(44100.0, 512);

    // G(67) as tonic x3, then A B C D E F (Mixolydian — F natural, not F#)
    juce::MidiBuffer buf;
    int t = 0;
    for (int i = 0; i < 3; ++i)
        buf.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8)100), t++);  // G4
    for (int note : {69, 71, 72, 74, 76, 77})                                    // A B C D E F
        buf.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)80), t++);

    ma.processMidiBuffer(buf);
    auto pcp   = ma.getNormalizedProfile();
    auto key   = ke.estimate(pcp);
    auto scale = sc.classify(pcp, key.root);

    CHECK(key.root == 7);  // G
    CHECK(scale.name == "Mixolydian");
}

TEST_CASE("Pipeline — D Dorian MIDI → root=2, Dorian mode", "[pipeline]")
{
    MidiAnalyzer    ma;
    KeyEstimator    ke;
    ScaleClassifier sc;
    ma.prepare(44100.0, 512);

    // D(62) as tonic x3, then E F G A B C (Dorian — B natural, not Bb)
    juce::MidiBuffer buf;
    int t = 0;
    for (int i = 0; i < 3; ++i)
        buf.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8)100), t++);  // D4
    for (int note : {64, 65, 67, 69, 71, 72})                                    // E F G A B C
        buf.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)80), t++);

    ma.processMidiBuffer(buf);
    auto pcp   = ma.getNormalizedProfile();
    auto key   = ke.estimate(pcp);
    auto scale = sc.classify(pcp, key.root);

    CHECK(key.root == 2);  // D
    CHECK(scale.name == "Dorian");
}

TEST_CASE("Pipeline — rankAll top-3 are sorted and within [0,1]", "[pipeline]")
{
    MidiAnalyzer    ma;
    KeyEstimator    ke;
    ScaleClassifier sc;
    ma.prepare(44100.0, 512);

    juce::MidiBuffer buf;
    int t = 0;
    for (int i = 0; i < 3; ++i)
        buf.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), t++);
    for (int note : {62, 64, 65, 67, 69, 71})
        buf.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)80), t++);

    ma.processMidiBuffer(buf);
    auto pcp     = ma.getNormalizedProfile();
    auto key     = ke.estimate(pcp);
    auto top3    = sc.rankAll(pcp, key.root, 3);

    REQUIRE(top3.size() == 3);
    for (const auto& r : top3)
    {
        CHECK(r.confidence >= 0.0f);
        CHECK(r.confidence <= 1.001f);
        CHECK(!r.name.empty());
    }
    CHECK(top3[0].confidence >= top3[1].confidence);
    CHECK(top3[1].confidence >= top3[2].confidence);
}

// ─── Entry point ─────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    return Catch::Session().run(argc, argv);
}
