add_library(StemSlicer_AudioEngine STATIC
    StemSeparator.cpp
    AudioFileReader.cpp
    AudioFileWriter.cpp
    SeparationThread.cpp
)

target_include_directories(StemSlicer_AudioEngine
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(StemSlicer_AudioEngine
    PUBLIC
        StemSlicer_ML
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_dsp
)
