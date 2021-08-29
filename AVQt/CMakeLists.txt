cmake_minimum_required(VERSION 3.12)

project(libAVQt)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTORCC on)
set(CMAKE_AUTOUIC on)

set(CMAKE_INCLUDE_CURRENT_DIR on)

add_compile_options(/EHa)

if (WIN32)
    string(FIND "${CMAKE_SYSTEM_VERSION}" "." FIRST_DOT)
    string(FIND "${CMAKE_SYSTEM_VERSION}" "." LAST_DOT REVERSE)
    math(EXPR LAST_DOT "${LAST_DOT}+1") # We don't want the dot in the build number

    string(SUBSTRING "${CMAKE_SYSTEM_VERSION}" 0 ${FIRST_DOT} WINDOWS_VERSION)
    string(SUBSTRING "${CMAKE_SYSTEM_VERSION}" ${LAST_DOT} -1 WINDOWS_BUILD)
    message("Windows ${WINDOWS_VERSION} with build number: ${WINDOWS_BUILD}")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS on)
endif()

#find_package(Qt6 COMPONENTS Core Gui Concurrent Widgets OpenGL OpenGLWidgets)
find_package(Qt5 COMPONENTS Core Gui Concurrent Widgets OpenGL)

#add_compile_options(-fsanitize=address)
#add_link_options(-fsanitize=address)

#if (${CMAKE_BUILD_TYPE} EQUAL "Release")
#    -O3)
#endif ()

set(SOURCES
        resources.qrc

        AVQt

        input/IPacketSource.h
        input/IFrameSource.h

        output/IPacketSink.h
        output/IFrameSink.h

        input/Demuxer.h
        input/private/Demuxer_p.h
        input/Demuxer.cpp

        filter/IDecoder.h
        filter/IEncoder.h

        filter/DecoderQSV.h
        filter/private/DecoderQSV_p.h
        filter/DecoderQSV.cpp

        filter/AudioDecoder.h
        filter/private/AudioDecoder_p.h
        filter/private/OpenALErrorHandler.h
        filter/AudioDecoder.cpp

        filter/EncoderQSV.h
        filter/private/EncoderQSV_p.h
        filter/EncoderQSV.cpp

        output/RenderClock.h
        output/private/RenderClock_p.h
        output/RenderClock.cpp

        output/OpenGLRenderer.h
        output/private/OpenGLRenderer_p.h
        output/OpenGLRenderer.cpp

        output/OpenALAudioOutput.h
        output/private/OpenALAudioOutput_p.h
        output/OpenALAudioOutput.cpp

        output/Muxer.h
        output/private/Muxer_p.h
        output/Muxer.cpp

        #        output/FrameFileSaver.h
        #        output/private/FrameFileSaver_p.h
        #        output/FrameFileSaver.cpp
        )
set(SOURCES_LINUX
        filter/DecoderVAAPI.h
        filter/private/DecoderVAAPI_p.h
        filter/DecoderVAAPI.cpp

        filter/DecoderMMAL.h
        filter/private/DecoderMMAL_p.h
        filter/DecoderMMAL.cpp

        filter/EncoderVAAPI.h
        filter/private/EncoderVAAPI_p.h
        filter/EncoderVAAPI.cpp
        )
set(SOURCES_WINDOWS
        filter/DecoderDXVA2.h
        filter/private/DecoderDXVA2_p.h
        filter/DecoderDXVA2.cpp
        
        filter/DecoderD3D11VA.h
        filter/private/DecoderD3D11VA_p.h
        filter/DecoderD3D11VA.cpp
        )

if (WIN32) # LibAVQt only supports Windows 10 1809 and above
    if (${WINDOWS_VERSION} GREATER_EQUAL 10 AND ${WINDOWS_BUILD} GREATER_EQUAL 17763)
        #    add_compile_options(/EHa)
        add_library(AVQt SHARED ${SOURCES} ${SOURCES_WINDOWS})
        add_library(AVQtStatic STATIC ${SOURCES} ${SOURCES_WINDOWS})
    else ()
        message(FATAL_ERROR "LibAVQt supports only Windows 10 and up, Build 1809 and up")
    endif ()
