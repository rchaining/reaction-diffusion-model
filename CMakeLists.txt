cmake_minimum_required(VERSION 3.25)
project(ReactionDiffusionModel CXX)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)

add_executable(ReactionDiffusionModel main.cpp)

target_link_libraries(ReactionDiffusionModel PRIVATE nlohmann_json::nlohmann_json)


