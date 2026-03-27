cmake_minimum_required(VERSION 3.20)
project(shadertoy_test)

set(CMAKE_CXX_STANDARD 26)

find_package(SFML COMPONENTS Network Graphics Window Audio System CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

add_executable(shadertoy_test
    src/main.cpp
    src/shadertoy_emulator.cpp
    src/shader_config.cpp
    src/glsl_preprocessor.cpp
    src/sound_stream.cpp
)
target_include_directories(shadertoy_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(shadertoy_test PRIVATE
    SFML::Network SFML::Graphics SFML::Window SFML::Audio SFML::System
    cxxopts::cxxopts
    nlohmann_json::nlohmann_json
    glad::glad
)
