cmake_minimum_required(VERSION 3.21)
project(pink_eLFOnts VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

# ---- JUCE (choose one) ----
include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8   # or newer 8.x
)
FetchContent_MakeAvailable(juce)

add_compile_definitions(JUCE_VST3_CAN_REPLACE_VST2=0)

juce_add_plugin(pink_eLFOnts
    COMPANY_NAME        "YourName"
    PRODUCT_NAME        "pink eLFOnts"
    BUNDLE_ID           com.yourname.pinkelfonts
    FORMATS             VST3 AU            # no Standalone
    IS_SYNTH            TRUE
    NEEDS_MIDI_INPUT    TRUE               # for retrig
    NEEDS_MIDI_OUTPUT   FALSE
    IS_MIDI_EFFECT      FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

juce_generate_juce_header(pink_eLFOnts)

target_sources(pink_eLFOnts PRIVATE
    source/PluginProcessor.cpp
    source/PluginProcessor.h
    source/PluginEditor.cpp
    source/PluginEditor.h
    source/SynthEngine.cpp
    source/SynthEngine.h
    source/LookAndFeel.h
    source/LookAndFeel.cpp)

target_link_libraries(pink_eLFOnts PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp)
