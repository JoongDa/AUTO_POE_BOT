cmake_minimum_required(VERSION 3.25)

project(poe-bot
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "POE automation tool - C++ rewrite of AHK bot"
)

# ---------------- Global C++ settings ----------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Flatten output directories so binaries land under build/bin/{Debug,Release}
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------------- Compiler flags ----------------
if(MSVC)
    add_compile_options(
        /utf-8              # source + exec charset = UTF-8
        /W4                 # warning level 4
        /permissive-        # strict conformance
        /Zc:__cplusplus     # report correct __cplusplus value
        /Zc:preprocessor    # conforming preprocessor
        /MP                 # multi-process compilation
    )
    add_compile_definitions(
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

# ---------------- Dependencies (vcpkg) ----------------
find_package(imgui CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ---------------- Subdirectories ----------------
add_subdirectory(core)
add_subdirectory(gui)
