cmake_minimum_required(VERSION 3.22)
project(ScribaHeadphones VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS minimo")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")

# JUCE: usa una copia locale con -DJUCE_DIR=/percorso/JUCE, altrimenti la scarica da GitHub
set(JUCE_DIR "" CACHE PATH "Cartella di JUCE (opzionale)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(SCRIBA_FORMATS VST3 AU)
else()
    set(SCRIBA_FORMATS VST3)
endif()

juce_add_plugin(ScribaHeadphones
    COMPANY_NAME "Meteora Plugins"
    COMPANY_EMAIL "meteora_plugins@aol.com"
    BUNDLE_ID "com.meteoraplugins.scribaheadphones"
    PLUGIN_MANUFACTURER_CODE Mtra        # usa lo stesso codice produttore degli altri plugin Meteora
    PLUGIN_CODE ScHp
    FORMATS ${SCRIBA_FORMATS}
    PRODUCT_NAME "SCRIBA Headphones"
    PLUGIN_NAME "SCRIBA Headphones"
    DESCRIPTION "Correzione cuffie e crossfeed"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(ScribaHeadphonesData SOURCES Resources/logo.png)

target_sources(ScribaHeadphones PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(ScribaHeadphones PRIVATE Source)

target_compile_definitions(ScribaHeadphones PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ScribaHeadphones
    PRIVATE
        ScribaHeadphonesData
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
