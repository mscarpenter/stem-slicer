#include "AudioAnalyzer.h"
#include <algorithm>
#include <cmath>

static constexpr float CHROMA_HALF_LIFE_SECONDS = 2.0f;

AudioAnalyzer::AudioAnalyzer(double sampleRate, int fftOrder)
    : _sampleRate(sampleRate)
    , _fftSize(1 << fftOrder)
    , _fft(fftOrder)
    , _window(static_cast<size_t>(1) << fftOrder, juce::dsp::WindowingFunction<float>::hann)
    , _circularBuffer(static_cast<size_t>(1) << fftOrder, 0.0f)
    , _fftBuffer(static_cast<size_t>(2) * (static_cast<size_t>(1) << fftOrder), 0.0f)
    , _hopDecay(std::pow(0.5f,
          static_cast<float>((1 << fftOrder) / 2)
          / (static_cast<float>(sampleRate) * CHROMA_HALF_LIFE_SECONDS)))
    , _yin(sampleRate)
{
}

void AudioAnalyzer::processAudioBlock(const float* samples, int numSamples)
{
    // ── FFT chroma pipeline ───────────────────────────────────────────────────
    for (int i = 0; i < numSamples; ++i)
    {
        _circularBuffer[static_cast<size_t>(_writePos++)] = samples[i];
        if (_writePos == _fftSize)
        {
            runFFT();
            // 50% overlap: shift second half to front
            std::copy(_circularBuffer.begin() + _fftSize / 2,
                      _circularBuffer.end(),
                      _circularBuffer.begin());
            _writePos = _fftSize / 2;
        }
    }

    // ── YIN pitch pipeline ────────────────────────────────────────────────────
    if (numSamples > 0)
    {
        float pitch = _yin.process(samples, numSamples);
        juce::SpinLock::ScopedLockType lock(_pitchLock);
        _detectedPitch = pitch;
    }
}

void AudioAnalyzer::runFFT()
{
    // Copy circular buffer into FFT working buffer; zero the second half
    std::copy(_circularBuffer.begin(), _circularBuffer.end(), _fftBuffer.begin());
    std::fill(_fftBuffer.begin() + _fftSize, _fftBuffer.end(), 0.0f);

    // Hann-window the real part before transform
    _window.multiplyWithWindowingTable(_fftBuffer.data(), static_cast<size_t>(_fftSize));

    // In-place transform: writes power per bin into _fftBuffer[0..fftSize/2]
    _fft.performFrequencyOnlyForwardTransform(_fftBuffer.data());

    updateChroma();
}

void AudioAnalyzer::updateChroma()
{
    std::array<float, 12> frame {};

    // Accumulate spectral power into pitch-class bins (skip DC, stop at Nyquist)
    for (int bin = 1; bin <= _fftSize / 2; ++bin)
    {
        float freq = static_cast<float>(bin)
                     * static_cast<float>(_sampleRate)
                     / static_cast<float>(_fftSize);
        int pc = freqToPitchClass(freq);
        if (pc >= 0)
            frame[static_cast<size_t>(pc)] += _fftBuffer[static_cast<size_t>(bin)];
    }

    // Normalize frame to unit sum so all frames carry equal weight
    float sum = 0.0f;
    for (float v : frame) sum += v;
    if (sum > 1e-6f)
        for (float& v : frame) v /= sum;

    // Exponential moving average across hops
    juce::SpinLock::ScopedLockType lock(_chromaLock);
    for (int i = 0; i < 12; ++i)
        _chroma[i] = _hopDecay * _chroma[i] + (1.0f - _hopDecay) * frame[i];
}

std::array<float, 12> AudioAnalyzer::getChromaVector() const
{
    juce::SpinLock::ScopedLockType lock(_chromaLock);
    return _chroma;
}

float AudioAnalyzer::getDetectedPitch() const
{
    juce::SpinLock::ScopedLockType lock(_pitchLock);
    return _detectedPitch;
}

int AudioAnalyzer::freqToPitchClass(float freqHz) const
{
    if (freqHz < 20.0f || freqHz > 20000.0f) return -1;
    float midiNote  = 69.0f + 12.0f * std::log2(freqHz / 440.0f);
    int   pitchClass = static_cast<int>(std::round(midiNote)) % 12;
    return (pitchClass + 12) % 12;
}
