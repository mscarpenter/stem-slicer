#pragma once
#include <juce_dsp/juce_dsp.h>
#include "YINDetector.h"
#include <array>
#include <vector>

class AudioAnalyzer {
public:
    explicit AudioAnalyzer(double sampleRate, int fftOrder = 12);

    void processAudioBlock(const float* samples, int numSamples);

    std::array<float, 12> getChromaVector() const;
    float getDetectedPitch() const;  // Hz, 0 if not detected

private:
    double _sampleRate;
    int    _fftSize;
    juce::dsp::FFT                      _fft;
    juce::dsp::WindowingFunction<float> _window;
    std::vector<float> _circularBuffer;  // size = _fftSize
    std::vector<float> _fftBuffer;       // size = 2 * _fftSize (JUCE FFT API requirement)
    int   _writePos { 0 };
    float _hopDecay { 0.984f };

    std::array<float, 12>  _chroma {};
    mutable juce::SpinLock _chromaLock;

    YINDetector            _yin;
    float                  _detectedPitch { 0.0f };
    mutable juce::SpinLock _pitchLock;

    void runFFT();
    void updateChroma();
    int  freqToPitchClass(float freqHz) const;
};
