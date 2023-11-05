@echo off

where /q cl
goto env_missing_%errorlevel%
:env_missing_1
echo Setting up env...
call "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvarsx86_amd64.bat"
:env_missing_0

echo Compiling...

mkdir out\bootstrap >nul 2>&1
del out\bootstrap\* /Q
cd out\bootstrap

    cl /c /nologo /EHsc ^
        /std:c++latest ^
        /MP ^
        /Zc:preprocessor ^
        /I..\..\src ^
        /I..\..\vendor\sol2\include ^
        /I..\..\vendor\luajit\src ^
        ..\..\src\build.cpp ^
        ..\..\src\debug.cpp ^
        ..\..\src\load.cpp ^
        ..\..\src\bldr.cpp

cd ..\..

echo Linking...

link /nologo ^
    /out:out\bootstrap\bldr.exe out\bootstrap\*.obj ^
    vendor\luajit\src\luajit.lib ^
    vendor\luajit\src\lua51.lib

echo Bootstrapping...

out\bootstrap\bldr.exe make -clean bldr