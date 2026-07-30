cmake_minimum_required(VERSION 3.16)

project(mp3lame)

set(CMAKE_C_FLAGS "-DHAVE_CONFIG_H -O2 -Wall -g")

set(LIB_SRCS
    libmp3lame/bitstream.c
    libmp3lame/encoder.c
    libmp3lame/fft.c
    libmp3lame/gain_analysis.c
    libmp3lame/id3tag.c
    libmp3lame/lame.c
    libmp3lame/mpglib_interface.c
    libmp3lame/newmdct.c
    libmp3lame/presets.c
    libmp3lame/psymodel.c
    libmp3lame/quantize.c
    libmp3lame/quantize_pvt.c
    libmp3lame/reservoir.c
    libmp3lame/set_get.c
    libmp3lame/tables.c
    libmp3lame/takehiro.c
    libmp3lame/util.c
    libmp3lame/vbrquantize.c
    libmp3lame/VbrTag.c
    libmp3lame/version.c
)

set(LIB_HEADERS
    include/lame.h
)

add_library(mp3lame STATIC ${LIB_SRCS} ${LIB_HEADERS})

target_include_directories(mp3lame PUBLIC include PRIVATE .)

install(FILES include/lame.h DESTINATION include)

install(TARGETS mp3lame ARCHIVE DESTINATION lib)

