juce_add_binary_data(SuppressorAssets
    NAMESPACE SuppressorAssets
    HEADER_NAME SuppressorAssets.h
    SOURCES
        ../assets/fonts/BarlowCondensed-Regular.ttf
        ../assets/fonts/BarlowCondensed-SemiBold.ttf)

juce_add_plugin(Suppressor
    VERSION 0.1.0
    COMPANY_NAME "Suppressor Audio"
    COMPANY_WEBSITE "https://github.com/user1303836/suppressor"
    COMPANY_EMAIL "dev@suppressor.audio"
    PLUGIN_MANUFACTURER_CODE SpAu
    PLUGIN_CODE Supr
    FORMATS VST3 Standalone
    PRODUCT_NAME "suppressor"
    DESCRIPTION "Zero-latency frequency-split noise suppressor for guitar DI"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics)

target_sources(Suppressor PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    PluginLookAndFeel.cpp)

target_compile_definitions(Suppressor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Suppressor PRIVATE
    SuppressorAssets
    suppressor-dsp
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

target_compile_features(Suppressor PRIVATE cxx_std_17)

if (SUPPRESSOR_ENABLE_CLAP)
    clap_juce_extensions_plugin(TARGET Suppressor
        CLAP_ID "audio.suppressor.suppressor"
        CLAP_FEATURES audio-effect
        CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 16
        CLAP_ALWAYS_JUCE_BRIDGE TRUE)
endif()

if (SUPPRESSOR_BUILD_TESTS)
    add_executable(SuppressorUITests)

    target_sources(SuppressorUITests PRIVATE
        ../Tests/test_ui.cpp)

    target_include_directories(SuppressorUITests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

    target_link_libraries(SuppressorUITests PRIVATE
        Suppressor
        doctest::doctest_with_main
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    target_compile_definitions(SuppressorUITests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    add_test(NAME suppressor-ui-unit COMMAND SuppressorUITests)
endif()
