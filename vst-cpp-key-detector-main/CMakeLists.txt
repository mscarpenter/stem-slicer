cmake_minimum_required(VERSION 3.22)
project(KeyDetectorVST VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS  "Build unit tests"                    ON)
option(FETCH_JUCE   "Fetch JUCE via FetchContent instead of local submodule" OFF)

# ── JUCE ────────────────────────────────────────────────────────────────────
if(FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        7.0.12
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    message(FATAL_ERROR
        "\n[KeyDetector] JUCE not found. Fix with one of:\n"
        "  1. git clone https://github.com/juce-framework/JUCE.git\n"
        "  2. cmake -DFETCH_JUCE=ON ..  (downloads automatically)\n"
    )
endif()

# ── Plugin formats (AU is macOS-only) ───────────────────────────────────────
if(APPLE)
    set(PLUGIN_FORMATS VST3 AU Standalone)
else()
    set(PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(KeyDetector
    PLUGIN_MANUFACTURER_CODE    Mids
    PLUGIN_CODE                 Kdtc
    FORMATS                     ${PLUGIN_FORMATS}
    PRODUCT_NAME                "Key Detector"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

target_sources(KeyDetector PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/NoteLEDBar.cpp
    src/CircleOfFifths.cpp
    src/ChordButton.cpp
    src/MidiAnalyzer.cpp
    src/AudioAnalyzer.cpp
    src/YINDetector.cpp
    src/KeyEstimator.cpp
    src/ScaleClassifier.cpp
    src/ProfileFuser.cpp
    src/ProgressionSuggester.cpp
)

target_include_directories(KeyDetector PRIVATE src)

target_compile_definitions(KeyDetector
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    PRIVATE
        JUCE_DISPLAY_SPLASH_SCREEN=0
        # In Debug builds point directly at the source tree's data/ folder so
        # the plugin finds markov_transitions.json without any manual install.
        # Sprint 12 will replace this with proper bundle-asset resolution.
        $<$<CONFIG:Debug>:PLUGIN_DEV_DATA_DIR="${CMAKE_SOURCE_DIR}/data">
)

target_link_libraries(KeyDetector PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# ── Tests ────────────────────────────────────────────────────────────────────
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
