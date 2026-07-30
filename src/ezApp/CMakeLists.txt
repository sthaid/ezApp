cmake_minimum_required(VERSION 3.16)

project(ezApp)

set(INCLUDE_PATH "")
cmake_path(APPEND INCLUDE_PATH "${CMAKE_PREFIX_PATH}" include)

set(LIB_PATH "")
cmake_path(APPEND LIB_PATH "${CMAKE_PREFIX_PATH}" lib)

set(CMAKE_C_FLAGS "-O2 -Wall -Wno-unused-result -Werror -g -Wl,-rpath,${LIB_PATH}")

include_directories(${INCLUDE_PATH} .)

add_executable(ezApp 
    main.c
        )

target_link_libraries(ezApp PRIVATE 
    ${LIB_PATH}/libezApp_lib.a
    ${LIB_PATH}/libSDL3.so
    ${LIB_PATH}/libSDL3_ttf.so
    ${LIB_PATH}/libSDL3_mixer.so
    ${LIB_PATH}/libpicoc.a
    ${LIB_PATH}/libmp3lame.a
    ${LIB_PATH}/libkissfft-float.a
    ${LIB_PATH}/libcJSON.a
    ${LIB_PATH}/liblodepng.a
    m crypto)

