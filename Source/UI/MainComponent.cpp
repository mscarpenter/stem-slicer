#include "MainComponent.h"

namespace ui {

// ─── Colour palette ───────────────────────────────────────────────────────────
// juce::Colour ctor is not constexpr in MSVC builds — use const instead.
static const juce::Colour kColourBg       { 0xff1a1a2e };
static const juce::Colour kColourPanel    { 0xff1e1e38 };
static const juce::Colour kColourSunken   { 0xff0e0e1e };
static const juce::Colour kColourCyan     { 0xff00cfff };
static const juce::Colour kColourViolet   { 0xff8888ff };
static const juce::Colour kColourBorder   { 0xff2a2e58 };
static const juce::Colour kColourBorderHi { 0xff3a3a5c };
static const juce::Colour kColourTextHi   { 0xffffffff };
static const juce::Colour kColourTextDim  { 0xffaaaacc };
static const juce::Colour kColourLabel    { 0xff44447a };

// ─── Layout constants ─────────────────────────────────────────────────────────
static constexpr int kHeaderH      = 60;
static constexpr int kDropZoneY    = 80;
static constexpr int kDropZoneH    = 200;
static constexpr int kBadgesY      = 300;
static constexpr int kBadgesH      = 40;
static constexpr int kProgressY    = 358;
static constexpr int kProgressH    = 12;
static constexpr int kButtonsY     = 430;
static constexpr int kButtonsH     = 40;
static constexpr int kMargin       = 20;
static constexpr int kBtnOpenW     = 130;
static constexpr int kBtnProcessW  = 130;
static constexpr int kBtnGap       = 10;
static constexpr float kRadius     = 8.0f;
static constexpr float kRadiusSm   = 6.0f;

// ─── Helper: draw a filled + bordered rounded rect ───────────────────────────
static void drawRoundedPanel(juce::Graphics& g,
                              juce::Rectangle<float> r,
                              juce::Colour fill,
                              juce::Colour border,
                              float radius,
                              float borderWidth = 1.0f)
{
    g.setColour(fill);
    g.fillRoundedRectangle(r, radius);
    g.setColour(border);
    g.drawRoundedRectangle(r.reduced(borderWidth * 0.5f), radius, borderWidth);
}

// ─── Constructor / Destructor ─────────────────────────────────────────────────
MainComponent::MainComponent(audio::StemSeparator& separator)
    : separator_(separator)
{
    addAndMakeVisible(openFileBtn_);
    addAndMakeVisible(outputDirBtn_);
    addAndMakeVisible(processBtn_);

    openFileBtn_.setButtonText("OPEN FILE");
    outputDirBtn_.setButtonText("OUTPUT FOLDER");
    processBtn_.setButtonText("SEPARATE");

    // Strip default LookAndFeel so we paint them ourselves
    openFileBtn_.setLookAndFeel(nullptr);
    outputDirBtn_.setLookAndFeel(nullptr);
    processBtn_.setLookAndFeel(nullptr);

    openFileBtn_.onClick = [this] { chooseInputFile(); };
    outputDirBtn_.onClick = [this] { chooseOutputDir(); };
    processBtn_.onClick = [this] {
        if (state_ == State::Idle && !inputPath_.empty())
            startSeparation(inputPath_);
    };

    setSize(800, 500);
}

MainComponent::~MainComponent()
{
    if (thread_ != nullptr)
        thread_->stopThread(3000);
}

// ─── paint ────────────────────────────────────────────────────────────────────
void MainComponent::paint(juce::Graphics& g)
{
    const int w = getWidth();

    // ── Background ────────────────────────────────────────────────────────────
    g.fillAll(kColourBg);

    // ── Header ────────────────────────────────────────────────────────────────
    g.setColour(kColourPanel);
    g.fillRect(0, 0, w, kHeaderH);

    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.setColour(kColourTextHi);
    g.drawText("STEMSLICER", kMargin, 0, 200, kHeaderH, juce::Justification::centredLeft);

    // Status — right-aligned
    juce::Colour statusColour;
    juce::String statusText;
    switch (state_)
    {
        case State::Idle:
            statusColour = kColourTextDim;
            statusText   = "Idle";
            break;
        case State::Processing:
            statusColour = kColourViolet;
            statusText   = "Processing...";
            break;
        case State::Done:
            statusColour = kColourCyan;
            statusText   = statusMessage_.isNotEmpty() ? statusMessage_ : "Done";
            break;
        case State::Error:
            statusColour = juce::Colour(0xffff4466);
            statusText   = statusMessage_.isNotEmpty() ? statusMessage_ : "Error";
            break;
    }
    g.setFont(juce::Font(11.0f));
    g.setColour(statusColour);
    g.drawText(statusText, 0, 0, w - kMargin, kHeaderH, juce::Justification::centredRight);

    // Separator line
    g.setColour(kColourBorderHi);
    g.fillRect(0, kHeaderH, w, 1);

    // ── Drop zone ─────────────────────────────────────────────────────────────
    const juce::Rectangle<float> dropZone(
        (float)kMargin, (float)kDropZoneY,
        (float)(w - kMargin * 2), (float)kDropZoneH);

    const juce::Colour dropBorder = filesDragging_ ? kColourCyan : kColourBorder;
    const float dropBorderW       = filesDragging_ ? 1.5f : 1.0f;
    drawRoundedPanel(g, dropZone, kColourSunken, dropBorder, kRadius, dropBorderW);

    const float dropCentreY = dropZone.getCentreY();

    if (state_ == State::Idle && inputPath_.empty())
    {
        // Arrow icon
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.setColour(kColourBorder);
        g.drawText(juce::CharPointer_UTF8("\xe2\x86\x93"),
                   dropZone.withY(dropZone.getY()).withHeight(dropZone.getHeight() - 60),
                   juce::Justification::centred);

        g.setFont(juce::Font(13.0f));
        g.setColour(kColourTextDim);
        g.drawText("Drop audio file here",
                   dropZone.withY(dropCentreY - 10).withHeight(22),
                   juce::Justification::centred);

        g.setFont(juce::Font(9.5f));
        g.setColour(kColourLabel);
        g.drawText("or click OPEN FILE",
                   dropZone.withY(dropCentreY + 14).withHeight(18),
                   juce::Justification::centred);
    }
    else
    {
        juce::String displayName;
        if (!inputPath_.empty())
            displayName = juce::String(inputPath_.filename().u8string());

        if (state_ == State::Processing)
        {
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.setColour(kColourCyan);
            g.drawText(displayName, dropZone.withY(dropCentreY - 14).withHeight(20),
                       juce::Justification::centred);

            g.setFont(juce::Font(11.0f));
            g.setColour(kColourTextDim);
            g.drawText("Processing...", dropZone.withY(dropCentreY + 8).withHeight(18),
                       juce::Justification::centred);
        }
        else
        {
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.setColour(kColourCyan);
            g.drawText(displayName, dropZone, juce::Justification::centred);
        }
    }

    // ── Stem badges ───────────────────────────────────────────────────────────
    const juce::StringArray stemNames { "VOCALS", "DRUMS", "BASS", "OTHER" };
    const int totalBadgesW = w - kMargin * 2;
    const int badgeGap     = 8;
    const int badgeW       = (totalBadgesW - badgeGap * 3) / 4;

    for (int i = 0; i < 4; ++i)
    {
        const int bx = kMargin + i * (badgeW + badgeGap);
        const juce::Rectangle<float> badgeRect(
            (float)bx, (float)kBadgesY, (float)badgeW, (float)kBadgesH);

        const bool active   = (state_ == State::Done);
        const auto bgColour = kColourPanel;
        const auto bdColour = active ? kColourCyan : kColourBorder;
        const auto txtColour = active ? kColourCyan : kColourTextDim;

        drawRoundedPanel(g, badgeRect, bgColour, bdColour, kRadiusSm, 1.0f);

        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.setColour(txtColour);
        g.drawText(stemNames[i], badgeRect, juce::Justification::centred);
    }

    // ── Progress bar ──────────────────────────────────────────────────────────
    if (state_ == State::Processing || state_ == State::Done)
    {
        const juce::Rectangle<float> barBg(
            (float)kMargin, (float)kProgressY,
            (float)(w - kMargin * 2), (float)kProgressH);

        drawRoundedPanel(g, barBg, kColourSunken, kColourBorder, kRadiusSm, 1.0f);

        const float fillW = barBg.getWidth() * juce::jlimit(0.0f, 1.0f, progress_);
        if (fillW > 0.0f)
        {
            const juce::Rectangle<float> fillRect(barBg.getX(), barBg.getY(), fillW, barBg.getHeight());
            g.setColour(kColourCyan);
            g.fillRoundedRectangle(fillRect, kRadiusSm);
        }

        const int pct = juce::roundToInt(progress_ * 100.0f);
        g.setFont(juce::Font(9.0f));
        g.setColour(kColourTextDim);
        g.drawText(juce::String(pct) + "%",
                   w - kMargin - 36, kProgressY - 1, 36, kProgressH + 2,
                   juce::Justification::centredRight);
    }

    // ── Buttons ───────────────────────────────────────────────────────────────
    // openFileBtn_
    {
        const juce::Rectangle<float> r(
            (float)kMargin, (float)kButtonsY, (float)kBtnOpenW, (float)kButtonsH);
        const bool hov = openFileBtn_.isMouseOver();
        const auto bg  = hov ? kColourPanel.brighter(0.15f) : kColourPanel;
        drawRoundedPanel(g, r, bg, kColourBorder, kRadiusSm, 1.0f);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.setColour(kColourTextDim);
        g.drawText("OPEN FILE", r, juce::Justification::centred);
    }

    // outputDirBtn_
    {
        const int outBtnX = kMargin + kBtnOpenW + kBtnGap;
        const int outBtnW = w - kMargin - outBtnX - kBtnProcessW - kBtnGap;
        const juce::Rectangle<float> r(
            (float)outBtnX, (float)kButtonsY, (float)outBtnW, (float)kButtonsH);
        const bool hov = outputDirBtn_.isMouseOver();
        const auto bg  = hov ? kColourPanel.brighter(0.15f) : kColourPanel;
        drawRoundedPanel(g, r, bg, kColourBorder, kRadiusSm, 1.0f);

        juce::String label = "OUTPUT FOLDER";
        if (!outputDir_.empty())
        {
            const auto dirStr = juce::String(outputDir_.u8string());
            label = dirStr.length() > 40
                        ? juce::String(juce::CharPointer_UTF8("\xe2\x80\xa6")) + dirStr.getLastCharacters(38)
                        : dirStr;
        }

        g.setFont(juce::Font(10.0f));
        g.setColour(kColourTextDim);
        g.drawText(label, r.reduced(6.0f, 0), juce::Justification::centred, true);
    }

    // processBtn_
    {
        const bool enabled = (state_ == State::Idle && !inputPath_.empty());
        const bool hov     = processBtn_.isMouseOver();
        const bool pressed = processBtn_.isMouseButtonDown();

        juce::Colour bg, bdColour, txtColour;
        if (enabled)
        {
            bg       = pressed ? kColourCyan.darker(0.2f)
                               : (hov ? kColourCyan.brighter(0.15f) : kColourCyan);
            bdColour = kColourCyan;
            txtColour = kColourBg;
        }
        else
        {
            bg        = kColourSunken;
            bdColour  = kColourBorder;
            txtColour = kColourLabel;
        }

        const int procBtnX = w - kMargin - kBtnProcessW;
        const juce::Rectangle<float> r(
            (float)procBtnX, (float)kButtonsY, (float)kBtnProcessW, (float)kButtonsH);
        drawRoundedPanel(g, r, bg, bdColour, kRadiusSm, 1.5f);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.setColour(txtColour);
        g.drawText("SEPARATE", r, juce::Justification::centred);
    }
}

// ─── resized ──────────────────────────────────────────────────────────────────
void MainComponent::resized()
{
    const int w = getWidth();

    openFileBtn_.setBounds(kMargin, kButtonsY, kBtnOpenW, kButtonsH);

    const int outBtnX = kMargin + kBtnOpenW + kBtnGap;
    const int outBtnW = w - kMargin - outBtnX - kBtnProcessW - kBtnGap;
    outputDirBtn_.setBounds(outBtnX, kButtonsY, outBtnW, kButtonsH);

    const int procBtnX = w - kMargin - kBtnProcessW;
    processBtn_.setBounds(procBtnX, kButtonsY, kBtnProcessW, kButtonsH);

    // Make buttons transparent so our paint() draws them
    openFileBtn_.setOpaque(false);
    outputDirBtn_.setOpaque(false);
    processBtn_.setOpaque(false);
}

// ─── FileDragAndDropTarget ────────────────────────────────────────────────────
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".aiff")
            return true;
    }
    return false;
}

