#include "AudioFileWriter.h"
#include <stdexcept>

namespace audio {

void writeAudioFile(
    const std::filesystem::path& path,
    const juce::AudioBuffer<float>& buffer,
    double sampleRate)
{
    std::filesystem::create_directories(path.parent_path());

    juce::File file(path.string());
    if (file.existsAsFile())
        file.deleteFile();

    juce::WavAudioFormat wavFormat;

    std::unique_ptr<juce::FileOutputStream> outputStream(file.createOutputStream());
    if (outputStream == nullptr)
        throw std::runtime_error("AudioFileWriter: cannot open output stream for " + path.string());

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(
            outputStream.release(),
            sampleRate,
            static_cast<unsigned int>(buffer.getNumChannels()),
            32,
            {},
            0));

    if (writer == nullptr)
        throw std::runtime_error("AudioFileWriter: cannot create WAV writer for " + path.string());

    if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
        throw std::runtime_error("AudioFileWriter: failed to write samples to " + path.string());
}

} // namespace audio
