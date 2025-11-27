cmake_minimum_required(VERSION 3.25)

project(ReactionDiffusionModel CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JSON
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)

# Find macOS Frameworks
find_library(COCOA_LIB Cocoa)
find_library(METAL_LIB Metal)
find_library(QUARTZ_LIB QuartzCore)
find_library(FOUNDATION_LIB Foundation)

# Setup Metal Shader Compilation
set(SHADER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Shaders.metal")
set(SHADER_LIB "${CMAKE_BINARY_DIR}/default.metallib") # Output to build folder for build sys to find it

# Custom command, compile .metal -> .air -> .metallib
add_custom_command(
    OUTPUT ${SHADER_LIB}
    COMMAND xcrun -sdk macosx metal -c ${SHADER_SRC} -o ${CMAKE_CURRENT_BINARY_DIR}/Shaders.air
    COMMAND xcrun -sdk macosx metallib ${CMAKE_CURRENT_BINARY_DIR}/Shaders.air -o ${SHADER_LIB}
    DEPENDS ${SHADER_SRC}
    COMMENT "Compiling Metal Shaders"
)

# Create a target for the shaders so the app depends on them
add_custom_target(MetalShaders DEPENDS ${SHADER_LIB})

# Define the Executable
add_executable(ReactionDiffusionModel
    main.mm
    Renderer.cpp
    Renderer.hpp
    Config.hpp
)

# Link the system frameworks and JSON library
target_link_libraries(ReactionDiffusionModel
    PRIVATE
    nlohmann_json::nlohmann_json
    ${COCOA_LIB}
    ${METAL_LIB}
    ${QUARTZ_LIB}
    ${FOUNDATION_LIB}
)

# Ensure shaders are built before the executable
add_dependencies(ReactionDiffusionModel MetalShaders)

# Add the current directory (for headers) and the metal-cpp folder
target_include_directories(ReactionDiffusionModel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/metal-cpp
)