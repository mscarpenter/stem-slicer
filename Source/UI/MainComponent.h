#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../AudioEngine/StemSeparator.h"
#include "../AudioEngine/SeparationThread.h"
#include <filesystem>
#include <memory>

namespace ui {

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget
{
public:
    explicit MainComponent(audio::StemSeparator& separator);
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;

private:
    void startSeparation(const std::filesystem::path& inputPath);
    void onProgress(float p);
    void onFinished(audio::SeparationThread::Result result);
    void chooseInputFile();
    void chooseOutputDir();

    audio::StemSeparator& separator_;
    std::unique_ptr<audio::SeparationThread> thread_;

    // State
    std::filesystem::path inputPath_;
    std::filesystem::path outputDir_;
    float progress_ = 0.0f;
    bool filesDragging_ = false;
    enum class State { Idle, Processing, Done, Error } state_ = State::Idle;
    juce::String statusMessage_;

    // Controls
    juce::TextButton openFileBtn_    { "OPEN FILE" };
    juce::TextButton outputDirBtn_   { "OUTPUT FOLDER" };
    juce::TextButton processBtn_     { "SEPARATE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace ui
