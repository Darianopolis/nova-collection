@echo off
copy /Y bin\bldr.exe build\out\bldr.exe >nul 2>nul
build\out\bldr.exe %*