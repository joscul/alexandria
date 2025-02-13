#!/bin/bash

cd `dirname $0`
cd ..

export CC=/usr/bin/gcc
export CXX=/usr/bin/g++

base_path=`pwd`

cd $base_path
cd deps

cd zlib-1.2.12
./configure
make -j4
make install

cd $base_path
cd deps

cd CRoaring
mkdir build
cd build
cmake ..
make
make install

