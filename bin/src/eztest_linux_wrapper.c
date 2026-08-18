#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <private.h>

#undef main

extern int MAIN(int argc, char **argv);

int main(int argc, char **argv)
{
    int rc;
    char *progname, *data_dir;

    // set line buffering
    setlinebuf(stdout);

    // adjust argv[0] to remove the leading '/tmp/'
    if (strncmp(argv[0], "/tmp/", 5) == 0) {
        argv[0] += 5;
    }
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: eztest_linux starting\n", progname);

    // if running an app then call sdlx_init
    if (strncmp(data_dir, "apps/", 5) == 0) {
        rc = sdlx_init(SUBSYS_VIDEO|SUBSYS_AUDIO|SUBSYS_SENSOR, 0);
        if (rc != 0) {
            printf("E %s: sdlx_init failed\n", progname);
            return 1;
        }
    }

    // call the mini app main routine
    rc = MAIN(argc, argv);

    // if running an app then call sdlx_quit
    if (strncmp(data_dir, "apps/", 5) == 0) {
        sdlx_quit(SUBSYS_VIDEO|SUBSYS_AUDIO|SUBSYS_SENSOR);
    }

    // if running an app then call sdlx_quit
    if (strncmp(data_dir, "apps/", 5) == 0) {
        sdlx_quit(SUBSYS_VIDEO|SUBSYS_AUDIO|SUBSYS_SENSOR);
    }

    // print result and return status
    if (rc == 0) {
        printf("I eztest_linux: normal termination\n");
    } else {
        printf("E eztest_linux: error termination\n");
    }
    return rc;
}

int picoc_ezApp(char *args)
{
    printf("E eztest_linux: stub picoc_ezApp should not be called\n");
    exit(1);
}
