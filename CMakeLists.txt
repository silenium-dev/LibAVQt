cmake_minimum_required(VERSION 3.17)
project(LibAVQt)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_AUTOMOC on)
set(CMAKE_AUTORCC on)
set(CMAKE_AUTOUIC on)

add_subdirectory(AVQt)
include_directories(AVQt)
add_subdirectory(Player)