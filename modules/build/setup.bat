@echo off

mkdir vendor
cd vendor

    git clone https://luajit.org/git/luajit.git
    cd luajit\src
        git pull
        call msvcbuild.bat static
    cd ..\..

    git clone https://github.com/ThePhD/sol2.git
    cd sol2
        git pull
    cd ..

cd ..