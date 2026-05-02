include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# ── test_key_estimator (no JUCE dependency) ──────────────────────────────────
add_executable(test_key_estimator
    test_key_estimator.cpp
    ${CMAKE_SOURCE_DIR}/src/KeyEstimator.cpp
)
target_link_libraries(test_key_estimator PRIVATE Catch2::Catch2WithMain)
target_include_directories(test_key_estimator PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(test_key_estimator PRIVATE cxx_std_17)

# ── test_scale_classifier (no JUCE dependency) ───────────────────────────────
add_executable(test_scale_classifier
    test_scale_classifier.cpp
    ${CMAKE_SOURCE_DIR}/src/KeyEstimator.cpp
    ${CMAKE_SOURCE_DIR}/src/ScaleClassifier.cpp
)
target_link_libraries(test_scale_classifier PRIVATE Catch2::Catch2WithMain)
target_include_directories(test_scale_classifier PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(test_scale_classifier PRIVATE cxx_std_17)

# ── test_midi_analyzer (requires juce_audio_basics for MidiBuffer) ───────────
# juce_add_console_app compiles JUCE modules properly for a standalone executable
juce_add_console_app(test_midi_analyzer PRODUCT_NAME "test_midi_analyzer")

target_sources(test_midi_analyzer PRIVATE
    test_midi_analyzer.cpp
    ${CMAKE_SOURCE_DIR}/src/MidiAnalyzer.cpp
    ${CMAKE_SOURCE_DIR}/src/KeyEstimator.cpp
    ${CMAKE_SOURCE_DIR}/src/ScaleClassifier.cpp
)
target_link_libraries(test_midi_analyzer PRIVATE
    Catch2::Catch2          # without main — we provide our own in test_midi_analyzer.cpp
    juce::juce_audio_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
target_compile_definitions(test_midi_analyzer PRIVATE
    JUCE_STANDALONE_APPLICATION=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_include_directories(test_midi_analyzer PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME KeyEstimator    COMMAND test_key_estimator)
add_test(NAME ScaleClassifier COMMAND test_scale_classifier)
add_test(NAME MidiAnalyzer    COMMAND test_midi_analyzer)

# ── test_progression_suggester (requires juce_core for JSON parsing) ──────────
juce_add_console_app(test_progression_suggester PRODUCT_NAME "test_progression_suggester")

target_sources(test_progression_suggester PRIVATE
    test_progression_suggester.cpp
    ${CMAKE_SOURCE_DIR}/src/ProgressionSuggester.cpp
)
target_link_libraries(test_progression_suggester PRIVATE
    Catch2::Catch2          # without main — we provide our own
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
target_compile_definitions(test_progression_suggester PRIVATE
    JUCE_STANDALONE_APPLICATION=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_include_directories(test_progression_suggester PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME ProgressionSuggester COMMAND test_progression_suggester)

# ── test_profile_fuser (no JUCE dependency) ──────────────────────────────────
add_executable(test_profile_fuser
    test_profile_fuser.cpp
    ${CMAKE_SOURCE_DIR}/src/ProfileFuser.cpp
)
target_link_libraries(test_profile_fuser PRIVATE Catch2::Catch2WithMain)
target_include_directories(test_profile_fuser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(test_profile_fuser PRIVATE cxx_std_17)

add_test(NAME ProfileFuser COMMAND test_profile_fuser)

# ── test_yin_detector (no JUCE dependency) ───────────────────────────────────
add_executable(test_yin_detector
    test_yin_detector.cpp
    ${CMAKE_SOURCE_DIR}/src/YINDetector.cpp
)
target_link_libraries(test_yin_detector PRIVATE Catch2::Catch2WithMain)
target_include_directories(test_yin_detector PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(test_yin_detector PRIVATE cxx_std_17)

add_test(NAME YINDetector COMMAND test_yin_detector)

# ── test_audio_analyzer (requires juce_dsp for FFT + windowing) ───────────────
juce_add_console_app(test_audio_analyzer PRODUCT_NAME "test_audio_analyzer")

target_sources(test_audio_analyzer PRIVATE
    test_audio_analyzer.cpp
    ${CMAKE_SOURCE_DIR}/src/AudioAnalyzer.cpp
    ${CMAKE_SOURCE_DIR}/src/YINDetector.cpp
)
target_link_libraries(test_audio_analyzer PRIVATE
    Catch2::Catch2          # without main — we provide our own
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
target_compile_definitions(test_audio_analyzer PRIVATE
    JUCE_STANDALONE_APPLICATION=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_include_directories(test_audio_analyzer PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME AudioAnalyzer COMMAND test_audio_analyzer)

# ── test_e2e_pipeline (full MIDI→key→scale→progression chain) ────────────────
# Needs juce_audio_basics (MidiAnalyzer) + juce_core (ProgressionSuggester JSON)
juce_add_console_app(test_e2e_pipeline PRODUCT_NAME "test_e2e_pipeline")

target_sources(test_e2e_pipeline PRIVATE
    test_e2e_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/MidiAnalyzer.cpp
    ${CMAKE_SOURCE_DIR}/src/KeyEstimator.cpp
    ${CMAKE_SOURCE_DIR}/src/ScaleClassifier.cpp
    ${CMAKE_SOURCE_DIR}/src/ProfileFuser.cpp
    ${CMAKE_SOURCE_DIR}/src/ProgressionSuggester.cpp
)
target_link_libraries(test_e2e_pipeline PRIVATE
    Catch2::Catch2
    juce::juce_audio_basics
    juce::juce_core
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
target_compile_definitions(test_e2e_pipeline PRIVATE
    JUCE_STANDALONE_APPLICATION=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_include_directories(test_e2e_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME E2EPipeline COMMAND test_e2e_pipeline)
