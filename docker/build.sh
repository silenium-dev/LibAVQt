#!/bin/bash

git clone https://edugit.org/sdcieo0330/LibAVQt.git

cd LibAVQt || exit
mkdir build && cd build || exit
if [[ -e /usr/bin/g++-10 ]]; then
  cmake -DCMAKE_C_COMPILER=/usr/bin/gcc-10 -DCMAKE_CXX_COMPILER=/usr/bin/g++-10 -DCMAKE_BUILD_TYPE=Debug .. && cmake --build . -j4
else
  cmake -DCMAKE_BUILD_TYPE=Debug .. && cmake --build . -j4
fi