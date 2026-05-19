cmake_minimum_required(VERSION 3.14)
project(SystemMonitor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies: httplib, nlohmann_json, gtest
include(FetchContent)

FetchContent_Declare(
        httplib
        URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.14.3.tar.gz
)
FetchContent_MakeAvailable(httplib)

FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
FetchContent_MakeAvailable(googletest)

# Collector executable
add_executable(sysstat_collector
        src/collector/main.cpp
        src/collector/StatsCollector.cpp
)
target_link_libraries(sysstat_collector pthread rt)

# WebServer executable
add_executable(sysstat_webserver
        src/webserver/main.cpp
        src/webserver/WebServer.cpp
)
target_link_libraries(sysstat_webserver pthread rt httplib nlohmann_json)

# Unit tests
add_executable(unit_tests
        src/tests/test_parser.cpp
        src/collector/StatsCollector.cpp
)
target_link_libraries(unit_tests pthread rt gtest gtest_main)