#include "YINDetector.h"
#include <algorithm>
#include <cmath>

YINDetector::YINDetector(double sampleRate, int windowSize, float threshold)
    : _sampleRate(sampleRate)
    , _windowSize(windowSize)
    , _threshold(threshold)
    , _ringBuffer(static_cast<size_t>(windowSize), 0.0f)
    , _yinBuffer(static_cast<size_t>(windowSize / 2), 0.0f)
{
}

float YINDetector::process(const float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        _ringBuffer[static_cast<size_t>(_writePos++)] = samples[i];
        if (_writePos == _windowSize)
        {
            _lastPitch = runYIN();
            // 50% hop: slide second half to front
            std::copy(_ringBuffer.begin() + _windowSize / 2,
                      _ringBuffer.end(),
                      _ringBuffer.begin());
            _writePos = _windowSize / 2;
        }
    }
    return _lastPitch;
}

// ─── private ──────────────────────────────────────────────────────────────────

float YINDetector::runYIN()
{
    computeDifference();
    computeCMNDF();

    int halfSize = static_cast<int>(_yinBuffer.size());
    int tauMin   = std::max(2, static_cast<int>(std::ceil(_sampleRate / maxFrequency)));
    int tauMax   = std::min(halfSize - 2,
                            static_cast<int>(std::floor(_sampleRate / minFrequency)));

    // Step 3: find first τ whose CMNDF dips below threshold, then slide to local min
    int tau = tauMin;
    while (tau <= tauMax)
    {
        if (_yinBuffer[static_cast<size_t>(tau)] < _threshold)
        {
            while (tau + 1 <= tauMax
                   && _yinBuffer[static_cast<size_t>(tau + 1)]
                      < _yinBuffer[static_cast<size_t>(tau)])
                ++tau;
            break;
        }
        ++tau;
    }
    if (tau > tauMax)
        return 0.0f;  // no clear pitch found

    // Step 4: parabolic interpolation for sub-sample accuracy
    float refined = parabolicInterpolation(tau);
    return static_cast<float>(_sampleRate) / refined;
}

void YINDetector::computeDifference()
{
    // d(τ) = Σ_{j=0}^{halfSize-1} (x[j] - x[j+τ])²
    int halfSize = static_cast<int>(_yinBuffer.size());
    _yinBuffer[0] = 0.0f;
    for (int tau = 1; tau < halfSize; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < halfSize; ++j)
        {
            float d = _ringBuffer[static_cast<size_t>(j)]
                    - _ringBuffer[static_cast<size_t>(j + tau)];
            sum += d * d;
        }
        _yinBuffer[static_cast<size_t>(tau)] = sum;
    }
}

void YINDetector::computeCMNDF()
{
    // d'(0) = 1;  d'(τ) = τ · d(τ) / Σ_{j=1}^{τ} d(j)
    int halfSize = static_cast<int>(_yinBuffer.size());
    _yinBuffer[0] = 1.0f;
    float runningSum = 0.0f;
    for (int tau = 1; tau < halfSize; ++tau)
    {
        runningSum += _yinBuffer[static_cast<size_t>(tau)];
        _yinBuffer[static_cast<size_t>(tau)] =
            (runningSum > 0.0f)
            ? _yinBuffer[static_cast<size_t>(tau)] * static_cast<float>(tau) / runningSum
            : 1.0f;
    }
}

float YINDetector::parabolicInterpolation(int tau) const
{
    int halfSize = static_cast<int>(_yinBuffer.size());
    if (tau <= 0 || tau >= halfSize - 1)
        return static_cast<float>(tau);

    float s0 = _yinBuffer[static_cast<size_t>(tau - 1)];
    float s1 = _yinBuffer[static_cast<size_t>(tau)];
    float s2 = _yinBuffer[static_cast<size_t>(tau + 1)];

    float denom = s0 + s2 - 2.0f * s1;
    if (std::abs(denom) < 1e-7f)
        return static_cast<float>(tau);

    // x = -b/(2a); clamped to ±0.5 to stay within the local bin
    float correction = 0.5f * (s0 - s2) / denom;
    correction = std::max(-0.5f, std::min(0.5f, correction));
    return static_cast<float>(tau) + correction;
}
