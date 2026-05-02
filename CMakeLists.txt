cmake_minimum_required(VERSION 3.22)
project(StemSlicer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── JUCE (fetched at configure time) ─────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ── ONNX Runtime ──────────────────────────────────────────────────────────────
# Requires vcpkg: cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
# vcpkg install: see vcpkg.json
find_package(onnxruntime CONFIG REQUIRED)

add_subdirectory(Source)
