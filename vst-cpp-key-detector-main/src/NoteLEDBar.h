#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

class NoteLEDBar : public juce::Component
{
public:
    NoteLEDBar() = default;

    // Call from the UI thread to update state; triggers repaint()
    void setState(const std::array<bool, 12>& active,
                  const std::array<bool, 12>& inScale);

    void paint(juce::Graphics& g) override;

private:
    std::array<bool, 12> _active  {};
    std::array<bool, 12> _inScale {};

    static constexpr std::array<const char*, 12> NOTE_NAMES {
        "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteLEDBar)
};
