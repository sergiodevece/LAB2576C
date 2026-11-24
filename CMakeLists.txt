cmake_minimum_required(VERSION 3.22)
project(LAB2576C VERSION 0.1.2 LANGUAGES C CXX)

option(LAB2576C_BUILD_PLUGIN "Build the JUCE plugin for the current platform" ON)
set(LAB2576C_JUCE_PATH "" CACHE PATH "Optional path to a local JUCE 8 source tree")

add_library(lab2576c_dsp STATIC Source/Compressor.cpp)
target_include_directories(lab2576c_dsp PUBLIC Source)
target_compile_features(lab2576c_dsp PUBLIC cxx_std_17)
set_target_properties(lab2576c_dsp PROPERTIES POSITION_INDEPENDENT_CODE YES CXX_EXTENSIONS NO)

include(CTest)
if(BUILD_TESTING)
    add_executable(lab2576c_dsp_tests Tests/CompressorTests.cpp)
    target_link_libraries(lab2576c_dsp_tests PRIVATE lab2576c_dsp)
    add_test(NAME lab2576c_dsp COMMAND lab2576c_dsp_tests)
endif()

if(NOT LAB2576C_BUILD_PLUGIN)
    return()
endif()

if(LAB2576C_JUCE_PATH)
    add_subdirectory("${LAB2576C_JUCE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(plugin_format AU)
else()
    set(plugin_format VST3)
endif()

juce_add_binary_data(LAB2576CSkin
    HEADER_NAME LAB2576CSkin.h
    NAMESPACE Lab2576CSkin
    SOURCES
        Resources/lab_panel.png
        Resources/button_lit.png
        Resources/button_unlit.png
        Resources/BarlowCondensed-SemiBold.ttf)
set_target_properties(LAB2576CSkin PROPERTIES POSITION_INDEPENDENT_CODE YES)

juce_add_plugin(LAB2576C
    PRODUCT_NAME "LAB2576C"
    COMPANY_NAME "VITAPLUG"
    BUNDLE_ID org.serxdev.vitaplug.lab2576c
    PLUGIN_MANUFACTURER_CODE VtLb
    PLUGIN_CODE L257
    AU_EXPORT_PREFIX LAB2576CAU
    VST3_CATEGORIES Fx Dynamics
    FORMATS ${plugin_format}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(LAB2576C PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(LAB2576C PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(LAB2576C PRIVATE
    lab2576c_dsp
    LAB2576CSkin
    juce::juce_audio_processors
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
