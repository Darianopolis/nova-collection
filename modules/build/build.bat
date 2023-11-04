@echo off

echo Compiling...

mkdir out\bootstrap >nul 2>&1
del out\bootstrap\* /Q
cd out\bootstrap

    cl /c /nologo /EHsc ^
        /std:c++latest ^
        /MP ^
        /openmp:llvm ^
        /Zc:preprocessor ^
        /I..\..\src ^
        /I..\..\vendor\sol2\include ^
        /I..\..\vendor\luajit\src ^
        ..\..\src\build.cpp ^
        ..\..\src\debug.cpp ^
        ..\..\src\load.cpp ^
        ..\..\src\main.cpp

cd ..\..

echo Linking...

link /nologo ^
    /out:out\bootstrap\bldr.exe out\bootstrap\*.obj ^
    vendor\luajit\src\luajit.lib ^
    vendor\luajit\src\lua51.lib
