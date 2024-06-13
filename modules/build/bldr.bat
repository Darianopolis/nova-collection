@echo off
mkdir build\out >nul 2>nul
copy /Y bin\bldr.exe build\out\bldr.exe >nul 2>nul
build\out\bldr.exe %*