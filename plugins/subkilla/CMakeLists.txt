cmake_minimum_required(VERSION 3.22)

# Single place for the version
project(TRAPWIN_SUBKILLA VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS: universal binary, macOS 10.13+
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# Static MSVC runtime -> no VC++ redistributable needed
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(SUBKILLA_BUILD_PLUGIN "Build the plugin (needs JUCE)" ON)
option(SUBKILLA_BUILD_TESTS "Build the headless DSP tests" ON)

# Headless engine tests (plain C++, no JUCE)
if(SUBKILLA_BUILD_TESTS)
    add_executable(SubKillaTests tests/EngineTests.cpp Source/DSP/SubEngine.cpp)
    target_include_directories(SubKillaTests PRIVATE Source)
endif()

if(NOT SUBKILLA_BUILD_PLUGIN)
    return()
endif()

# JUCE: local checkout (-DJUCE_SOURCE_DIR=...) or download
set(JUCE_SOURCE_DIR "" CACHE PATH "Optional local JUCE checkout")
if(JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.15.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Plugin + manufacturer codes are fixed forever: TwSk / Trpw
juce_add_plugin(SubKilla
    COMPANY_NAME "TrapWin"
    COMPANY_WEBSITE "https://trapwin.com"
    BUNDLE_ID com.trapwin.subkilla
    PLUGIN_MANUFACTURER_CODE Trpw
    PLUGIN_CODE TwSk
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "TrapWin SUB KILLA"
    DESCRIPTION "808 & sub-bass synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(SubKillaAssets SOURCES
    Resources/bg.png
    Resources/knob_big.png
    Resources/knob_small.png
    Resources/fonts/ArchivoBlack-Regular.ttf
    Resources/fonts/BarlowCondensed-SemiBold.ttf
    Resources/fonts/BarlowCondensed-Bold.ttf
    Resources/fonts/ShareTechMono-Regular.ttf)

target_sources(SubKilla PRIVATE
    Source/DSP/SubEngine.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(SubKilla PRIVATE Source)

target_compile_definitions(SubKilla PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(SubKilla
    PRIVATE
        SubKillaAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Store screenshot renderer (dev tool)
option(SUBKILLA_BUILD_TOOLS "Build dev tools" OFF)
if(SUBKILLA_BUILD_TOOLS)
    juce_add_console_app(SubKillaSnapshot PRODUCT_NAME "SubKillaSnapshot")
    target_sources(SubKillaSnapshot PRIVATE tools/Snapshot.cpp)
    target_include_directories(SubKillaSnapshot PRIVATE Source)
    target_compile_definitions(SubKillaSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(SubKillaSnapshot PRIVATE SubKilla SubKillaAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
