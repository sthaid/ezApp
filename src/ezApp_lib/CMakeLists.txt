cmake_minimum_required(VERSION 3.16)

project(ezApp_lib)

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

# this applies the core flags to BOTH C and C++ files in this library
target_compile_options(ezApp_lib PRIVATE -Wall -Werror -Wno-unused-result -g -O2)

install(FILES include/sdlx.h include/svcs.h include/utils.h include/private.h
        DESTINATION include)
install(TARGETS ezApp_lib ARCHIVE DESTINATION lib)

