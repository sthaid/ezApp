cmake_minimum_required(VERSION 3.16)

project(ezApp_lib)

set(CMAKE_C_FLAGS "-O2 -Wall -Werror -Wno-unused-result -g")

set(LIB_SRCS
    sdlx_audio.c
    sdlx_event.c
    sdlx_misc.c
    sdlx_sensor.c
    sdlx_video.c
    svcs.c
    utils.c
    utils_openssl.c
    utils_android.cpp
    utils_jpeg_decode.c
    logging.c
    run.c
)

set(LIB_HEADERS
    include/sdlx.h
    include/svcs.h
    include/utils.h
    include/private.h
)

set(INCLUDE_PATH "")
cmake_path(APPEND INCLUDE_PATH "${CMAKE_INSTALL_PREFIX}" include)

include_directories(. ./include ${INCLUDE_PATH})

add_library(ezApp_lib STATIC ${LIB_SRCS} ${LIB_HEADERS})

install(FILES include/sdlx.h include/svcs.h include/utils.h include/private.h
        DESTINATION include)

install(TARGETS ezApp_lib ARCHIVE DESTINATION lib)

