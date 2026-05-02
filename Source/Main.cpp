#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "ML/OnnxBackend.h"
#include "AudioEngine/StemSeparator.h"
#include "UI/MainComponent.h"

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow(juce::String name,
               std::unique_ptr<ml::OnnxBackend> backend,
               const std::filesystem::path& modelPath)
        : juce::DocumentWindow(name,
                               juce::Colour(0xff1a1a2e),
                               juce::DocumentWindow::allButtons),
          backend_(std::move(backend)),
          separator_(std::make_unique<audio::StemSeparator>(std::move(backend_))),
          mainComponent_(std::make_unique<ui::MainComponent>(*separator_))
    {
        setUsingNativeTitleBar(true);
        setContentNonOwned(mainComponent_.get(), true);
        setResizable(false, false);
        centreWithSize(800, 500);
        setVisible(true);

        if (!modelPath.empty())
        {
            juce::MessageManager::callAsync([this, modelPath]
            {
                if (!std::filesystem::exists(modelPath))
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Model Not Found",
                        "Could not locate: " + juce::String(modelPath.u8string()) +
                        "\n\nPlace 4stems.onnx in the Models/ folder to enable separation.",
                        "OK");
                }
            });
        }
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    // backend_ is moved-from before separator_ is constructed; we keep it here
    // for the transfer only — separator_ owns it after construction.
    std::unique_ptr<ml::OnnxBackend>      backend_;
    std::unique_ptr<audio::StemSeparator> separator_;
    std::unique_ptr<ui::MainComponent>    mainComponent_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

// Searches for 4stems.onnx relative to the executable.
// Checks: <exe_dir>/Models/4stems.onnx, then <exe_dir>/4stems.onnx.
// Returns the first existing path, or the primary candidate if none found.
static std::filesystem::path findModelPath()
{
    const juce::File exeDir =
        juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory();

    // Use wide-string path on Windows — JUCE strings are UTF-16 internally, and
    // toWideCharPointer() preserves non-ASCII characters (accents, CJK, etc.) that
    // toStdString() may mangle when the system codepage is not UTF-8.
    const std::filesystem::path candidates[] = {
#ifdef _WIN32
        std::filesystem::path(exeDir.getChildFile("Models/4stems.onnx").getFullPathName().toWideCharPointer()),
        std::filesystem::path(exeDir.getChildFile("4stems.onnx").getFullPathName().toWideCharPointer()),
#else
        std::filesystem::path(exeDir.getChildFile("Models/4stems.onnx").getFullPathName().toStdString()),
        std::filesystem::path(exeDir.getChildFile("4stems.onnx").getFullPathName().toStdString()),
#endif
    };

    for (const auto& p : candidates)
        if (std::filesystem::exists(p))
            return p;

    return candidates[0];
}

struct StemSlicerApp : public juce::JUCEApplication
{
    const juce::String getApplicationName() override    { return "StemSlicer"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise(const juce::String&) override
    {
        const auto modelPath = findModelPath();

        auto backend = std::make_unique<ml::OnnxBackend>();

        if (std::filesystem::exists(modelPath))
        {
            try
            {
                backend->loadModel(modelPath);
            }
            catch (const std::exception& e)
            {
                juce::MessageManager::callAsync([msg = juce::String(e.what())]
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Model Load Error",
                        "Failed to load model:\n" + msg +
                        "\n\nThe application will run without separation support.",
                        "OK");
                });
            }
        }

        mainWindow_ = std::make_unique<MainWindow>(
            getApplicationName(),
            std::move(backend),
            modelPath);
    }

    void shutdown() override { mainWindow_.reset(); }

    void systemRequestedQuit() override { quit(); }

private:
    std::unique_ptr<MainWindow> mainWindow_;
};

START_JUCE_APPLICATION(StemSlicerApp)
