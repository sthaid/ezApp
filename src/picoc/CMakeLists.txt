cmake_minimum_required(VERSION 3.16)

project(picoc)

set(INCLUDE_PATH "")
cmake_path(APPEND INCLUDE_PATH "${CMAKE_INSTALL_PREFIX}" include)

set(CMAKE_C_FLAGS "-O2 -DUNIX_HOST -Wall -Werror -g")

include_directories(${INCLUDE_PATH} .)

set(LIB_SRCS
    clibrary.c 
    debug.c 
    expression.c 
    heap.c 
    include.c 
    lex.c 
    parse.c 
    picoc_ezApp.c 
    platform.c 
    table.c 
    type.c 
    variable.c 
    cstdlib/ctype.c 
    cstdlib/errno.c 
    cstdlib/math.c 
    cstdlib/stdbool.c 
    cstdlib/stdio.c 
    cstdlib/stdlib.c 
    cstdlib/string.c 
    cstdlib/time.c 
    cstdlib/unistd.c 
    cstdlib/libgen.c 
    cstdlib/fcntl.c 
    platform/library_unix.c 
    platform/platform_unix.c
)

set(LIB_HEADERS
    include/picoc_ezApp.h
)

add_library(picoc STATIC ${LIB_SRCS} ${LIB_HEADERS})

target_include_directories(picoc PUBLIC include)

install(FILES include/picoc_ezApp.h DESTINATION include)

install(TARGETS picoc ARCHIVE DESTINATION lib)

