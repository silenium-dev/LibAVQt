cmake_minimum_required(VERSION 3.17)
project(LibAVQt)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTORCC on)
set(CMAKE_AUTOUIC on)

add_subdirectory(AVQt)
include_directories(AVQt)

SET(CMAKE_INSTALL_RPATH "$ORIGIN/../AVQt")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH on)

#find_package(Qt6 COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Concurrent)
find_package(Qt5 COMPONENTS Core Gui Widgets OpenGL Concurrent)
find_package(OpenAL)

#add_compile_options(-O4)

add_executable(Player main.cpp)

#target_link_libraries(Player Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Widgets Qt6::OpenGLWidgets Qt6::OpenGL avformat avfilter avutil avcodec avdevice swscale swresample AVQt GL)
target_link_libraries(Player Qt5::Core Qt5::Gui Qt5::Concurrent Qt5::Widgets Qt5::OpenGL avformat avfilter avutil avcodec avdevice swscale swresample AVQt)

if (LINUX)
    target_link_libraries(Player openal GL)
endif ()
