cmake_minimum_required(VERSION 3.16)
project(X5FisheyeViewer CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GLEW REQUIRED)

set(FFGL_DIR "${CMAKE_SOURCE_DIR}/../ffgl" CACHE PATH "FFGL SDK root")
set(FFGL_LIB "${FFGL_DIR}/source/lib")

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

target_link_libraries(X5FisheyeViewer PRIVATE GLEW::GLEW opengl32)

set_target_properties(X5FisheyeViewer PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
)
