FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(doctest)

add_executable(suppressor-tests
    test_main.cpp
    test_biquad.cpp
    test_gate.cpp
    test_core.cpp
    test_engine.cpp
    test_hum.cpp
    test_multiband.cpp
    test_cue.cpp)

target_link_libraries(suppressor-tests PRIVATE suppressor-dsp doctest::doctest)
target_include_directories(suppressor-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME suppressor-dsp-unit COMMAND suppressor-tests)
