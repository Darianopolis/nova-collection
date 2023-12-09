@echo off

where /q cl
goto env_missing_%errorlevel%
:env_missing_1
echo Setting up env...
call "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvarsx86_amd64.bat"
:env_missing_0

mkdir build\vendor
cd build\vendor

    git clone --depth 1 https://github.com/LuaJIT/LuaJIT.git
    cd luajit\src
        git pull
        call msvcbuild.bat debug static
    cd ..\..

    git clone --depth 1 https://github.com/ThePhD/sol2.git
    cd sol2
        git pull
    cd ..

cd ..\..