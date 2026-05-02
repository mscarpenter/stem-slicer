#include "AudioFileReader.h"
#include <stdexcept>

namespace audio {

juce::AudioBuffer<float> loadAudioFile(
    const std::filesystem::path& path,
    double targetSampleRate)
{
    if (!std::filesystem::exists(path))
        throw std::runtime_error("File not found: " + path.string());

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::File file(path.string());
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader == nullptr)
        throw std::runtime_error("Unsupported format or unreadable file: " + path.filename().string());

    const int numChannels = static_cast<int>(reader->numChannels);
    const int numSamples  = static_cast<int>(reader->lengthInSamples);

    juce::AudioBuffer<float> raw(numChannels, numSamples);
    reader->read(&raw, 0, numSamples, 0, true, true);

    juce::AudioBuffer<float> resampled;

    if (reader->sampleRate != targetSampleRate)
    {
        const double ratio = reader->sampleRate / targetSampleRate;
        const int numOutputSamples = static_cast<int>(static_cast<double>(numSamples) / ratio);

        resampled = juce::AudioBuffer<float>(numChannels, numOutputSamples);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            juce::LagrangeInterpolator interpolator;
            interpolator.process(ratio,
                                 raw.getReadPointer(ch),
                                 resampled.getWritePointer(ch),
                                 numOutputSamples);
        }
    }
    else
    {
        resampled = std::move(raw);
    }

    if (resampled.getNumChannels() == 1)
    {
        juce::AudioBuffer<float> stereo(2, resampled.getNumSamples());
        stereo.copyFrom(0, 0, resampled, 0, 0, resampled.getNumSamples());
        stereo.copyFrom(1, 0, resampled, 0, 0, resampled.getNumSamples());
        return stereo;
    }

    if (resampled.getNumChannels() == 2)
        return resampled;

    juce::AudioBuffer<float> stereo(2, resampled.getNumSamples());
    stereo.copyFrom(0, 0, resampled, 0, 0, resampled.getNumSamples());
    stereo.copyFrom(1, 0, resampled, 1, 0, resampled.getNumSamples());
    return stereo;
}

} // namespace audio
