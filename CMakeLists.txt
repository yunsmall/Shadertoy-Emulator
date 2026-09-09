cmake_minimum_required(VERSION 3.20)
project(ShadertoyEmulator)

set(CMAKE_CXX_STANDARD 26)

find_package(SFML COMPONENTS Network Graphics Window Audio System CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/shadertoy_emulator.cpp
    src/shader_config.cpp
    src/glsl_preprocessor.cpp
    src/sound_stream.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE
    SFML::Network SFML::Graphics SFML::Window SFML::Audio SFML::System
    ImGui-SFML::ImGui-SFML
    cxxopts::cxxopts
    nlohmann_json::nlohmann_json
    glad::glad
)
