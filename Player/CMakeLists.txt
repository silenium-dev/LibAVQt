#SET(CMAKE_INSTALL_RPATH "$ORIGIN/../AVQt")
#SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH on)

#find_package(Qt6 COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Concurrent)

set(CMAKE_CXX_STANDARD 20)

find_package(Qt5 COMPONENTS Core Gui Widgets OpenGL Concurrent)
find_package(OpenAL)

#add_compile_options(-fsanitize=address)
#add_link_options(-fsanitize=address)

add_executable(Player main.cpp)

#target_link_libraries(Player Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Widgets Qt6::OpenGLWidgets Qt6::OpenGL avformat avfilter avutil avcodec avdevice swscale swresample AVQtStatic)
target_link_libraries(Player Qt5::Core Qt5::Gui Qt5::Concurrent Qt5::Widgets Qt5::OpenGL avformat avfilter avutil avcodec avdevice swscale swresample AVQtStatic)

if (LINUX)
    target_link_libraries(Player openal GL EGL GLU va)
elseif (WIN32)
    target_link_libraries(Player OpenAL32 opengl32)
endif ()