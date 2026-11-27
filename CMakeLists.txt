cmake_minimum_required(VERSION 3.22)
project(MultiverseX VERSION 0.3.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Video on the stage strip (DirectShow, works on Windows 7/8). If a build ever fails inside
# juce_video, turn this OFF (cmake -DMVX_VIDEO=OFF) - everything else keeps working.
option(MVX_VIDEO "Enable video playback on the stage strip" OFF)

include(FetchContent)
# JUCE 7.0.12 = last line that still targets older Windows; JUCE 8 needs Win10+
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 7.0.12)
FetchContent_MakeAvailable(JUCE)

set(MVX_SOURCES
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/Vfx.cpp Source/VfxScenes.cpp)

# Builds the same source twice:
#   MultiverseX    = instrument (synth + sampler + FX rack)
#   MultiverseXFX  = effect     (process any audio through the FX rack; also plays the synth from MIDI)
function(mvx_plugin TARGET PRODUCT CODE IS_SYNTH)
    juce_add_plugin(${TARGET}
        COMPANY_NAME "Hazel Bandz"
        PLUGIN_MANUFACTURER_CODE HzBz
        PLUGIN_CODE ${CODE}
        FORMATS VST3
        PRODUCT_NAME "${PRODUCT}"
        IS_SYNTH ${IS_SYNTH}
        NEEDS_MIDI_INPUT TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(${TARGET} PRIVATE ${MVX_SOURCES})

    target_compile_definitions(${TARGET} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0 _WIN32_WINNT=0x0601)

    target_link_libraries(${TARGET} PRIVATE
        juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    if(MVX_VIDEO)
        target_compile_definitions(${TARGET} PUBLIC MVX_VIDEO=1 JUCE_DIRECTSHOW=1)
        target_link_libraries(${TARGET} PRIVATE juce::juce_video)
    else()
        target_compile_definitions(${TARGET} PUBLIC MVX_VIDEO=0)
    endif()
endfunction()

mvx_plugin(MultiverseX   "MultiverseX"   MvXx TRUE)
mvx_plugin(MultiverseXFX "MultiverseXFX" MvXf FALSE)
