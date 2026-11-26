cmake_minimum_required(VERSION 3.22)
project(VocalChainStudio VERSION 0.2.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VocalChainStudio
    COMPANY_NAME "TuNombre"
    PLUGIN_MANUFACTURER_CODE Tnom
    PLUGIN_CODE Vcst
    FORMATS VST3 Standalone
    PRODUCT_NAME "Vocal Chain Studio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE)

target_sources(VocalChainStudio PRIVATE
    Source/BuiltinEffect.h  Source/BuiltinEffect.cpp
    Source/ChainProcessor.h Source/ChainProcessor.cpp
    Source/ChainAssistant.cpp
    Source/ChainEditor.h    Source/ChainEditor.cpp)

target_compile_definitions(VocalChainStudio PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1)

target_compile_options(VocalChainStudio PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(VocalChainStudio
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
