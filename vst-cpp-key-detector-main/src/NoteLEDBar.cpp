#include "NoteLEDBar.h"

void NoteLEDBar::setState(const std::array<bool, 12>& active,
                          const std::array<bool, 12>& inScale)
{
    _active  = active;
    _inScale = inScale;
    repaint();
}

void NoteLEDBar::paint(juce::Graphics& g)
{
    const float totalW  = static_cast<float>(getWidth());
    const float totalH  = static_cast<float>(getHeight());
    const float slotW   = totalW / 12.0f;
    const float labelH  = 14.0f;
    const float ledH    = totalH - labelH - 4.0f;
    const float padX    = 2.5f;
    const float padY    = 2.0f;

    for (int i = 0; i < 12; ++i)
    {
        const float x = static_cast<float>(i) * slotW + padX;
        const float y = padY;
        const float w = slotW - padX * 2.0f;
        const float h = ledH - padY;

        juce::Colour ledColour;
        if (_active[static_cast<size_t>(i)])
            ledColour = _inScale[static_cast<size_t>(i)]
                            ? juce::Colour(0xff00cfff)   // in-scale: cyan
                            : juce::Colour(0xffff5555);  // out-of-scale: red
        else
            ledColour = juce::Colour(0xff252540);        // off: dark

        // Gradient fill: slightly brighter at top
        juce::ColourGradient grad(ledColour.brighter(0.12f), x, y,
                                  ledColour.darker(0.18f), x, y + h, false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(x, y, w, h, 4.0f);

        // Border: bright glow when active, subtle when off
        g.setColour(_active[static_cast<size_t>(i)]
                        ? ledColour.brighter(0.5f)
                        : juce::Colour(0xff3a3a5a));
        g.drawRoundedRectangle(x, y, w, h, 4.0f, 1.0f);

        // Note name label below the LED
        g.setColour(_active[static_cast<size_t>(i)]
                        ? juce::Colours::white
                        : juce::Colour(0xff5050a0));
        g.setFont(juce::Font(9.5f, juce::Font::plain));
        g.drawText(NOTE_NAMES[static_cast<size_t>(i)],
                   juce::Rectangle<float>(x, y + h + 2.0f, w, labelH),
                   juce::Justification::centred);
    }
}
