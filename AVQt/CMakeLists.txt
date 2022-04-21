cmake_minimum_required(VERSION 3.12)

function(list_transform_prepend var_out var_in prefix)
    set(temp "")
    foreach (f ${${var_in}})
        list(APPEND temp "${prefix}${f}")
    endforeach ()
    set(${var_out} "${temp}" PARENT_SCOPE)
endfunction()

set(PROJECT_NAME AVQt)
set(CMAKE_CXX_STANDARD 17)

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

set(REQUIRED_LIBS Core Gui Concurrent OpenGL)

set(REQUIRED_LIBS_QUALIFIED)

list_transform_prepend(REQUIRED_LIBS_QUALIFIED REQUIRED_LIBS "Qt${QT_VERSION}::")

find_package(Qt${QT_VERSION} COMPONENTS ${REQUIRED_LIBS} REQUIRED)

find_package(PkgConfig REQUIRED)

include(CheckCXXSourceCompiles)

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavutil libavformat libavcodec libavfilter libswscale libswresample zlib)

set(SOURCES
        AVQtShader.qrc

        include/AVQt/AVQt
        include/AVQt/AVQt.h

        include/AVQt/global.hpp
        src/global.cpp

        include/AVQt/input/Demuxer.hpp
        src/input/private/Demuxer_p.hpp
        src/input/Demuxer.cpp

        include/AVQt/output/Muxer.hpp
        src/output/private/Muxer_p.hpp
        src/output/Muxer.cpp

        include/AVQt/communication/Message.hpp
        src/communication/Message.cpp

        include/AVQt/debug/CommandConsumer.hpp
        src/debug/CommandConsumer.cpp

        include/AVQt/communication/IComponent.hpp

        include/AVQt/communication/PacketPadParams.hpp
        src/communication/PacketPadParams.cpp

        include/AVQt/communication/VideoPadParams.hpp
        src/communication/VideoPadParams.cpp

        include/AVQt/decoder/VideoDecoder.hpp
        src/decoder/private/VideoDecoder_p.hpp
        src/decoder/VideoDecoder.cpp

        include/AVQt/decoder/IVideoDecoderImpl.hpp

        include/AVQt/decoder/VideoDecoderFactory.hpp
        src/decoder/VideoDecoderFactory.cpp

        include/AVQt/renderers/IOpenGLFrameMapper.hpp

        include/AVQt/renderers/OpenGLFrameMapperFactory.hpp
        src/renderers/OpenGLFrameMapperFactory.cpp

        src/renderers/FallbackFrameMapper.hpp
        src/renderers/private/FallbackFrameMapper_p.hpp
        src/renderers/FallbackFrameMapper.cpp

        include/AVQt/encoder/VideoEncoder.hpp
        src/encoder/private/VideoEncoder_p.hpp
        src/encoder/VideoEncoder.cpp

        include/AVQt/encoder/IVideoEncoderImpl.hpp
        src/encoder/IVideoEncoderImpl.cpp

        include/AVQt/encoder/VideoEncoderFactory.hpp
        src/encoder/VideoEncoderFactory.cpp

        src/communication/FrameDestructor.hpp
        src/communication/FrameDestructor.cpp

        src/communication/PacketDestructor.hpp
        src/communication/PacketDestructor.cpp

        include/AVQt/common/FBOPool.hpp
        src/common/private/FBOPool_p.hpp
        src/common/FBOPool.cpp

        include/AVQt/common/Platform.hpp
        src/common/Platform.cpp

        include/AVQt/common/PixelFormat.hpp
        src/common/PixelFormat.cpp

        include/AVQt/common/ContainerFormat.hpp
        src/common/ContainerFormat.cpp

        include/AVQt/capture/IDesktopCaptureImpl.hpp

        include/AVQt/capture/DesktopCapturer.hpp
        src/capture/private/DesktopCapturer_p.hpp
        src/capture/DesktopCapturer.cpp

        include/AVQt/capture/DesktopCaptureFactory.hpp
        src/capture/DesktopCaptureFactory.cpp

        src/capture/DummyDesktopCaptureImpl.hpp
        src/capture/DummyDesktopCaptureImpl.cpp

        src/decoder/QSVDecoderImpl.hpp
        src/decoder/private/QSVDecoderImpl_p.hpp
        src/decoder/QSVDecoderImpl.cpp
        )
set(SOURCES_UNIX
        )
