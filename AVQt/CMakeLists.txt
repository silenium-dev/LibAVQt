cmake_minimum_required(VERSION 3.12)

project(libAVQt)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTORCC on)
set(CMAKE_AUTOUIC on)

set(CMAKE_INCLUDE_CURRENT_DIR on)

find_package(Qt6 COMPONENTS Core Gui Concurrent Widgets OpenGL OpenGLWidgets)

if (WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS on)
endif ()
add_compile_options(-Wno-unused-result)

#if (${CMAKE_BUILD_TYPE} EQUAL "Release")
#    add_compile_options(-O3)
#endif ()

add_library(
        AVQt
        SHARED

        resources.qrc

        AVQt


        input/IPacketSource.h
        input/IFrameSource.h

        output/IPacketSink.h
        output/IFrameSink.h


        input/Demuxer.h
        input/private/Demuxer_p.h
        input/Demuxer.cpp

        filter/DecoderVAAPI.h
        filter/private/DecoderVAAPI_p.h
        filter/DecoderVAAPI.cpp

        filter/AudioDecoder.h
        filter/private/AudioDecoder_p.h
        filter/private/OpenALErrorHandler.h
        filter/AudioDecoder.cpp

        #        output/FrameFileSaver.h
        #        output/private/FrameFileSaver_p.h
        #        output/FrameFileSaver.cpp

        #        output/EncoderVAAPI.h
        #        output/private/EncoderVAAPI_p.h
        #        output/EncoderVAAPI.cpp

        output/RenderClock.h
        output/private/RenderClock_p.h
        output/RenderClock.cpp

        output/OpenGLRenderer.h
        output/private/OpenGLRenderer_p.h
        output/OpenGLRenderer.cpp

        output/OpenALAudioOutput.h
        output/private/OpenALAudioOutput_p.h
        output/OpenALAudioOutput.cpp
)

target_compile_options(AVQt PRIVATE
        -std=c++20
        -Werror=all
        -Werror=extra
        -Werror=pedantic
        -ansi
        -Werror=init-self
        -Werror=old-style-cast
        -Werror=overloaded-virtual
        -Werror=uninitialized
        -Werror=missing-declarations
        -Werror=init-self)

target_link_libraries(AVQt Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets avformat avfilter avutil avcodec avdevice swscale swresample GL openal)
