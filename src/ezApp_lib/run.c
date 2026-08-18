#include <std_hdrs.h>

#include <sdlx.h>
#include <private.h>

static bool app_running = false;

// can not '#include <picoc_ezApp.h>' here because of a circular
// build dependency between ezApp_lib and picoc
extern int picoc_ezApp(char *args);

int run(char *name, bool is_svc)
{
    char           dir_path[100];
    int            rc;
    DIR           *dir;
    struct dirent *dirent;
    char          *p;
    char           picoc_args[1000];

    // construct path to the directory of the app or svc being run
    if (!is_svc) {
        sprintf(dir_path, "apps/%s", name);
    } else {
        sprintf(dir_path, "svcs/%s", name);
    }

    // construct list of *.c files in the dir
    picoc_args[0] = '\0';
    dir = opendir(dir_path);
    if (dir == NULL) {
        ERROR("%s: failed to opendir %s, %s\n", name, dir_path, strerror(errno));
        return 99;
    }
    p = picoc_args;
    while ((dirent = readdir(dir)) != NULL) {
        char *fn = dirent->d_name;
        int len = strlen(fn);
        if (len > 2 && strcmp(fn+len-2, ".c") == 0) {
            p += sprintf(p, "%s/%s ", dir_path, fn);
        }
    }
    closedir(dir);

    // error if no source code found in dir_path
    if (picoc_args[0] == '\0') {
        ERROR("%s: no source code in %s\n", name, dir_path);
        sdlx_show_toast("NO SOURCE CODE");
        return 99;
    }

    // if running an app then add lib/lib.c
    if (!is_svc) {
        p += sprintf(p, "%s", "lib/lib.c ");
    }

    // add progname and data_dir args, which will be passed to the
    // app or svc which will be run by picoc
    p += sprintf(p, " - %s %s", name, dir_path);

    // run the app using the picoc c language interpreter
    INFO("%s: starting, args = %s\n", name, picoc_args);
    if (!is_svc) app_running = true;
    rc = picoc_ezApp(picoc_args);
    if (!is_svc) app_running = false;
    INFO("%s: completed, rc = %d\n", name, rc);

    // return completion status
    return rc;
}

bool is_app_running(void)
{
    return app_running;
}
