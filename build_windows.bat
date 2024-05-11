@echo off
setlocal

if not exist "build" (
    git clone https://github.com/TinyCC/tinycc.git

    cd .\tinycc\win32
    
    call build-tcc.bat
	
    move include ..\..\	
    move libtcc.dll ..\..
    move .\lib\libtcc1.a ..\..
    move .\lib\runmain.o ..\..
    move .\lib\bt-log.o ..\..
    move .\include\tccdefs.h ..\..

    cd ..\..
)

mkdir "build"

move runmain.o .\build
move bt-log.o .\build
move tccdefs.h .\build
move include .\build
move libtcc1.a .\build

rmdir /s /q tinycc

g++ -std=c++2a *.cpp parser/*.cpp static_analyzer/*.cpp lexer/*.cpp code_generation/*.cpp data_structures/*.cpp jit/*.cpp setup/*.cpp  libtcc.dll -o occultc.exe

copy libtcc.dll .\build
move ./occultc.exe .\build

endlocal
