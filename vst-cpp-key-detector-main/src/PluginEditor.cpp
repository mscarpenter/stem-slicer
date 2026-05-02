#include "PluginEditor.h"
#include "PluginProcessor.h"

// ─── layout constants ────────────────────────────────────────────────────────

static constexpr int kEditorW   = 500;
static constexpr int kEditorH   = 380;
static constexpr int kHeaderH   = 55;
static constexpr int kLedLabelH = 18;
static constexpr int kLedBarH   = 48;
static constexpr int kCentralH  = 174;
// progression panel: 380 - 55 - 18 - 48 - 174 = 85 px

static const std::array<const char*, 12> ROOT_NAMES {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

// Parses a chord name (e.g. "G", "Am", "F#m", "Bdim", "Db") and returns
// {root, third, fifth, root+octave} as MIDI notes in octave 3 (C3 = 48).
// Invalid/empty → all -1.
static std::array<int, 4> chordNameToMidi(const std::string& name)
{
    constexpr std::array<int, 4> kNone { -1,-1,-1,-1 };
    if (name.empty()) return kNone;

    int rootPc = -1;
    switch (name[0])
    {
        case 'C': rootPc = 0;  break;
        case 'D': rootPc = 2;  break;
        case 'E': rootPc = 4;  break;
        case 'F': rootPc = 5;  break;
        case 'G': rootPc = 7;  break;
        case 'A': rootPc = 9;  break;
        case 'B': rootPc = 11; break;
        default:  return kNone;
    }
    size_t pos = 1;

    if (pos < name.size() && name[pos] == '#')
        { rootPc = (rootPc + 1)  % 12; ++pos; }
    else if (pos < name.size() && name[pos] == 'b')
        { rootPc = (rootPc + 11) % 12; ++pos; }

    const std::string suffix = name.substr(pos);
    int third = 4, fifth = 7;           // major default
    if (suffix == "m")   { third = 3; }
    if (suffix == "dim") { third = 3; fifth = 6; }

    const int root = 48 + rootPc;       // C3 = MIDI 48
    return { root, root + third, root + fifth, root + 12 };
}

// ─── construction ────────────────────────────────────────────────────────────

KeyDetectorEditor::KeyDetectorEditor(KeyDetectorProcessor& p)
    : AudioProcessorEditor(&p), _processor(p)
{
    addAndMakeVisible(_ledBar);
    addAndMakeVisible(_cof);

    for (int i = 0; i < 3; ++i)
    {
        addAndMakeVisible(_suggestionBtns[i]);
        _suggestionBtns[i].onClick = [this, i]()
        {
            if (static_cast<size_t>(i) < _suggestions.size())
            {
                const auto midi = chordNameToMidi(_suggestions[static_cast<size_t>(i)].name);
                _processor.scheduleChord(midi);
            }
        };
    }

    setSize(kEditorW, kEditorH);
    startTimerHz(30);
}

KeyDetectorEditor::~KeyDetectorEditor()
{
    stopTimer();
}

// ─── layout ──────────────────────────────────────────────────────────────────

void KeyDetectorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(kHeaderH + kLedLabelH);
    _ledBar.setBounds(area.removeFromTop(kLedBarH).reduced(8, 2));

    // Circle of Fifths: centred square; flanked by key/scale info panels
    const int centralY = kHeaderH + kLedLabelH + kLedBarH;
    const int cofSize  = kCentralH - 18;                          // 156 px
    const int cofX     = (kEditorW - cofSize) / 2;               // 172
    _cof.setBounds(cofX, centralY + 9, cofSize, cofSize);

    // Chord suggestion buttons in the progression panel
    const int progY = kHeaderH + kLedLabelH + kLedBarH + kCentralH;  // 295
    const int btnY  = progY + 8;
    const int btnH  = kEditorH - btnY - 10;                           // ~67 px
    const int btnX0 = 100;
    const int btnW  = (kEditorW - btnX0 - 14) / 3;                   // ~128 px
    for (int i = 0; i < 3; ++i)
        _suggestionBtns[i].setBounds(btnX0 + i * (btnW + 4), btnY, btnW, btnH);
}

// ─── timer ───────────────────────────────────────────────────────────────────

void KeyDetectorEditor::timerCallback()
{
    _activeNotes = _processor.getActiveNotes();
    _keyResult   = _processor.getLastKeyResult();
    _scaleResult = _processor.getLastScaleResult();
    _roman       = _processor.getLastRoman();
    _suggestions = _processor.getLastSuggestions();

    // Derive which pitch classes belong to the current scale
    std::array<bool, 12> inScale {};
    for (int interval : _scaleResult.formula)
        inScale[(_keyResult.root + interval + 12) % 12] = true;

    _ledBar.setState(_activeNotes, inScale);
    _cof.setState(_keyResult.root, _keyResult.mode);

    for (int i = 0; i < 3; ++i)
    {
        if (static_cast<size_t>(i) < _suggestions.size())
        {
            const auto& s = _suggestions[static_cast<size_t>(i)];
            _suggestionBtns[i].setChord(s.name, s.roman, s.probability);
            _suggestionBtns[i].setActive(s.roman == _roman);
            _suggestionBtns[i].setVisible(true);
        }
        else
        {
            _suggestionBtns[i].setChord("", "", 0.0f);
            _suggestionBtns[i].setActive(false);
            _suggestionBtns[i].setVisible(false);
        }
    }

    repaint();
}

