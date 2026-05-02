#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ml {

class InferenceBackend {
public:
    virtual ~InferenceBackend() = default;

    virtual void loadModel(const std::filesystem::path& modelPath) = 0;

    // inputTensor: flat STFT magnitude spectrogram in NHWC layout.
    // inputShape:  dimension sizes matching the tensor layout.
    // Returns one float vector per output node — one per stem for Spleeter 4stems.
    virtual std::vector<std::vector<float>> runInference(
        const std::vector<float>& inputTensor,
        const std::vector<int64_t>& inputShape) = 0;
};

} // namespace ml
