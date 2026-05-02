#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "StemSeparator.h"
#include <filesystem>
#include <functional>
#include <string>

namespace audio {

class SeparationThread : public juce::Thread
{
public:
    struct Result {
        bool success = false;
        std::string errorMessage;
    };

    // Callbacks — always invoked from the background thread.
    // UI components must use juce::MessageManager::callAsync to update themselves.
    std::function<void(float)>   onProgress; // 0.0 .. 1.0
    std::function<void(Result)>  onFinished; // called once when thread ends

    SeparationThread(StemSeparator& separator,
                     std::filesystem::path inputPath,
                     std::filesystem::path outputDir);

    ~SeparationThread() override;

    // Starts the thread. Returns immediately.
    // Call stopThread() to request cancellation; thread checks threadShouldExit() cooperatively.
    void run() override;

private:
    StemSeparator&        separator_;
    std::filesystem::path inputPath_;
    std::filesystem::path outputDir_;
};

} // namespace audio