set(SOURCES_LINUX
        src/decoder/VAAPIDecoderImpl.hpp
        src/decoder/private/VAAPIDecoderImpl_p.hpp
        src/decoder/VAAPIDecoderImpl.cpp

        src/renderers/VAAPIOpenGLRenderMapper.hpp
        src/renderers/private/VAAPIOpenGLRenderMapper_p.hpp
        src/renderers/VAAPIOpenGLRenderMapper.cpp

        ##      WIP
        #        src/renderers/DRM_OpenGL_RenderMapper.hpp
        #        src/renderers/private/DRM_OpenGL_RenderMapper_p.hpp
        #        src/renderers/DRM_OpenGL_RenderMapper.cpp

        include/AVQt/filter/VaapiYuvToRgbMapper.hpp
        src/filter/private/VaapiYuvToRgbMapper_p.hpp
        src/filter/VaapiYuvToRgbMapper.cpp

        src/encoder/VAAPIEncoderImpl.hpp
        src/encoder/private/VAAPIEncoderImpl_p.hpp
        src/encoder/VAAPIEncoderImpl.cpp

        src/capture/PipeWireDesktopCaptureImpl.hpp
        src/capture/private/PipeWireDesktopCaptureImpl_p.hpp
        src/capture/PipeWireDesktopCaptureImpl.cpp

        src/decoder/V4L2M2MDecoderImpl.hpp
        src/decoder/private/V4L2M2MDecoderImpl_p.hpp
        src/decoder/V4L2M2MDecoderImpl.cpp
        )
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
elseif (UNIX AND NOT ANDROID AND NOT IOS) # only on desktop
    add_library(AVQtObject OBJECT ${SOURCES} ${SOURCES_LINUX} ${SOURCES_UNIX})
elseif (ANDROID)
    add_library(AVQtObject OBJECT ${SOURCES} ${SOURCES_UNIX})
else ()
    message(FATAL_ERROR "LibAVQt doesn't support other platforms than Windows (10 and up, Build 1809 and up) and Linux")
endif ()

target_include_directories(AVQtObject PRIVATE include/AVQt src)
target_include_directories(AVQtObject PUBLIC include)

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
    if (CMAKE_COMPILER_IS_GNUCXX)
        target_compile_options(AVQtObject PRIVATE -Wno-psabi)
    endif ()
endif ()

add_library(AVQtStatic STATIC $<TARGET_OBJECTS:AVQtObject>)
add_library(AVQt SHARED $<TARGET_OBJECTS:AVQtObject>)
set_target_properties(AVQtStatic PROPERTIES OUTPUT_NAME AVQt_static)

target_compile_definitions(AVQtStatic PRIVATE AVQT_LIBRARY_BUILD)
target_compile_definitions(AVQt PRIVATE AVQT_LIBRARY_BUILD)

set(LINK_LIBRARIES
        ${REQUIRED_LIBS_QUALIFIED}

        pgraph_network_static

        PkgConfig::FFMPEG
        )

if (NOT ANDROID AND NOT IOS)
    LIST(APPEND LINK_LIBRARIES
            ScreenCaptureStatic)
endif ()

if (WIN32)
    list(APPEND LINK_LIBRARIES opengl32 OpenAL32 windowsapp user32 kernel32 Dwmapi psapi d3d9 glu32 dxguid)
else ()
    list(APPEND LINK_LIBRARIES EGL)
    if (NOT ANDROID)
        list(APPEND LINK_LIBRARIES GL va va-drm GLU)
    else ()
        list(APPEND LINK_LIBRARIES GLESv2)
    endif ()
endif ()

target_link_libraries(AVQtObject PRIVATE ${LINK_LIBRARIES})
target_link_libraries(AVQtStatic PUBLIC ${LINK_LIBRARIES})
target_link_libraries(AVQt PUBLIC ${LINK_LIBRARIES})

add_library(AVQt::Static ALIAS AVQtStatic)
add_library(AVQt::Shared ALIAS AVQt)

if (UNIX)
    include(GNUInstallDirs)
    install(TARGETS AVQt AVQtStatic
            EXPORT AVQtTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            )
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/AVQt"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.hpp"
            )
    install(EXPORT AVQtTargets
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AVQt
            NAMESPACE AVQt::
            FILE AVQtTargets.cmake
            )
    include(CMakePackageConfigHelpers)
    configure_package_config_file(
            "cmake/Config.cmake.in"
            "AVQtConfig.cmake"
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AVQt
            PATH_VARS CMAKE_INSTALL_LIBDIR
    )
    write_basic_package_version_file(
            ${CMAKE_CURRENT_BINARY_DIR}/AVQtConfigVersion.cmake
            VERSION 0.1-dev
            COMPATIBILITY ExactVersion
    )
    install(
            FILES "${CMAKE_CURRENT_BINARY_DIR}/AVQtConfigVersion.cmake" ${CMAKE_CURRENT_BINARY_DIR}/AVQtConfig.cmake
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/AVQt"
    )
endif ()
