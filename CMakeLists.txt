cmake_minimum_required(VERSION 3.20)
project(ShadertoyEmulator)

set(CMAKE_CXX_STANDARD 23)

find_package(SFML COMPONENTS Network Graphics Window Audio System CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
# ANGLE 的 shader translator：把 ESSL 翻成桌面 GLSL，顺带做 WebGL 的未初始化
# 变量补零（桌面驱动不保证这个，Shadertoy 的 shader 却依赖它）
find_package(unofficial-angle CONFIG REQUIRED)
# Windows 上用 vcpkg 装的 ffmpeg（自带 FindFFMPEG 模块）；
# Linux 发行版的 libav*-dev 包更省事，不必再编译一遍 ffmpeg，走 pkg-config
find_package(FFMPEG QUIET)
if(NOT FFMPEG_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
        libavcodec libavformat libavutil libswscale libswresample)
    set(FFMPEG_LIBRARIES PkgConfig::FFMPEG)
endif()

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/shadertoy_emulator.cpp
    src/shader_config.cpp
    src/glsl_preprocessor.cpp
    src/sound_stream.cpp
    src/texture_cache.cpp
    src/input_state.cpp
    src/exporter.cpp
    src/render_core.cpp
    src/glsl_translator.cpp
    src/debug_overlay.cpp
    src/video_writer.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE
    SFML::Network SFML::Graphics SFML::Window SFML::Audio SFML::System
    ImGui-SFML::ImGui-SFML
    cxxopts::cxxopts
    nlohmann_json::nlohmann_json
    glad::glad
    unofficial::angle::libANGLE
    ${FFMPEG_LIBRARIES}
)

# ANGLE 的 SimpleMutex 用到 WaitOnAddress/WakeByAddressSingle，这俩在 Windows 的
# synchronization.lib 里
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE synchronization)
endif()
