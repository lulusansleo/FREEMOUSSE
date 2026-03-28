cmake_minimum_required(VERSION 3.22)
project(dj_visualizer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(FetchContent)

FetchContent_Declare(SPSCQueue
  GIT_REPOSITORY https://github.com/rigtorp/SPSCQueue.git
  GIT_TAG        v1.1)
FetchContent_MakeAvailable(SPSCQueue)

FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        0.9.9.8)
FetchContent_MakeAvailable(glm)

find_package(OpenGL REQUIRED)
find_package(SDL2   REQUIRED)
include(cmake/FindPortAudio.cmake)
include(cmake/FindFFTW3.cmake)
include(cmake/FindAubio.cmake)

add_subdirectory(src/core)
add_subdirectory(src/audio)
add_subdirectory(src/analysis)
add_subdirectory(src/render)

add_executable(dj_visualizer src/main.cpp)
target_link_libraries(dj_visualizer PRIVATE dv_core dv_audio dv_analysis dv_render)