void MainComponent::fileDragEnter(const juce::StringArray& files, int, int)
{
    if (isInterestedInFileDrag(files))
    {
        filesDragging_ = true;
        repaint();
    }
}

void MainComponent::fileDragExit(const juce::StringArray&)
{
    filesDragging_ = false;
    repaint();
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int)
{
    filesDragging_ = false;
    if (files.isEmpty() || state_ == State::Processing)
        return;

    const std::filesystem::path path(files[0].toStdString());
    startSeparation(path);
}

// ─── Separation control ───────────────────────────────────────────────────────
void MainComponent::startSeparation(const std::filesystem::path& inputPath)
{
    if (state_ == State::Processing)
        return;

    inputPath_ = inputPath;

    if (outputDir_.empty())
        outputDir_ = std::filesystem::path(
            juce::File::getSpecialLocation(juce::File::userMusicDirectory)
#ifdef _WIN32
                .getFullPathName().toWideCharPointer()
#else
                .getFullPathName().toStdString()
#endif
        );

    state_    = State::Processing;
    progress_ = 0.0f;
    statusMessage_.clear();
    repaint();

    thread_ = std::make_unique<audio::SeparationThread>(separator_, inputPath_, outputDir_);

    thread_->onProgress = [this](float p) { onProgress(p); };
    thread_->onFinished = [this](audio::SeparationThread::Result r) { onFinished(r); };

    thread_->startThread();
}

