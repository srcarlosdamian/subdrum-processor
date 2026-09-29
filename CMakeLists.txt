cmake_minimum_required(VERSION 3.22)

project(SubdrumProcessor VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent for JUCE dependency management if not provided locally
include(FetchContent)

# Option to use local JUCE or fetch automatically
if(NOT TARGET juce::juce_audio_processors)
    message(STATUS "Fetching JUCE from GitHub...")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Plugin Target Configuration
juce_add_plugin(SubdrumProcessor
    COMPANY_NAME "UndergroundDSP"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ugnd
    PLUGIN_CODE Sbdp
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Subdrum Processor"
)

# Source files
target_sources(SubdrumProcessor
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/DSP/DrumSynth.h
        Source/DSP/SamplePlayer.h
        Source/DSP/StepSequencer.h
        Source/DSP/TapeSaturation.h
        Source/DSP/SamplerFilter.h
        Source/DSP/VCACompressor.h
        Source/DSP/VinylNoise.h
)

# Target compile definitions & include dirs
target_include_directories(SubdrumProcessor
    PRIVATE
        Source
        Source/DSP
)

target_compile_definitions(SubdrumProcessor
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Link JUCE Modules
target_link_libraries(SubdrumProcessor
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
)
