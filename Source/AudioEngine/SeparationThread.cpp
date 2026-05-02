#include "SeparationThread.h"

namespace audio {

SeparationThread::SeparationThread(StemSeparator& separator,
                                   std::filesystem::path inputPath,
                                   std::filesystem::path outputDir)
    : juce::Thread("StemSlicer_SeparationThread")
    , separator_(separator)
    , inputPath_(std::move(inputPath))
    , outputDir_(std::move(outputDir))
{}

SeparationThread::~SeparationThread()
{
    stopThread(5000); // wait up to 5 s for clean shutdown
}

void SeparationThread::run()
{
    Result result;
    try
    {
        separator_.separateFile(
            inputPath_,
            outputDir_,
            [this](float p) {
                if (onProgress) onProgress(p);
            });
        result.success = true;
    }
    catch (const std::exception& e)
    {
        result.success      = false;
        result.errorMessage = e.what();
    }

    if (onFinished) onFinished(result);
}

} // namespace audio
