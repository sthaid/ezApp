// EXAPP this is picoc entrypoint, called by src/ezApp/main.c

#include "picoc.h"

#define PICOC_STACK_SIZE (128000*4)  // 512,000 bytes

int picoc_ezApp(char *args)
{
    Picoc pc;
    char  args_copy[1000];
    char *argv[20];
    char *s;
    int   argc = 0;
    bool  processing_argv_args = false;
    bool  first = true;
    char *saveptr;

    // init pc
    PicocInitialize(&pc, PICOC_STACK_SIZE);

    // setjmp for error condition
    if (PicocPlatformSetExitPoint(&pc)) {
        printf("E picoc: '%s' longjmp error exit %d\n", args, pc.PicocExitValue);
        PicocCleanup(&pc);
        return pc.PicocExitValue;
    }

    // tokenize args
    strcpy(args_copy, args);
    while (true) {
        s = strtok_r(first ? args_copy : NULL, " ", &saveptr);
        first = false;
        if (s == NULL) {
            break;
        }

        if (strcmp(s, "-") == 0) {
            processing_argv_args = true;
            continue;
        }

        if (!processing_argv_args) {
            //printf("I picoc: scanning %s\n", s);
            PicocPlatformScanFile(&pc, s);
        } else {
            //printf("I picoc: adding argv[%d] = %s\n", argc, s);
            argv[argc++] = s;
        }
    }
    
    // run program
    PicocCallMain(&pc, argc, argv);

    // cleanup and return
    printf("I picoc: normal exit, exit_code=%d\n", pc.PicocExitValue);
    PicocCleanup(&pc);
    return pc.PicocExitValue;
}
