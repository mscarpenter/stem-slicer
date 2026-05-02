#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <string>

class CircleOfFifths : public juce::Component
{
public:
    CircleOfFifths() = default;

    void setState(int rootClass, const std::string& mode);
    void paint(juce::Graphics& g) override;

private:
    int         _root { 0 };
    std::string _mode { "major" };

    // CoF clockwise order starting at 12-o'clock (C major / Am)
    // Each entry is the pitch class for that wheel position.
    inline static constexpr std::array<int, 12> MAJOR_ORDER { 0,7,2,9,4,11,6,1,8,3,10,5 };
    inline static constexpr std::array<int, 12> MINOR_ORDER { 9,4,11,6,1,8,3,10,5,0,7,2 };

    static const std::array<const char*, 12> MAJOR_NAMES;
    static const std::array<const char*, 12> MINOR_NAMES;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircleOfFifths)
};
