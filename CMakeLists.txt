cmake_minimum_required(VERSION 3.22)
project(ChoirCrowd VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ChoirCrowd
    COMPANY_NAME "CrowdLab"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Cwdl
    PLUGIN_CODE Chcr
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Choir Crowd"
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(ChoirCrowd PRIVATE Source/PluginProcessor.cpp Source/PluginProcessor.h)

target_compile_definitions(ChoirCrowd PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ChoirCrowd
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
