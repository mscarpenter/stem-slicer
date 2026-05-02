#include "OnnxBackend.h"
#include <numeric>
#include <stdexcept>

namespace ml {

OnnxBackend::OnnxBackend()
    : env_(ORT_LOGGING_LEVEL_WARNING, "StemSlicer")
{
    sessionOptions_.SetIntraOpNumThreads(1);
    sessionOptions_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
}

void OnnxBackend::loadModel(const std::filesystem::path& modelPath)
{
    if (!std::filesystem::exists(modelPath))
        throw std::runtime_error("ONNX model not found: " + modelPath.string());

    Ort::AllocatorWithDefaultOptions allocator;

#ifdef _WIN32
    session_ = std::make_unique<Ort::Session>(env_, modelPath.wstring().c_str(), sessionOptions_);
#else
    session_ = std::make_unique<Ort::Session>(env_, modelPath.string().c_str(), sessionOptions_);
#endif

    const size_t inputCount = session_->GetInputCount();
    inputNames_.clear();
    inputNames_.reserve(inputCount);
    for (size_t i = 0; i < inputCount; ++i)
        inputNames_.emplace_back(session_->GetInputNameAllocated(i, allocator).get());

    const size_t outputCount = session_->GetOutputCount();
    outputNames_.clear();
    outputNames_.reserve(outputCount);
    for (size_t i = 0; i < outputCount; ++i)
        outputNames_.emplace_back(session_->GetOutputNameAllocated(i, allocator).get());
}

std::vector<std::vector<float>> OnnxBackend::runInference(
    const std::vector<float>& inputTensor,
    const std::vector<int64_t>& inputShape)
{
    if (!session_)
        throw std::runtime_error("OnnxBackend: call loadModel() before runInference()");

    const size_t expectedElements = std::accumulate(
        inputShape.begin(), inputShape.end(),
        size_t{1}, std::multiplies<size_t>{});
    if (inputTensor.size() != expectedElements)
        throw std::invalid_argument(
            "OnnxBackend: inputTensor size " + std::to_string(inputTensor.size()) +
            " != inputShape product " + std::to_string(expectedElements));

    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    Ort::Value inputValue = Ort::Value::CreateTensor<float>(
        memoryInfo,
        const_cast<float*>(inputTensor.data()),
        inputTensor.size(),
        inputShape.data(),
        inputShape.size()
    );

    std::vector<const char*> inputPtrs;
    inputPtrs.reserve(inputNames_.size());
    for (const auto& name : inputNames_)
        inputPtrs.push_back(name.c_str());

    std::vector<const char*> outputPtrs;
    outputPtrs.reserve(outputNames_.size());
    for (const auto& name : outputNames_)
        outputPtrs.push_back(name.c_str());

    auto outputValues = session_->Run(
        Ort::RunOptions{ nullptr },
        inputPtrs.data(),
        &inputValue,
        inputPtrs.size(),
        outputPtrs.data(),
        outputPtrs.size()
    );

    std::vector<std::vector<float>> stems;
    stems.reserve(outputValues.size());
    for (auto& ortValue : outputValues)
    {
        const float* data  = ortValue.GetTensorData<float>();
        const size_t count = ortValue.GetTensorTypeAndShapeInfo().GetElementCount();
        stems.emplace_back(data, data + count);
    }

    return stems;
}

} // namespace ml
