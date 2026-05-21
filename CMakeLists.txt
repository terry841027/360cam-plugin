cmake_minimum_required(VERSION 3.16)
project(X5FisheyeViewer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 自動下載 FFGL SDK
include(FetchContent)
FetchContent_Declare(
    ffgl
    GIT_REPOSITORY https://github.com/resolume/ffgl.git
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(ffgl)

# 直接把 FFGL 的 .cpp 一起編進來，不依賴 SDK 自己的 CMake target
set(FFGL_LIB ${ffgl_SOURCE_DIR}/source/lib)

file(GLOB_RECURSE FFGL_SRCS
    "${FFGL_LIB}/ffgl/*.cpp"
    "${FFGL_LIB}/ffglex/*.cpp"
)

add_library(X5FisheyeViewer SHARED
    Plugin.cpp
    Plugin.h
    ${FFGL_SRCS}
)

target_include_directories(X5FisheyeViewer PRIVATE
    ${FFGL_LIB}
    ${FFGL_LIB}/ffgl
    ${FFGL_LIB}/ffglex
)

target_link_libraries(X5FisheyeViewer PRIVATE opengl32)

set_target_properties(X5FisheyeViewer PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/Debug"
)
