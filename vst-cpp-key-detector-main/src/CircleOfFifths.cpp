#include "CircleOfFifths.h"
#include <cmath>

// ─── static name tables (indexed by CoF wheel position, 0 = 12 o'clock) ─────

const std::array<const char*, 12> CircleOfFifths::MAJOR_NAMES {
    "C","G","D","A","E","B","F#","Db","Ab","Eb","Bb","F"
};
const std::array<const char*, 12> CircleOfFifths::MINOR_NAMES {
    "Am","Em","Bm","F#m","C#m","G#m","D#m","Bbm","Fm","Cm","Gm","Dm"
};

// ─── state ────────────────────────────────────────────────────────────────────

void CircleOfFifths::setState(int rootClass, const std::string& mode)
{
    _root = rootClass;
    _mode = mode;
    repaint();
}

// ─── paint ────────────────────────────────────────────────────────────────────

void CircleOfFifths::paint(juce::Graphics& g)
{
    const auto  boundsF  = getLocalBounds().toFloat().reduced(4.0f);
    const float diameter = std::min(boundsF.getWidth(), boundsF.getHeight());
    const float cx       = boundsF.getCentreX();
    const float cy       = boundsF.getCentreY();

    const float rOuter = diameter * 0.5f;
    const float rMid   = rOuter  * 0.61f;   // boundary between major and minor rings
    const float rInner = rOuter  * 0.37f;   // inner hole radius

    const float segAngle    = juce::MathConstants<float>::twoPi / 12.0f;
    const float startOffset = -juce::MathConstants<float>::halfPi;
    const float gap         = 0.020f;       // gap between wedges (radians)

    // Locate which wheel position holds the detected root
    int majorPos = -1, minorPos = -1;
    for (int i = 0; i < 12; ++i)
    {
        if (MAJOR_ORDER[static_cast<size_t>(i)] == _root) majorPos = i;
        if (MINOR_ORDER[static_cast<size_t>(i)] == _root) minorPos = i;
    }

    // ─── draw 12 segments ────────────────────────────────────────────────────
    for (int i = 0; i < 12; ++i)
    {
        const float a0   = startOffset + static_cast<float>(i) * segAngle + gap;
        const float a1   = a0 + segAngle - gap * 2.0f;
        const float aMid = (a0 + a1) * 0.5f;

        // ── Major (outer) ring ────────────────────────────────────────────
        {
            const bool isActive   = (_mode == "major" && i == majorPos);
            const bool isAdjacent = (_mode == "major" && majorPos >= 0 &&
                                     (i == (majorPos + 1) % 12 ||
                                      i == (majorPos + 11) % 12));

            const juce::Colour fill =
                isActive   ? juce::Colour(0xff00cfff) :
                isAdjacent ? juce::Colour(0xff1e4060) :
                             juce::Colour(0xff181c38);

            juce::Path wedge;
            wedge.addPieSegment(cx - rOuter, cy - rOuter,
                                rOuter * 2.0f, rOuter * 2.0f,
                                a0, a1, rMid / rOuter);

            g.setColour(fill);
            g.fillPath(wedge);

            g.setColour(juce::Colour(0xff2a2e58));
            g.strokePath(wedge, juce::PathStrokeType(1.0f));

            // Label centred in the ring band
            const float lr = (rOuter + rMid) * 0.5f;
            const float lx = cx + lr * std::cos(aMid);
            const float ly = cy + lr * std::sin(aMid);

            g.setColour(isActive ? juce::Colours::black
                                 : juce::Colour(0xff6868a8));
            g.setFont(juce::Font(isActive ? 9.5f : 8.5f,
                                 isActive ? juce::Font::bold : juce::Font::plain));
            g.drawText(MAJOR_NAMES[static_cast<size_t>(i)],
                       juce::Rectangle<float>(lx - 14.0f, ly - 7.5f, 28.0f, 15.0f),
                       juce::Justification::centred);
        }

        // ── Minor (inner) ring ────────────────────────────────────────────
        {
            const bool isActive   = (_mode == "minor" && i == minorPos);
            const bool isAdjacent = (_mode == "minor" && minorPos >= 0 &&
                                     (i == (minorPos + 1) % 12 ||
                                      i == (minorPos + 11) % 12));

            const juce::Colour fill =
                isActive   ? juce::Colour(0xff8888ff) :
                isAdjacent ? juce::Colour(0xff1e1c48) :
                             juce::Colour(0xff101020);

            juce::Path wedge;
            wedge.addPieSegment(cx - rMid, cy - rMid,
                                rMid * 2.0f, rMid * 2.0f,
                                a0, a1, rInner / rMid);

            g.setColour(fill);
            g.fillPath(wedge);

            g.setColour(juce::Colour(0xff2a2e58));
            g.strokePath(wedge, juce::PathStrokeType(0.8f));

            // Label centred in the ring band
            const float lr = (rMid + rInner) * 0.5f;
            const float lx = cx + lr * std::cos(aMid);
            const float ly = cy + lr * std::sin(aMid);

            g.setColour(isActive ? juce::Colours::white
                                 : juce::Colour(0xff383860));
            g.setFont(juce::Font(isActive ? 7.5f : 7.0f,
                                 isActive ? juce::Font::bold : juce::Font::plain));
            g.drawText(MINOR_NAMES[static_cast<size_t>(i)],
                       juce::Rectangle<float>(lx - 14.0f, ly - 6.0f, 28.0f, 12.0f),
                       juce::Justification::centred);
        }
    }

    // ─── center circle ───────────────────────────────────────────────────────
    g.setColour(juce::Colour(0xff0e0e1e));
    g.fillEllipse(cx - rInner, cy - rInner, rInner * 2.0f, rInner * 2.0f);
    g.setColour(juce::Colour(0xff2a2e58));
    g.drawEllipse(cx - rInner, cy - rInner, rInner * 2.0f, rInner * 2.0f, 1.0f);

    // Key label in the center hole
    juce::String centerName;
    if (_mode == "major" && majorPos >= 0)
        centerName = juce::String(MAJOR_NAMES[static_cast<size_t>(majorPos)]);
    else if (_mode == "minor" && minorPos >= 0)
        centerName = juce::String(MINOR_NAMES[static_cast<size_t>(minorPos)]);
    else
        centerName = "?";

    g.setColour(juce::Colour(0xff00cfff));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(centerName,
               juce::Rectangle<float>(cx - rInner, cy - 8.0f, rInner * 2.0f, 16.0f),
               juce::Justification::centred);
}
