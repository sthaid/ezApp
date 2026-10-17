#include <std_hdrs.h>

#include <sdlx.h>
#include <private.h>

static bool app_running = false;

// can not '#include <picoc_ezApp.h>' here because of a circular
// build dependency between ezApp_lib and picoc
extern int picoc_ezApp(char *args);

static void remove_trailing_newline(char *s);

int run(char *name, bool is_svc)
{
    char  dir_path[100], cmd[200], s[200], picoc_args[10000], *p;
    int   rc;
    FILE *fp;

    // construct path to the directory of the app or svc being run
    if (!is_svc) {
        sprintf(dir_path, "apps/%s", name);
    } else {
        sprintf(dir_path, "svcs/%s", name);
    }

    // construct list of *.c files in dir_path
    // /bin/ls -1 is used to provide the same order of .c files as 
    // when picoc is being run by eztest
    p = picoc_args;
    picoc_args[0] = '\0';
    sprintf(cmd, "/bin/ls -1 %s/*.c", dir_path);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        ERROR("%s: failed to list *.c in %s\n", name, dir_path);
        sdlx_show_toast("NO SOURCE CODE");
        return 99;
    }
    while (fgets(s, sizeof(s), fp) != NULL) {
        remove_trailing_newline(s);
        p += sprintf(p, "%s ", s);
    }
    pclose(fp);

    // error if no source code found in dir_path
    if (picoc_args[0] == '\0') {
        ERROR("%s: no source code in %s\n", name, dir_path);
        sdlx_show_toast("NO SOURCE CODE");
        return 99;
    }

    // add lib/lib.c to list of C src files to be run by picoc
    p += sprintf(p, "%s", "lib/lib.c ");

    // add progname and data_dir args, which will be passed to the
    // app or svc which will be run by picoc
    p += sprintf(p, " - %s %s", name, dir_path);

    // invalidate cached params, this may be needed if a params 
    // file is changed as part of a miniApp update
    util_invalidate_cached_params();

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

static void remove_trailing_newline(char *s)
{
    int len = strlen(s);

    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }
}

