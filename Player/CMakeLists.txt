cmake_minimum_required(VERSION 3.12)

function(list_transform_prepend var_out var_in prefix)
    set(temp "")
    foreach (f ${${var_in}})
        list(APPEND temp "${prefix}${f}")
    endforeach ()
    set(${var_out} "${temp}" PARENT_SCOPE)
endfunction()

set(CMAKE_CXX_STANDARD 20)

set(PROJECT_NAME Player)

#add_compile_definitions(ENABLE_QSV_ENCODE)

add_executable(${PROJECT_NAME} main.cpp)

set(QT_VERSION "5")

set(REQUIRED_LIBS Core Gui Concurrent Widgets OpenGL)
if (QT_VERSION EQUAL "6")
    LIST(APPEND REQUIRED_LIBS OpenGLWidgets)
endif ()
set(REQUIRED_LIBS_QUALIFIED)
list_transform_prepend(REQUIRED_LIBS_QUALIFIED REQUIRED_LIBS "Qt${QT_VERSION}::")

find_package(Qt${QT_VERSION} COMPONENTS ${REQUIRED_LIBS} REQUIRED)
target_link_libraries(${PROJECT_NAME} ${REQUIRED_LIBS_QUALIFIED})
if (WIN32)
    set(DEBUG_SUFFIX)
    if (CMAKE_BUILD_TYPE MATCHES "Debug")
        set(DEBUG_SUFFIX "d")
    endif ()
    set(QT_INSTALL_PATH "${CMAKE_PREFIX_PATH}")
    if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
        set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        if (NOT EXISTS "${QT_INSTALL_PATH}/bin")
            set(QT_INSTALL_PATH "${QT_INSTALL_PATH}/..")
        endif ()
    endif ()
    if (EXISTS "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/plugins/platforms/qwindows${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/plugins/platforms/")
    endif ()
    foreach (QT_LIB ${REQUIRED_LIBS})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${QT_INSTALL_PATH}/bin/Qt${QT_VERSION}${QT_LIB}${DEBUG_SUFFIX}.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
    endforeach (QT_LIB)
endif ()

#target_link_libraries(Player Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Widgets Qt6::OpenGLWidgets Qt6::OpenGL avformat avfilter avutil avcodec avdevice swscale swresample AVQt)
target_link_libraries(Player avformat avfilter avutil avcodec avdevice swscale swresample AVQt)


if (LINUX)
    find_package(OpenAL)
    target_link_libraries(Player openal GL EGL GLU va)
elseif (WIN32)
    target_link_libraries(Player OpenAL32 opengl32 dxguid d3d9)
endif ()
