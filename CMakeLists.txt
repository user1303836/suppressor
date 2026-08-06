cmake_minimum_required(VERSION 3.22)

project(suppressor
    VERSION 0.1.0
    DESCRIPTION "Zero-latency frequency-split noise suppressor for guitar DI"
    LANGUAGES CXX C)

option(SUPPRESSOR_BUILD_PLUGIN "Build VST3/CLAP plug-ins" ON)
option(SUPPRESSOR_BUILD_TESTS  "Build DSP unit tests"   ON)
option(SUPPRESSOR_ENABLE_CLAP  "Build CLAP format"      ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)

# Framework-free DSP core (header-only)
add_library(suppressor-dsp INTERFACE)
target_include_directories(suppressor-dsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source)

if (SUPPRESSOR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

if (SUPPRESSOR_BUILD_PLUGIN)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)

    if (SUPPRESSOR_ENABLE_CLAP)
        FetchContent_Declare(clap-juce-extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG        c1a5ad025f95d01e03267857fa8276ebeed16500
            GIT_SHALLOW    FALSE)
        FetchContent_MakeAvailable(clap-juce-extensions)
    endif()

    add_subdirectory(Source)
endif()
