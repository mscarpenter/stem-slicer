#pragma once
#include <vector>

// Monophonic pitch detector based on the YIN algorithm
// (de Cheveigné & Kawahara, JASA 2002).
//
// Process audio in any block size; results are available after the first
// full window has been accumulated.  A 50% hop is used so the detector
// updates roughly every windowSize/2 samples.
class YINDetector {
public:
    // sampleRate  — audio sample rate in Hz
    // windowSize  — analysis window length in samples (must be even, default 2048)
    // threshold   — CMNDF threshold for onset detection (0.10–0.20 typical)
    explicit YINDetector(double sampleRate,
                         int    windowSize = 2048,
                         float  threshold  = 0.15f);

    // Feed samples; returns the most recent detected pitch in Hz,
    // or 0.0f when no pitch is detected (silence or out of range).
    float process(const float* samples, int numSamples);

    // Pitch search bounds (Hz). Adjust before calling process() if needed.
    float minFrequency { 50.0f };
    float maxFrequency { 2000.0f };

private:
    double _sampleRate;
    int    _windowSize;   // = 2 * halfSize
    float  _threshold;

    std::vector<float> _ringBuffer;  // accumulates incoming samples
    std::vector<float> _yinBuffer;   // working buffer, size = windowSize/2
    int   _writePos  { 0 };
    float _lastPitch { 0.0f };

    float runYIN();
    void  computeDifference();
    void  computeCMNDF();
    float parabolicInterpolation(int tau) const;
};
