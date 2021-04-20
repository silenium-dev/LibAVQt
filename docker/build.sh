#!/bin/bash

git clone https://edugit.org/sdcieo0330/LibAVQt.git

cd LibAVQt || exit
mkdir build && (cd build || exit)
cmake -DCMAKE_C_COMPILER=/usr/bin/gcc-10 -DCMAKE_CXX_COMPILER=/usr/bin/g++-10 -DCMAKE_BUILD_TYPE=Debug .. && cmake --build . -j4