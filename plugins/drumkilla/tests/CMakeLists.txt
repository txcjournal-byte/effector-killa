juce_add_console_app(K808Tests PRODUCT_NAME "K808Tests")

target_sources(K808Tests PRIVATE Tests.cpp)
target_include_directories(K808Tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)
target_compile_definitions(K808Tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

# links against the plugin's shared code (processor, engine, presets)
target_link_libraries(K808Tests PRIVATE
    K808
    K808Assets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags)
