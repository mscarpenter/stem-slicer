#include "ChordButton.h"

void ChordButton::setChord(const std::string& chordName,
                           const std::string& roman,
                           float probability)
{
    _name  = chordName;
    _roman = roman;
    _prob  = probability;
    repaint();
}

void ChordButton::setActive(bool active)
{
    if (_active != active)
    {
        _active = active;
        repaint();
    }
}

void ChordButton::paint(juce::Graphics& g)
{
    const auto  bounds = getLocalBounds().toFloat().reduced(2.0f);
    const float h      = bounds.getHeight();

    // ── Background ────────────────────────────────────────────────────────
    const juce::Colour bg =
        _pressed ? juce::Colour(0xff003060) :
        _hovered ? juce::Colour(0xff182040) :
        _active  ? juce::Colour(0xff0a1838) :
                   juce::Colour(0xff11112a);
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 6.0f);

    // ── Border ────────────────────────────────────────────────────────────
    const juce::Colour border =
        _active  ? juce::Colour(0xff00cfff) :
        _hovered ? juce::Colour(0xff3a4888) :
                   juce::Colour(0xff2a2e58);
    g.setColour(border);
    g.drawRoundedRectangle(bounds, 6.0f, _active ? 1.5f : 1.0f);

    if (_name.empty()) return;

    // ── Chord name (upper ~60 % of height) ───────────────────────────────
    const juce::Colour nameCol =
        _pressed || _hovered || _active ? juce::Colours::white
                                        : juce::Colour(0xffccccee);
    g.setColour(nameCol);
    g.setFont(juce::Font(17.0f, juce::Font::bold));
    g.drawText(juce::String(_name),
               getLocalBounds().reduced(4, 3).removeFromTop(static_cast<int>(h * 0.58f)),
               juce::Justification::centred);

    // ── Roman + probability (lower ~35 %) ─────────────────────────────────
    const juce::String sub =
        juce::String(_roman) + "  "
        + juce::String(juce::roundToInt(_prob * 100.0f)) + "%";
    g.setColour(juce::Colour(0xff5858a0));
    g.setFont(juce::Font(9.0f));
    g.drawText(sub,
               getLocalBounds().reduced(4, 2).removeFromBottom(static_cast<int>(h * 0.35f)),
               juce::Justification::centred);
}

void ChordButton::mouseDown(const juce::MouseEvent&)
{
    _pressed = true;
    repaint();
    if (onClick) onClick();
}

void ChordButton::mouseUp(const juce::MouseEvent&)
{
    _pressed = false;
    repaint();
}

void ChordButton::mouseEnter(const juce::MouseEvent&)
{
    _hovered = true;
    repaint();
}

void ChordButton::mouseExit(const juce::MouseEvent&)
{
    _hovered = false;
    _pressed = false;
    repaint();
}
