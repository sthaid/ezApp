cmake_minimum_required(VERSION 3.16)

project(lodepng)

set(CMAKE_C_FLAGS "-O2 -Wall -Werror -g")

set(LIB_SRCS
    lodepng.c
)

set(LIB_HEADERS
    include/lodepng.h
)

add_library(lodepng STATIC ${LIB_SRCS} ${LIB_HEADERS})

target_include_directories(lodepng PUBLIC include)

install(FILES include/lodepng.h DESTINATION include)

install(TARGETS lodepng ARCHIVE DESTINATION lib)

