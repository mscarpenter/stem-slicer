add_library(StemSlicer_ML STATIC
    OnnxBackend.cpp
)

target_include_directories(StemSlicer_ML
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(StemSlicer_ML
    PUBLIC
        onnxruntime::onnxruntime
)
