
SRCS =  picoc.c table.c lex.c parse.c expression.c heap.c type.c \
	variable.c clibrary.c platform.c include.c debug.c \
	platform/platform_unix.c platform/library_unix.c \
	cstdlib/stdio.c cstdlib/math.c cstdlib/string.c cstdlib/stdlib.c \
	cstdlib/time.c cstdlib/errno.c cstdlib/ctype.c cstdlib/stdbool.c \
	cstdlib/unistd.c cstdlib/libgen.c cstdlib/fcntl.c \
        picoc_ezApp.c

LIBS = -L../../linux/local/lib -lezApp_lib -lmp3lame -llodepng -lcJSON -lkissfft-float -lSDL3 -lSDL3_ttf -lSDL3_mixer -lm \
       -Wl,-rpath,$(PWD)/../../linux/local/lib

picoc:
	gcc -o picoc -Wall -Werror -O2 -g -DUNIX_HOST -I../../linux/local/include $(SRCS) $(LIBS)

clean:
	rm -f picoc
	rm -f libgen/libgen libgen/libgen_out1.h libgen/libgen_out2.h
	rm -f tests/fred.txt

.PHONY: picoc