// ─── paint ───────────────────────────────────────────────────────────────────

void KeyDetectorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));

    // ── Header panel ─────────────────────────────────────────────────────
    {
        juce::Rectangle<int> hdr { 0, 0, kEditorW, kHeaderH };
        g.setColour(juce::Colour(0xff1e1e38));
        g.fillRect(hdr);

        g.setColour(juce::Colour(0xff3a3a5c));
        g.drawLine(0.0f, static_cast<float>(kHeaderH),
                   static_cast<float>(kEditorW), static_cast<float>(kHeaderH), 1.0f);

        // Title
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(20.0f, juce::Font::bold));
        g.drawText("KEY DETECTOR",
                   juce::Rectangle<int>(0, 0, kEditorW / 2, kHeaderH),
                   juce::Justification::centredRight);

        // Key root + mode quick-glance (right half of header)
        const juce::String keyStr =
            juce::String(ROOT_NAMES[_keyResult.root]) + "  " + juce::String(_keyResult.mode);
        g.setColour(juce::Colour(0xff00cfff));
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(keyStr,
                   juce::Rectangle<int>(kEditorW / 2 + 8, 0, kEditorW / 2 - 16, kHeaderH),
                   juce::Justification::centred);
    }

    // ── LED bar section label ─────────────────────────────────────────────
    {
        g.setColour(juce::Colour(0xff6060a0));
        g.setFont(juce::Font(11.0f));
        g.drawText("Active Notes",
                   juce::Rectangle<int>(10, kHeaderH, 120, kLedLabelH),
                   juce::Justification::centredLeft);
    }

    // ── Central row: [Key panel] [CoF background] [Scale panel] ─────────────
    {
        const int   py      = kHeaderH + kLedLabelH + kLedBarH + 9;
        const int   ph      = kCentralH - 18;   // 156 px  (matches CoF size)
        const int   cofSize = ph;
        const int   cofX    = (kEditorW - cofSize) / 2;   // 172
        const int   panelW  = cofX - 10 - 8;              // 154

        // Shared rounded background spanning the full row
        const juce::Rectangle<float> rowBg {
            10.0f, static_cast<float>(py),
            static_cast<float>(kEditorW - 20), static_cast<float>(ph)
        };
        g.setColour(juce::Colour(0xff0e0e1e));
        g.fillRoundedRectangle(rowBg, 8.0f);
        g.setColour(juce::Colour(0xff2a2e58));
        g.drawRoundedRectangle(rowBg, 8.0f, 1.0f);

        // Thin vertical dividers on each side of the CoF
        const float divTop = static_cast<float>(py + 12);
        const float divBot = static_cast<float>(py + ph - 12);
        g.setColour(juce::Colour(0xff242450));
        g.drawLine(static_cast<float>(cofX - 2),      divTop,
                   static_cast<float>(cofX - 2),      divBot, 1.0f);
        g.drawLine(static_cast<float>(cofX + cofSize + 2), divTop,
                   static_cast<float>(cofX + cofSize + 2), divBot, 1.0f);

        // ── Left panel: Key info ──────────────────────────────────────────
        {
            const int px = 14;

            // Section label
            g.setColour(juce::Colour(0xff44447a));
            g.setFont(juce::Font(9.0f));
            g.drawText("KEY", juce::Rectangle<int>(px, py + 4, panelW, 12),
                       juce::Justification::topLeft);

            // Large root note name
            g.setColour(juce::Colour(0xff00cfff));
            g.setFont(juce::Font(44.0f, juce::Font::bold));
            g.drawText(juce::String(ROOT_NAMES[_keyResult.root]),
                       juce::Rectangle<int>(px, py + 14, panelW, 52),
                       juce::Justification::centredLeft);

            // Mode string
            g.setColour(juce::Colour(0xffaaaacc));
            g.setFont(juce::Font(12.0f));
            g.drawText(juce::String(_keyResult.mode),
                       juce::Rectangle<int>(px, py + 66, panelW, 16),
                       juce::Justification::centredLeft);

            // Confidence bar
            const float conf = juce::jlimit(0.0f, 1.0f, _keyResult.confidence);
            const int   barY = py + 90;

            g.setColour(juce::Colour(0xff44447a));
            g.setFont(juce::Font(9.0f));
            g.drawText("confidence",
                       juce::Rectangle<int>(px, barY - 12, panelW, 11),
                       juce::Justification::centredLeft);

            juce::Rectangle<int> barBg { px, barY, panelW - 8, 8 };
            g.setColour(juce::Colour(0xff141428));
            g.fillRect(barBg);

            const juce::Colour barCol =
                conf > 0.60f ? juce::Colour(0xff00cfff) :
                conf > 0.30f ? juce::Colour(0xffffaa00) :
                               juce::Colour(0xffff5555);
            g.setColour(barCol);
            g.fillRect(barBg.withWidth(static_cast<int>((panelW - 8) * conf)));

            g.setColour(juce::Colour(0xff2a2e58));
            g.drawRect(barBg, 1);

            g.setColour(juce::Colour(0xff7070a0));
            g.setFont(juce::Font(9.0f));
            g.drawText(juce::String(juce::roundToInt(conf * 100.0f)) + "%",
                       juce::Rectangle<int>(px, barY + 10, panelW - 8, 12),
                       juce::Justification::centredRight);
        }

        // ── Right panel: Scale info ───────────────────────────────────────
        {
            const int px = cofX + cofSize + 10;

            // Section label
            g.setColour(juce::Colour(0xff44447a));
            g.setFont(juce::Font(9.0f));
            g.drawText("SCALE", juce::Rectangle<int>(px, py + 4, panelW, 12),
                       juce::Justification::topLeft);

            // Scale name (up to 2 lines)
            g.setColour(juce::Colour(0xff8888ff));
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawFittedText(juce::String(_scaleResult.name),
                             juce::Rectangle<int>(px, py + 14, panelW, 34),
                             juce::Justification::topLeft, 2);

            // Notes in scale
            if (!_scaleResult.formula.empty())
            {
                juce::String notesStr;
                for (int interval : _scaleResult.formula)
                {
                    const int pc = (_keyResult.root + interval + 12) % 12;
                    notesStr += juce::String(ROOT_NAMES[static_cast<size_t>(pc)]);
                    notesStr += " ";
                }

                g.setColour(juce::Colour(0xff44447a));
                g.setFont(juce::Font(9.0f));
                g.drawText("notes in key",
                           juce::Rectangle<int>(px, py + 52, panelW, 11),
                           juce::Justification::topLeft);

                g.setColour(juce::Colour(0xffaaaacc));
                g.setFont(juce::Font(9.5f));
                g.drawFittedText(notesStr.trimEnd(),
                                 juce::Rectangle<int>(px, py + 64, panelW, 26),
                                 juce::Justification::topLeft, 2);
            }

            // Mode-fit confidence bar
            const float sc   = juce::jlimit(0.0f, 1.0f, _scaleResult.confidence);
            const int   barY = py + 100;

            g.setColour(juce::Colour(0xff44447a));
            g.setFont(juce::Font(9.0f));
            g.drawText("mode fit",
                       juce::Rectangle<int>(px, barY - 12, panelW, 11),
                       juce::Justification::centredLeft);

            juce::Rectangle<int> barBg { px, barY, panelW - 8, 8 };
            g.setColour(juce::Colour(0xff141428));
            g.fillRect(barBg);
            g.setColour(juce::Colour(0xff8888ff));
            g.fillRect(barBg.withWidth(static_cast<int>((panelW - 8) * sc)));
            g.setColour(juce::Colour(0xff2a2e58));
            g.drawRect(barBg, 1);

            g.setColour(juce::Colour(0xff7070a0));
            g.setFont(juce::Font(9.0f));
            g.drawText(juce::String(juce::roundToInt(sc * 100.0f)) + "%",
                       juce::Rectangle<int>(px, barY + 10, panelW - 8, 12),
                       juce::Justification::centredRight);
        }
    }

    // ── Progression panel ─────────────────────────────────────────────────
    {
        const int y = kHeaderH + kLedLabelH + kLedBarH + kCentralH;
        const juce::Rectangle<float> box {
            12.0f, static_cast<float>(y) + 4.0f,
            static_cast<float>(kEditorW) - 24.0f,
            static_cast<float>(kEditorH - y) - 8.0f
        };
        g.setColour(juce::Colour(0xff1e1e38));
        g.fillRoundedRectangle(box, 8.0f);
        g.setColour(juce::Colour(0xff3a3a5c));
        g.drawRoundedRectangle(box, 8.0f, 1.0f);

        // Section label
        g.setColour(juce::Colour(0xff9090d0));
        g.setFont(juce::Font(10.0f));
        g.drawText("Now playing:",
                   juce::Rectangle<int>(20, y + 8, 90, 13),
                   juce::Justification::centredLeft);

        // Current Roman numeral
        g.setColour(juce::Colour(0xff00cfff));
        g.setFont(juce::Font(20.0f, juce::Font::bold));
        g.drawText(juce::String(_roman),
                   juce::Rectangle<int>(20, y + 20, 70, 34),
                   juce::Justification::centred);

        // Separator between Roman numeral column and chord buttons
        g.setColour(juce::Colour(0xff2e3060));
        g.drawLine(94.0f, static_cast<float>(y + 10),
                   94.0f, static_cast<float>(y + kEditorH - y - 10), 1.0f);

        // "Next chord →" label above the button row
        g.setColour(juce::Colour(0xff44447a));
        g.setFont(juce::Font(9.0f));
        g.drawText("next  \xe2\x86\x92",    // UTF-8 "→"
                   juce::Rectangle<int>(100, y + 8, kEditorW - 114, 12),
                   juce::Justification::centredLeft);
    }
}
