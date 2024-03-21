#!/bin/bash

git clone https://github.com/TinyCC/tinycc.git

cd tinycc

./configure --with-selinux
make CFLAGS='-Ofast' -j4

mv libtcc.a ../
mv libtcc1.a ../
mv libtcc.h ../
mv runmain.o ../
mv bt-log.o ../

cd .. 

mkdir -p build

mv runmain.o ./build
mv bt-log.o ./build

rm -rf tinycc

SOURCES=$(find $SRC_DIR -name "*.cpp")

SOURCE_DIR="."
BUILD_DIR="obj"
EXECUTABLE="occultc"

mkdir -p $BUILD_DIR
mkdir -p build

for SOURCE in $SOURCES; do
     g++ -c -w -g -Ofast $SOURCE -o $BUILD_DIR/$(basename ${SOURCE%.*}.o)
done

OBJECTS=$(find $BUILD_DIR -name "*.o")

g++ -o $BUILD_DIR/$EXECUTABLE $OBJECTS ./libtcc.a ./libtcc1.a

chmod +x $BUILD_DIR/$EXECUTABLE

mv libtcc1.a ./build
mv $BUILD_DIR/$EXECUTABLE ./build

cd build

exec bash