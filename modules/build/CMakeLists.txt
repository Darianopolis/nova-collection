cmake_minimum_required(VERSION 3.26)

project(bldr)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(
    /Zc:preprocessor
    /Zc:__cplusplus
    /utf-8
    /openmp:llvm)

include_directories(
        src
        build/vendor/luajit/src
        build/vendor/sol2/include)

file(GLOB_RECURSE BLDR_SRC_FILES src/*.cpp src/*.hpp)
add_executable(${PROJECT_NAME} ${BLDR_SRC_FILES})

target_link_libraries(
        ${PROJECT_NAME}
        ${CMAKE_SOURCE_DIR}/build/vendor/LuaJIT/src/luajit.lib
        ${CMAKE_SOURCE_DIR}/build/vendor/LuaJIT/src/lua51.lib)

add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_SOURCE_DIR}/bin/bldr.exe)