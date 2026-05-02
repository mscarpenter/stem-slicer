add_library(StemSlicer_UI STATIC
    MainComponent.cpp
)

target_include_directories(StemSlicer_UI
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(StemSlicer_UI
    PUBLIC
        StemSlicer_AudioEngine
        juce::juce_gui_basics
        juce::juce_audio_formats
)
