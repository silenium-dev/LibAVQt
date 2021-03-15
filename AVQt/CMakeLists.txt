cmake_minimum_required(VERSION 3.12)

project(libAVQt)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTORCC on)
set(CMAKE_AUTOUIC on)

find_package(Qt6 COMPONENTS Core Gui Concurrent Widgets OpenGL OpenGLWidgets)

if (WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS on)
endif ()
add_compile_options(-Wno-unused-result)

add_library(AVQt SHARED AVQt input/IFrameSource.h output/IFrameSink.h input/DecoderVAAPI.cpp input/DecoderVAAPI.h output/FrameFileSaver.cpp output/FrameFileSaver.h output/EncoderVAAPI.cpp output/EncoderVAAPI.h output/OpenGLWidgetRenderer.cpp output/OpenGLWidgetRenderer.h output/private/OpenGLWidgetRenderer_p.h output/private/EncoderVAAPI_p.h output/private/FrameFileSaver_p.h input/private/DecoderVAAPI_p.h)
target_link_libraries(AVQt Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets avformat avfilter avutil avcodec avdevice swscale swresample)