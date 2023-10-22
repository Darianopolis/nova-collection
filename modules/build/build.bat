@echo off

echo Compiling...

mkdir build >nul 2>&1
mkdir out >nul 2>&1
cd build
cl /c /nologo /EHsc ^
    /std:c++latest ^
    /I..\src ^
    /I..\vendor\sol2\include ^
    /I..\vendor\luajit\src ^
    ..\src\main.cpp
cd ..

echo Linking...

link /nologo ^
    /out:out\bldr.exe build\main.obj ^
    vendor\luajit\src\luajit.lib ^
    vendor\luajit\src\lua51.lib