void MainComponent::onProgress(float p)
{
    // All member writes must happen on the message thread to avoid data races with paint().
    juce::MessageManager::callAsync([this, p] {
        progress_ = p;
        repaint();
    });
}

void MainComponent::onFinished(audio::SeparationThread::Result result)
{
    juce::MessageManager::callAsync([this, result = std::move(result)] {
        if (result.success)
        {
            state_         = State::Done;
            progress_      = 1.0f;
            statusMessage_ = "Done";
        }
        else
        {
            state_         = State::Error;
            statusMessage_ = juce::String(result.errorMessage);
        }
        repaint();
    });
}

// ─── File choosers ────────────────────────────────────────────────────────────
void MainComponent::chooseInputFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select audio file",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.wav;*.mp3;*.flac;*.aiff");

    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            if (state_ == State::Processing)
                return;

            const auto result = fc.getResult();
            if (result.existsAsFile())
            {
                state_    = State::Idle;
                progress_ = 0.0f;
                startSeparation(std::filesystem::path(result.getFullPathName().toStdString()));
            }
        });
}

void MainComponent::chooseOutputDir()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select output folder",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory));

    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc)
        {
            const auto result = fc.getResult();
            if (result.isDirectory())
            {
                outputDir_ = std::filesystem::path(result.getFullPathName().toStdString());
                repaint();
            }
        });
}

} // namespace ui