elseif (UNIX)
    add_library(AVQt SHARED ${SOURCES} ${SOURCES_LINUX})
    add_library(AVQtStatic STATIC ${SOURCES} ${SOURCES_LINUX})
else ()
    message(FATAL_ERROR "LibAVQt doesn't support other platforms than Windows (10 and up, Build 1809 and up) and Linux")
endif ()

set_target_properties(AVQtStatic PROPERTIES OUTPUT_NAME AVQt_static)

if (!WIN32)
    target_compile_options(AVQt PRIVATE
            -std=c++20
            -Werror=all
            -Werror=extra
            -Werror=pedantic
            -Wno-float-equal
            -ansi
            -Werror=init-self
            -Werror=old-style-cast
            -Werror=overloaded-virtual
            -Werror=uninitialized
            -Werror=missing-declarations
            -Werror=init-self
            #    -Wall -Wextra
            -Wno-error=non-virtual-dtor
            #        -Wno-unused-result
            #-Wpedantic
            -Wshadow
            -Wfatal-errors
            -Wundef
            -Wuninitialized
            -Winit-self
            -Wconversion
            -Wfloat-equal
            -Wstrict-aliasing
            -Wtrigraphs
            -Wodr # one definition rule
            -Wzero-as-null-pointer-constant
            -Wsizeof-array-argument
            -Winline
            -Wcast-qual
            -Weffc++


            #    -fno-common # each global variable is only declared once.
            #    -fmerge-all-constants
            #    -ffunction-sections
            #    -fdata-sections
            #    -fno-use-cxa-atexit
            #    -fverbose-asm
            #    -fstrict-enums
            -Wstack-protector
            -Wdouble-promotion
            )
    target_compile_options(AVQtStatic PRIVATE
            -std=c++20
            -Werror=all
            -Werror=extra
            -Werror=pedantic
            -Wno-float-equal
            -ansi
            -Werror=init-self
            -Werror=old-style-cast
            -Werror=overloaded-virtual
            -Werror=uninitialized
            -Werror=missing-declarations
            -Werror=init-self
            #    -Wall -Wextra
            -Wno-error=non-virtual-dtor
            #        -Wno-unused-result
            #-Wpedantic
            -Wshadow
            -Wfatal-errors
            -Wundef
            -Wuninitialized
            -Winit-self
            -Wconversion
            -Wfloat-equal
            -Wstrict-aliasing
            -Wtrigraphs
            -Wodr # one definition rule
            -Wzero-as-null-pointer-constant
            -Wsizeof-array-argument
            -Winline
            -Wcast-qual
            -Weffc++


            #    -fno-common # each global variable is only declared once.
            #    -fmerge-all-constants
            #    -ffunction-sections
            #    -fdata-sections
            #    -fno-use-cxa-atexit
            #    -fverbose-asm
            #    -fstrict-enums
            -Wstack-protector
            -Wdouble-promotion
            )
endif ()

#target_link_libraries(AVQt Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets avformat avfilter avutil avcodec avdevice swscale swresample GL openal)
#target_link_libraries(AVQtStatic Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets avformat avfilter avutil avcodec avdevice swscale swresample GL openal)
target_link_libraries(AVQt Qt5::Core Qt5::Gui Qt5::Concurrent Qt5::Widgets Qt5::OpenGL avformat avfilter avutil avcodec avdevice swscale swresample)
target_link_libraries(AVQtStatic Qt5::Core Qt5::Gui Qt5::Concurrent Qt5::Widgets Qt5::OpenGL avformat avfilter avutil avcodec avdevice swscale swresample)

if (WIN32)
    target_link_libraries(AVQt opengl32 OpenAL32 windowsapp user32 kernel32 Dwmapi psapi d3d9 dxguid)
    target_link_libraries(AVQtStatic opengl32 OpenAL32 windowsapp user32 kernel32 Dwmapi psapi d3d9 dxguid)
else ()
    target_link_libraries(AVQt GL openal EGL GLU va)
    target_link_libraries(AVQtStatic GL openal EGL GLU va)
endif ()