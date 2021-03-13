cmake_minimum_required(VERSION 3.17)
project(transcode)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTORCC on)
set(CMAKE_AUTOUIC on)

add_subdirectory(AV)

SET(CMAKE_INSTALL_RPATH "$ORIGIN/../AV")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH on)

find_package(Qt6 COMPONENTS Core Gui Widgets Concurrent)

add_compile_options(-O4)

add_executable(transcode main.cpp)

target_link_libraries(transcode Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Widgets avformat avfilter avutil avcodec avdevice swscale swresample AV)