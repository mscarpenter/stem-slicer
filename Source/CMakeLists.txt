add_subdirectory(AudioEngine)
add_subdirectory(ML)
add_subdirectory(UI)

juce_add_gui_app(StemSlicer
    PRODUCT_NAME      "StemSlicer"
    COMPANY_NAME      "MySide"
    BUNDLE_ID         "com.myside.stemslicer"
    NEEDS_CURL        FALSE
    NEEDS_WEB_BROWSER FALSE
)

target_sources(StemSlicer PRIVATE Main.cpp)

target_compile_definitions(StemSlicer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(StemSlicer
    PRIVATE
        StemSlicer_AudioEngine
        StemSlicer_UI
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
