@echo off
mkdir build >nul 2>&1
mkdir out >nul 2>&1
cd build
cl /c /nologo /EHsc /c ..\src\main.cpp
cd ..
link /nologo /out:out\bldr.exe build\main.obj