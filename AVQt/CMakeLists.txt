cmake_minimum_required(VERSION 3.12)

function(list_transform_prepend var_out var_in prefix)
    set(temp "")
    foreach (f ${${var_in}})
        list(APPEND temp "${prefix}${f}")
    endforeach ()
    set(${var_out} "${temp}" PARENT_SCOPE)
endfunction()

set(PROJECT_NAME AVQt)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTORCC on)
set(CMAKE_AUTOUIC on)

set(CMAKE_INCLUDE_CURRENT_DIR on)

if (WIN32)
    add_compile_options(/EHa)
    string(FIND "${CMAKE_SYSTEM_VERSION}" "." FIRST_DOT)
    string(FIND "${CMAKE_SYSTEM_VERSION}" "." LAST_DOT REVERSE)
    math(EXPR LAST_DOT "${LAST_DOT}+1") # We don't want the dot in the build number

    string(SUBSTRING "${CMAKE_SYSTEM_VERSION}" 0 ${FIRST_DOT} WINDOWS_VERSION)
    string(SUBSTRING "${CMAKE_SYSTEM_VERSION}" ${LAST_DOT} -1 WINDOWS_BUILD)
    message("Windows ${WINDOWS_VERSION} with build number: ${WINDOWS_BUILD}")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS on)
endif ()

set(QT_VERSION "5")

set(REQUIRED_LIBS Core Gui DBus Concurrent Widgets OpenGL)
if (QT_VERSION EQUAL "6")
    LIST(APPEND REQUIRED_LIBS OpenGLWidgets)
endif ()
set(REQUIRED_LIBS_QUALIFIED)
list_transform_prepend(REQUIRED_LIBS_QUALIFIED REQUIRED_LIBS "Qt${QT_VERSION}::")

find_package(Qt${QT_VERSION} COMPONENTS ${REQUIRED_LIBS} REQUIRED)

#add_compile_options(-fsanitize=address)
#add_link_options(-fsanitize=address)

#if (${CMAKE_BUILD_TYPE} EQUAL "Release")
#    -O3)
#endif ()

include_directories("include")
include_directories(src)
include_directories(include/AVQt)

set(SOURCES
        AVQtShader.qrc

        AVQt
        global.hpp

        include/AVQt/input/IPacketSource.hpp
        include/AVQt/input/IFrameSource.hpp

        include/AVQt/output/IPacketSink.hpp
        include/AVQt/output/IFrameSink.hpp

        include/AVQt/input/Demuxer.hpp
        src/input/private/Demuxer_p.hpp
        src/input/Demuxer.cpp

        include/AVQt/encoder/IEncoder.hpp

        #        include/AVQt/decoder/DecoderQSV.hpp
        #        src/decoder/private/DecoderQSV_p.hpp
        #        src/decoder/DecoderQSV.cpp

        src/renderers/private/OpenALErrorHandler.hpp

        include/AVQt/encoder/EncoderQSV.hpp
        src/encoder/private/EncoderQSV_p.hpp
        src/encoder/EncoderQSV.cpp

        include/AVQt/renderers/RenderClock.hpp
        src/renderers/private/RenderClock_p.hpp
        src/renderers/RenderClock.cpp

        #        include/AVQt/renderers/OpenGLRendererOld.hpp
        #        src/renderers/private/OpenGLRendererOld_p.hpp
        #        src/renderers/private/OpenGLRendererOld_p.cpp
        #        src/renderers/OpenGLRendererOld.cpp

        include/AVQt/renderers/OpenALAudioOutput.hpp
        src/renderers/private/OpenALAudioOutput_p.hpp
        src/renderers/OpenALAudioOutput.cpp

        #        include/AVQt/widgets/OpenGLWidgetRenderer.hpp
        #        src/widgets/private/OpenGLWidgetRenderer_p.hpp
        #        src/widgets/OpenGLWidgetRenderer.cpp

        include/AVQt/output/Muxer.hpp
        src/output/private/Muxer_p.hpp
        src/output/Muxer.cpp

        #        output/FrameFileSaver.hpp
        #        output/private/FrameFileSaver_p.hpp
        #        output/FrameFileSaver.cpp
        src/communication/Message.cpp
        include/AVQt/communication/Message.hpp
        include/AVQt/communication/PacketPadParams.hpp

        src/input/CommandConsumer.cpp
        include/AVQt/input/CommandConsumer.hpp
        src/communication/PacketPadParams.cpp
        include/AVQt/communication/IComponent.hpp
        src/communication/FilePadParams.cpp
        include/AVQt/communication/FilePadParams.hpp
        include/AVQt/communication/IOutput.hpp

        src/communication/VideoPadParams.cpp
        include/AVQt/communication/VideoPadParams.hpp include/AVQt/decoder/IDecoderImpl.hpp src/decoder/DecoderFactory.cpp include/AVQt/decoder/DecoderFactory.hpp src/renderers/OpenGLFrameMapperFactory.cpp include/AVQt/renderers/OpenGLFrameMapperFactory.hpp include/AVQt/renderers/IOpenGLFrameMapper.hpp src/renderers/FallbackFrameMapper.cpp src/renderers/FallbackFrameMapper.hpp src/renderers/private/FallbackFrameMapper_p.hpp global.cpp src/encoder/Encoder.cpp include/AVQt/encoder/Encoder.hpp src/encoder/private/Encoder_p.hpp src/encoder/EncoderFactory.cpp include/AVQt/encoder/EncoderFactory.hpp include/AVQt/encoder/IEncoderImpl.hpp src/encoder/IEncoderImpl.cpp include/AVQt/communication/HwContextSync.hpp src/communication/HWContextSync.cpp include/AVQt/transcoder/ITranscoderImpl.hpp src/transcoder/TranscoderFactory.cpp include/AVQt/transcoder/TranscoderFactory.hpp src/transcoder/Transcoder.cpp include/AVQt/transcoder/Transcoder.hpp src/transcoder/private/Transcoder_p.hpp src/communication/FrameDestructor.cpp src/communication/FrameDestructor.hpp src/communication/PacketDestructor.cpp src/communication/PacketDestructor.hpp src/common/FBOPool.hpp src/common/FBOPool.cpp)
