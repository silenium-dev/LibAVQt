cmake_minimum_required(VERSION 3.16)
project(LibAVQt)

set(CMAKE_CXX_STANDARD 20)

if (WIN32)
    include_directories(${OPENAL_INCLUDE_DIR} ${FFMPEG_INCLUDE_DIR})
    link_directories(${OPENAL_LIBRARY} ${FFMPEG_LIBRARY})
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS on)
endif ()

if (WIN32)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif ()

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTORCC on)
set(CMAKE_AUTOUIC on)

add_subdirectory(AVQt)
include_directories(AVQt)
add_subdirectory(Player)