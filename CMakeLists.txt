cmake_minimum_required(VERSION 3.22)
project(MultiverseX VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
# JUCE 7.0.12 = last line that still targets older Windows; JUCE 8 needs Win10+
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 7.0.12)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MultiverseX
    COMPANY_NAME "Hazel Bandz"
    PLUGIN_MANUFACTURER_CODE HzBz
    PLUGIN_CODE MvXx
    FORMATS VST3
    PRODUCT_NAME "MultiverseX"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(MultiverseX PRIVATE
    Source/PluginProcessor.cpp Source/PluginEditor.cpp)

target_compile_definitions(MultiverseX PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 _WIN32_WINNT=0x0601)

target_link_libraries(MultiverseX PRIVATE
    juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
