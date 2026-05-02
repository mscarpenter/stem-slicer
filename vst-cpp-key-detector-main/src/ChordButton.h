#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <string>

// Clickable chord card shown in the progression panel.
// Displays chord name + Roman numeral + probability;
// provides hover/press visual feedback and an onClick callback.
class ChordButton : public juce::Component
{
public:
    std::function<void()> onClick;

    ChordButton() = default;

    void setChord(const std::string& chordName,
                  const std::string& roman,
                  float probability);

    void setActive(bool active);   // highlights when this chord is currently detected

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    std::string _name;
    std::string _roman;
    float       _prob    { 0.0f };
    bool        _active  { false };
    bool        _pressed { false };
    bool        _hovered { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordButton)
};
