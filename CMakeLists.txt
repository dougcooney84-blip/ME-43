cmake_minimum_required(VERSION 3.22)

project(StageASynth VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)
include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")

# Replit's Nix environment exposes dependency include paths through this
# generic variable, while the compiler wrapper only consumes architecture-
# qualified variants.
if(DEFINED ENV{NIX_CFLAGS_COMPILE})
    string(APPEND CMAKE_C_FLAGS " $ENV{NIX_CFLAGS_COMPILE}")
    string(APPEND CMAKE_CXX_FLAGS " $ENV{NIX_CFLAGS_COMPILE}")
endif()

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(StageASynth
    COMPANY_NAME "Independent"
    BUNDLE_ID "audio.independent.me43"
    PLUGIN_MANUFACTURER_CODE Indp
    PLUGIN_CODE StgA
    PRODUCT_NAME "ME - 43"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 Standalone
)

target_sources(StageASynth PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
)

target_compile_definitions(StageASynth PUBLIC
    JUCE_WEB
