#!/bin/bash

SOURCE_DIR="."
BUILD_DIR="obj"
EXECUTABLE="occultc"

if [ ! -d "$BUILD_DIR" ] && [ ! -d "build" ]; then
     git clone https://github.com/TinyCC/tinycc.git

     cd tinycc

     ./configure --with-selinux
     make -j8 # CFLAGS='-Ofast'

     mv libtcc.a ../
     mv libtcc1.a ../
     mv libtcc.h ../jit/
     mv runmain.o ../
     mv bt-log.o ../

     cd ..
fi

mkdir -p build
mkdir -p $BUILD_DIR

mv runmain.o ./build
mv bt-log.o ./build

rm -rf tinycc

SOURCES=$(find $SRC_DIR -name "*.cpp")

for SOURCE in $SOURCES; do
      g++ -c -w -g $SOURCE -o $BUILD_DIR/$(basename ${SOURCE%.*}.o)
done

OBJECTS=$(find $BUILD_DIR -name "*.o")

g++ -o $BUILD_DIR/$EXECUTABLE $OBJECTS ./libtcc.a ./libtcc1.a

chmod +x $BUILD_DIR/$EXECUTABLE

cp libtcc1.a ./build
mv $BUILD_DIR/$EXECUTABLE ./build