@echo off

where /q cl
goto env_missing_%errorlevel%
:env_missing_1
    echo Setting up env...
    call "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvarsx86_amd64.bat"
:env_missing_0

mkdir build\out >nul 2>&1
del build\out\* /Q
cd build\out

    echo Compiling...

    cl /c /nologo /EHsc ^
        /std:c++latest ^
        /MP ^
        /openmp ^
        /Zc:preprocessor ^
        /I..\..\src ^
        /I..\vendor\sol2\include ^
        /I..\vendor\luajit\src ^
        ..\..\src\build.cpp ^
        ..\..\src\debug.cpp ^
        ..\..\src\load.cpp ^
        ..\..\src\bldr.cpp

    echo Linking...

    link /nologo ^
        /out:bldr.exe *.obj ^
        ..\vendor\luajit\src\luajit.lib ^
        ..\vendor\luajit\src\lua51.lib

cd ..\..

echo Bootstrapping...

build\out\bldr.exe make -clean bldr
bin\bldr ide bldr