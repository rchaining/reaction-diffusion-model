cmake_minimum_required(VERSION 3.25)
project(SDL3_Triangle CXX)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)

# Abseil
FetchContent_Declare(
  absl
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG        20250814.1  # A stable release tag
)
set(ABSL_PROPAGATE_CXX_STD ON) # Prevent Abseil compiler flags
FetchContent_MakeAvailable(absl)

add_executable(SDL3_Triangle main.cpp)
target_link_libraries(SDL3_Triangle 
    PRIVATE 
    SDL3::SDL3
    absl::flags
    absl::flags_parse
)