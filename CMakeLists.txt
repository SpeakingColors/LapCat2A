cmake_minimum_required(VERSION 3.22)
# The release workflow rewrites this version from the tag (sed) so menu, installer and release agree.
project(LapCat2A VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(LapAssets SOURCES
    assets/fonts/BarlowCondensed-Medium.ttf
    assets/fonts/BarlowCondensed-SemiBold.ttf
    assets/fonts/BarlowCondensed-Bold.ttf
    assets/fonts/BarlowCondensed-ExtraBoldItalic.ttf
    assets/fonts/JetBrainsMono-Regular.ttf)
set_target_properties(LapAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(LapCat2A
    COMPANY_NAME "Studio 87"
    PLUGIN_MANUFACTURER_CODE Stu8
    PLUGIN_CODE Lap2
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE
    PRODUCT_NAME "LapCat2A")

set(LAP_SOURCES
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginProcessor.h
    src/core/LapCat2A.h
    src/editor/PluginEditor.cpp
    src/editor/PluginEditor.h
    src/editor/Presets.cpp
    src/editor/Presets.h
    src/editor/Dialogs.cpp
    src/editor/Dialogs.h
    src/editor/Theme.h
    src/editor/Layout.h
    src/editor/Widgets.h
    src/editor/Views.h)
target_sources(LapCat2A PRIVATE ${LAP_SOURCES})

target_compile_definitions(LapCat2A PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(LapCat2A
    PRIVATE LapAssets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Offscreen window renderer and self test (not part of releases).
option(LAP_BUILD_SNAPSHOT "Build the editor snapshot tool" OFF)
if(LAP_BUILD_SNAPSHOT)
    juce_add_console_app(LapSnapshot PRODUCT_NAME "LapSnapshot")
    target_sources(LapSnapshot PRIVATE tools/snapshot.cpp ${LAP_SOURCES})
    target_compile_definitions(LapSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="LapCat2A" JucePlugin_VersionString="${PROJECT_VERSION}")
    target_link_libraries(LapSnapshot PRIVATE LapAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
