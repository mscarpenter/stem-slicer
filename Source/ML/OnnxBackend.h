#pragma once

#include "InferenceBackend.h"
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <string>
#include <vector>

namespace ml {

class OnnxBackend : public InferenceBackend {
public:
    OnnxBackend();
    ~OnnxBackend() override = default;

    OnnxBackend(const OnnxBackend&)            = delete;
    OnnxBackend& operator=(const OnnxBackend&) = delete;

    void loadModel(const std::filesystem::path& modelPath) override;

    // Throws Ort::Exception on inference failure.
    // inputShape must match the model's expected input dimensions exactly.
    std::vector<std::vector<float>> runInference(
        const std::vector<float>& inputTensor,
        const std::vector<int64_t>& inputShape) override;

private:
    Ort::Env            env_;
    Ort::SessionOptions sessionOptions_;
    std::unique_ptr<Ort::Session> session_;

    std::vector<std::string> inputNames_;
    std::vector<std::string> outputNames_;
};

} // namespace ml
