cmake_minimum_required(VERSION 3.16)
project(expresslrs_sender VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(ENABLE_TESTS "Enable unit tests" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
    if(ENABLE_COVERAGE)
        add_compile_options(--coverage)
        add_link_options(--coverage)
    endif()
else()
    add_compile_options(-O2)
endif()

# Find packages
find_package(nlohmann_json 3.9 REQUIRED)
find_package(spdlog REQUIRED)

# Library sources
set(LIB_SOURCES
    src/crsf/crc8.cpp
    src/crsf/crsf.cpp
    src/uart/uart.cpp
    src/history/history_loader.cpp
    src/playback/playback_controller.cpp
    src/safety/safety_monitor.cpp
    src/config/config.cpp
    src/gpio/gpio_uart_map.cpp
)

# Create library
add_library(elrs_lib STATIC ${LIB_SOURCES})
target_include_directories(elrs_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(elrs_lib PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    pthread
)

# Main executable
add_executable(expresslrs_sender src/main.cpp)
target_link_libraries(expresslrs_sender PRIVATE elrs_lib)

# Install
install(TARGETS expresslrs_sender RUNTIME DESTINATION bin)
install(DIRECTORY config/ DESTINATION etc/expresslrs_sender)

# Tests
if(ENABLE_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)

    set(TEST_SOURCES
        tests/test_main.cpp
        tests/test_crc8.cpp
        tests/test_crsf.cpp
        tests/test_history_loader.cpp
        tests/test_playback.cpp
        tests/test_safety.cpp
        tests/test_config.cpp
        tests/test_cli.cpp
        tests/test_gpio_uart_map.cpp
    )

    add_executable(test_expresslrs_sender ${TEST_SOURCES})
    target_link_libraries(test_expresslrs_sender PRIVATE
        elrs_lib
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_expresslrs_sender PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    include(GoogleTest)
    gtest_discover_tests(test_expresslrs_sender)
endif()
