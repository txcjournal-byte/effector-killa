cmake_minimum_required(VERSION 3.22)

project(EffectorKilla VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(EK_BUILD_TESTS "Build unit tests" ON)
option(EK_BUILD_TOOLS "Build the UI snapshot tool" OFF)

# ---------------------------------------------------------------------------
# JUCE (pinned tag). A local checkout can be used with
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(EK_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND EK_FORMATS AU)
endif()

juce_add_plugin(EffectorKilla
    COMPANY_NAME                "Killa"
    COMPANY_WEBSITE             "https://killa.audio"
    BUNDLE_ID                   "audio.killa.effectorkilla"
    PLUGIN_MANUFACTURER_CODE    Klla
    PLUGIN_CODE                 EfKl
    PRODUCT_NAME                "Effector Killa"
    DESCRIPTION                 "Every effect. One Killa."
    FORMATS                     ${EK_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect)

# Graphic assets: reference.png is the cabinet placeholder. Drop in Resources/Design/cabinet.png and
# fonts (Resources/Fonts/osd.ttf, vfd.ttf, hand.ttf) and they are picked up automatically.
set(EK_DATA Resources/Presets/factory.json Resources/Design/reference.png)
set(EK_DATA_DEFS)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Design/cabinet.png")
    list(APPEND EK_DATA Resources/Design/cabinet.png)
    list(APPEND EK_DATA_DEFS EK_HAS_CABINET=1)
endif()
file(GLOB EK_FONT_FILES CONFIGURE_DEPENDS Resources/Fonts/*.ttf Resources/Fonts/*.otf)
list(APPEND EK_DATA ${EK_FONT_FILES})
juce_add_binary_data(EffectorKillaData
    HEADER_NAME EKBinaryData.h
    NAMESPACE   EKData
    SOURCES     ${EK_DATA})
target_compile_definitions(EffectorKillaData INTERFACE ${EK_DATA_DEFS})

# Engine sources are shared by the plugin and the test runner.
file(GLOB_RECURSE EK_ENGINE_SOURCES CONFIGURE_DEPENDS Source/engine/*.cpp)

file(GLOB EK_UI_SOURCES CONFIGURE_DEPENDS
    Source/ui/*.cpp Source/ui/cabinet/*.cpp Source/ui/tv/*.cpp)

target_sources(EffectorKilla PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${EK_ENGINE_SOURCES}
    ${EK_UI_SOURCES})

target_include_directories(EffectorKilla PRIVATE Source)

target_compile_definitions(EffectorKilla PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(EffectorKilla
    PRIVATE
        EffectorKillaData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests (console app built on juce::UnitTest)
# ---------------------------------------------------------------------------
if(EK_BUILD_TESTS)
    juce_add_console_app(EffectorKillaTests PRODUCT_NAME "EffectorKillaTests")
    file(GLOB EK_TEST_SOURCES CONFIGURE_DEPENDS Tests/*.cpp)
    target_sources(EffectorKillaTests PRIVATE ${EK_TEST_SOURCES} ${EK_ENGINE_SOURCES} Source/PluginProcessor.cpp)
    target_include_directories(EffectorKillaTests PRIVATE Source Tests)
    target_compile_definitions(EffectorKillaTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 EK_TESTS=1)
    target_link_libraries(EffectorKillaTests PRIVATE
        EffectorKillaData
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    enable_testing()
    add_test(NAME EffectorKillaTests COMMAND EffectorKillaTests)
endif()

# ---------------------------------------------------------------------------
# UI snapshot tool (renders the editor to PNG, software TV)
# ---------------------------------------------------------------------------
if(EK_BUILD_TOOLS)
    juce_add_console_app(EffectorKillaSnapshot PRODUCT_NAME "EffectorKillaSnapshot")
    target_sources(EffectorKillaSnapshot PRIVATE Tools/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp
        ${EK_ENGINE_SOURCES} ${EK_UI_SOURCES})
    target_include_directories(EffectorKillaSnapshot PRIVATE Source)
    target_compile_definitions(EffectorKillaSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(EffectorKillaSnapshot PRIVATE EffectorKillaData juce::juce_audio_utils juce::juce_dsp juce::juce_opengl
        juce::juce_recommended_config_flags)
endif()