set(SOURCES_LINUX
        include/AVQt/decoder/Decoder.hpp
        src/decoder/private/Decoder_p.hpp
        src/decoder/Decoder.cpp

        #        include/AVQt/decoder/DecoderMMAL.hpp
        #        src/decoder/private/DecoderMMAL_p.hpp
        #        src/decoder/DecoderMMAL.cpp

        include/AVQt/encoder/EncoderVAAPI.hpp
        src/encoder/private/EncoderVAAPI_p.hpp
        src/encoder/EncoderVAAPI.cpp

        #        src/renderers/private/unix/OpenGLRenderer_p.cpp
        #        src/renderers/unix/OpenGLRendererOld.cpp

        src/decoder/VAAPIDecoderImpl.hpp
        src/decoder/VAAPIDecoderImpl.cpp
        src/decoder/private/VAAPIDecoderImpl_p.hpp
        src/renderers/VAAPIOpenGLRenderMapper.cpp src/renderers/VAAPIOpenGLRenderMapper.hpp src/renderers/private/VAAPIOpenGLRenderMapper_p.hpp src/filter/VaapiYuvToRgbMapper.cpp include/AVQt/filter/VaapiYuvToRgbMapper.hpp src/filter/private/VaapiYuvToRgbMapper_p.hpp src/encoder/VAAPIEncoderImpl.cpp src/encoder/VAAPIEncoderImpl.hpp src/encoder/private/VAAPIEncoderImpl_p.hpp src/transcoder/VaapiTranscoderImpl.cpp src/transcoder/VaapiTranscoderImpl.hpp src/transcoder/private/VaapiTranscoderImpl_p.hpp src/renderers/DRM_OpenGL_RenderMapper.cpp src/renderers/DRM_OpenGL_RenderMapper.hpp src/renderers/private/DRM_OpenGL_RenderMapper_p.hpp)
set(SOURCES_WINDOWS
        decoder/DecoderDXVA2.hpp
        src/decoder/private/DecoderDXVA2_p.hpp
        src/decoder/DecoderDXVA2.cpp

        decoder/DecoderD3D11VA.hpp

        src/renderers/private/windows/OpenGLRenderer_p.cpp)

if (WIN32) # LibAVQt only supports Windows 10 1809 and above
    if (${WINDOWS_VERSION} GREATER_EQUAL 10 AND ${WINDOWS_BUILD} GREATER_EQUAL 17763)
        #    add_compile_options(/EHa)
        add_library(AVQtObject OBJECT ${SOURCES} ${SOURCES_WINDOWS})
    else ()
        message(FATAL_ERROR "LibAVQt supports only Windows 10 and up, Build 1809 and up")
    endif ()
elseif (UNIX)
    add_library(AVQtObject OBJECT ${SOURCES} ${SOURCES_LINUX})
else ()
    message(FATAL_ERROR "LibAVQt doesn't support other platforms than Windows (10 and up, Build 1809 and up) and Linux")
endif ()

target_compile_definitions(AVQtObject PRIVATE AVQT_LIBRARY_BUILD)

if (!WIN32)
    target_compile_options(AVQtObject PRIVATE
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


add_library(AVQtStatic STATIC $<TARGET_OBJECTS:AVQtObject>)
add_library(AVQt SHARED $<TARGET_OBJECTS:AVQtObject>)
set_target_properties(AVQtStatic PROPERTIES OUTPUT_NAME AVQt_static)

set(LINK_LIBRARIES
        ${REQUIRED_LIBS_QUALIFIED}
        LibScreenCapture

        pgraph
        pgraph_network

        boost_json

        avformat
        avfilter
        avutil
        avcodec
        avdevice
        swscale
        swresample
        )

if (WIN32)
    list(APPEND LINK_LIBRARIES opengl32 OpenAL32 windowsapp user32 kernel32 Dwmapi psapi d3d9 glu32 dxguid)
else ()
    list(APPEND LINK_LIBRARIES GL openal EGL GLU va)
endif ()

target_link_libraries(AVQtObject PUBLIC ${LINK_LIBRARIES})
target_link_libraries(AVQtStatic PUBLIC ${LINK_LIBRARIES})
target_link_libraries(AVQt PUBLIC ${LINK_LIBRARIES})
