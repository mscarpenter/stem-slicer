#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "NoteLEDBar.h"
#include "CircleOfFifths.h"
#include "ChordButton.h"
#include "KeyEstimator.h"
#include "ScaleClassifier.h"
#include "ProgressionSuggester.h"

class KeyDetectorProcessor;

class KeyDetectorEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit KeyDetectorEditor(KeyDetectorProcessor& p);
    ~KeyDetectorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    KeyDetectorProcessor& _processor;
    NoteLEDBar            _ledBar;
    CircleOfFifths        _cof;
    ChordButton           _suggestionBtns[3];

    // Processor state cached at 30 Hz (message thread only — no lock needed here)
    std::array<bool, 12>         _activeNotes {};
    KeyResult                    _keyResult   {};
    ScaleResult                  _scaleResult {};
    std::string                  _roman       { "?" };
    std::vector<ChordSuggestion> _suggestions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyDetectorEditor)
};
