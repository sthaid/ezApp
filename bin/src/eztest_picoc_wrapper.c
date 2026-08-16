#include <string.h>

setlinebuf(stdout);

char *argv[2];
argv[0] = "PROGNAME";
argv[1] = "DATA_DIR";

printf("I %s: eztest_picoc starting\n", argv[0]);

// if running an app then call sdlx_init
if (strncmp(argv[1], "apps/", 5) == 0) {
    int rc = sdlx_init(SUBSYS_VIDEO|SUBSYS_AUDIO|SUBSYS_SENSOR, DEFAULT_ASPECT_RATIO);
    if (rc != 0) {
        printf("E %s: sdlx_init failed\n", argv[0]);
        return 1;
    }
}

// call app/svc main routine
main(2, argv);

// if running an app then call sdlx_quit
if (strncmp(data_dir, "apps/", 5) == 0) {
    sdlx_quit(SUBSYS_VIDEO|SUBSYS_AUDIO|SUBSYS_SENSOR);
}

// done
printf("I %s: eztest_picoc terminating\n", argv[0]);
