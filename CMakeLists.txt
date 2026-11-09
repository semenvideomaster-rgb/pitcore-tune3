cmake_minimum_required(VERSION 3.22)
project(PitCoreTune VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PitCoreTune
    COMPANY_NAME "PitCore"
    PLUGIN_MANUFACTURER_CODE Pitc
    PLUGIN_CODE Ptun
    FORMATS VST3
    PRODUCT_NAME "Pit Core Tune"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Pitch Shift
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PitCoreTune PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h Source/AutoTuneCore.h)
target_compile_definitions(PitCoreTune PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(PitCoreTune
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
