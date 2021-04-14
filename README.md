# LibAVQt

An object-oriented Qt interface for FFmpeg.

## Features

### Current

- VAAPI accelerated decode
- Frame sink for saving every n-th frame to file
- QOpenGLWidget based renderer (***WIP***)
- OpenAL based audio output (***WIP***)

### Planned

- Reliable API Reference
- Audio encode
- Other hardware accelerations
- Frame filters for e.g. scaling or color <-> greyscale
- Other renderers (e.g. QWidget, QGraphicsItem)
- CMake module

## Build from source

### Prequisites

- CMake
- C++ Development environment
- Vulkan + headers (In Qt6, QML defaults to Vulkan instead of OpenGL for
  rendering)
- Correctly configured Qt6 installation
    - for linux distributions with Qt6 in the package repositories, just install
      it with your distributions package manager
    - for linux distributions without Qt6 in the repositories, Windows and
      macOS, use the installer from the Qt website: (you have to create an Qt
      account in order to install Qt): https://www.qt.io/download-qt-installer

### Clone the repo

```
git clone https://github.com/sdcieo0330/LibAVQt.git
cd LibAVQt
```

### Run CMake and build

```
mkdir build && cd build
cmake ..
cmake --build . -j4
```

## Use with CMake

Add the following lines to your ``CMakeLists.txt``
(the ``<path>`` must be replaced with the path to the cloned repository):

```
include_directories(<path>)
link_directories(<path>/build/AVQt)
```

## Examples

A simple example for displaying video with the OpenGLRenderer and OpenALAudioOutput can be found
in ``main.cpp``. It will be compiled along with the library by the commands above.

### Run the example
In your build directory, run the example by:
```
./Player
```
