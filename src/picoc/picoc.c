/* picoc main program - this varies depending on your operating system and
 * how you're using picoc */
/* platform-dependent code for running programs is in this file */
#if defined(UNIX_HOST) || defined(WIN32)
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#endif

/* include only picoc.h here - should be able to use it with only the
    external interfaces, no internals from interpreter.h */
#include "picoc.h"

#if defined(UNIX_HOST) || defined(WIN32)
#include "LICENSE.h"

/* Override via STACKSIZE environment variable */
#define PICOC_STACK_SIZE (128000*4)

static char *progname;

int main(int argc, char **argv)
{
    int ParamCount = 1;
    int DontRunMain = false;
    int StackSize = getenv("STACKSIZE") ? atoi(getenv("STACKSIZE")) : PICOC_STACK_SIZE;
    Picoc pc;

    if (argc < 2 || strcmp(argv[ParamCount], "-h") == 0) {
        printf(PICOC_VERSION "  \n"
               "Format:\n\n"
               "> picoc [-n <progname>] <file1.c>... [- <arg1>...]\n"
               "                                      : run a program, calls main() as the entry point\n"
               "> picoc -s <file1.c>... [- <arg1>...] : run a script, runs the program without calling main(),\n"
               "                                        automatically includes all system header files\n"
               "> picoc -S <file1.c>... [- <arg1>...] : run a script, runs the program without calling main(),\n"
               "                                        does not include system header files\n"
               "> picoc -i                            : interactive mode, Ctrl+d to exit\n"
               "> picoc -c                            : copyright info\n"
               "> picoc -h                            : this help message\n");
        return 0;
    }

    if (strcmp(argv[ParamCount], "-c") == 0) {
        printf("%s\n", (char*)&__LICENSE);
        return 0;
    }

    PicocInitialize(&pc, StackSize);

    if (strcmp(argv[ParamCount], "-n") == 0) {
        ParamCount++;
        progname = argv[ParamCount];
        ParamCount++;
    }

    if (strcmp(argv[ParamCount], "-s") == 0) {
        DontRunMain = true;
        PicocIncludeAllSystemHeaders(&pc);
        ParamCount++;
    }

    if (strcmp(argv[ParamCount], "-S") == 0) {
        DontRunMain = true;
        ParamCount++;
    }

    if (argc > ParamCount && strcmp(argv[ParamCount], "-i") == 0) {
        PicocIncludeAllSystemHeaders(&pc);
        PicocParseInteractive(&pc);
    } else {
        if (PicocPlatformSetExitPoint(&pc)) {
            PicocCleanup(&pc);
            return pc.PicocExitValue;
        }

        for (; ParamCount < argc && strcmp(argv[ParamCount], "-") != 0; ParamCount++)
            PicocPlatformScanFile(&pc, argv[ParamCount]);

        if (!DontRunMain) {
            if (argc - ParamCount > 0 &&
                strcmp(argv[ParamCount], "-") == 0 &&
                progname != NULL)
            {
                argv[ParamCount] = progname;
            }
            PicocCallMain(&pc, argc - ParamCount, &argv[ParamCount]);
        }
    }

    PicocCleanup(&pc);
    return pc.PicocExitValue;
}
#endif
