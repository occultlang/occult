#!/bin/bash


SOURCES=$(find $SRC_DIR -name "*.cpp")
HEADERS=$(find $SRC_DIR -name "*.hpp")

SOURCE_DIR="."
BUILD_DIR="output"
EXECUTABLE="occultc"

mkdir -p $BUILD_DIR

for HEADER in $HEADERS; do
   g++ -c -w -g $HEADER -o $BUILD_DIR/$(basename ${HEADER%.*}.o)
done

for SOURCE in $SOURCES; do
     g++ -c -w -g $SOURCE -o $BUILD_DIR/$(basename ${SOURCE%.*}.o)
done

OBJECTS=$(find $BUILD_DIR -name "*.o")

g++ -o $BUILD_DIR/$EXECUTABLE $OBJECTS ./libtcc.a ./libtcc1.a

chmod +x $BUILD_DIR/$EXECUTABLE
