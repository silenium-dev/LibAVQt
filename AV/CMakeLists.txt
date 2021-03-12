cmake_minimum_required(VERSION 3.12)

project(libAV)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTORCC on)
set(CMAKE_AUTOUIC on)

find_package(Qt6 COMPONENTS Core Gui Concurrent Widgets)

if (WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS on)
endif ()
add_compile_options(-Wno-unused-result)

add_library(AV SHARED IFrameSource.h IFrameSink.h DecoderVAAPI.cpp DecoderVAAPI.h FrameSaver.cpp FrameSaver.h EncoderVAAPI.cpp EncoderVAAPI.h)
target_link_libraries(AV Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Widgets avformat avfilter avutil avcodec avdevice swscale swresample)