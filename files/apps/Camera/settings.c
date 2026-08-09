#include "apps/Camera/common.h"

int get_usage_at_path(char *path, long *k);
int get_filesystem_usage_info(char *path, long *size, long *used, long *avail, int *percent_used);

// -----------------  SETTINGS  ---------------------

#define K2G(k) ((k) / 1048576.)

void settings(void)
{
    bool done = false;
    int  rc, percent_used;
    sdlx_event_t event;
    long used, avail, size;

    // loop until done flag is set
    while (!done) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // display the usage in the photos dir
        sdlx_render_printf(0, ROW2Y(1), "Photos:");
        rc = get_usage_at_path(photos_dir, &used);
        if (rc == 0) {
            if (K2G(used) < 1) {
                sdlx_render_printf(0, ROW2Y(2), " Used  %0.3f G", K2G(used));
            } else {
                sdlx_render_printf(0, ROW2Y(2), " Used  %3.0f G", K2G(used));
            }
        } else {
            sdlx_render_printf(0, ROW2Y(2), " Used  ?");
        }

        // display filesystem usage info
        sdlx_render_printf(0, ROW2Y(4), "Filesystem:");
        rc = get_filesystem_usage_info(".", &size, &used, &avail, &percent_used);
        if (rc == 0) {
            sdlx_render_printf(0, ROW2Y(5), " Size  %3.0f G", K2G(size));
            sdlx_render_printf(0, ROW2Y(6), " Used  %3.0f G  %d%%", K2G(used), percent_used);
            sdlx_render_printf(0, ROW2Y(7), " Avail %3.0f G", K2G(avail));
        } else {
            sdlx_render_printf(0, ROW2Y(5), " Total ?");
            sdlx_render_printf(0, ROW2Y(6), " Used  ?");
            sdlx_render_printf(0, ROW2Y(7), " Avail ?");
        }

        // register control event exit settings
        sdlx_register_control_events(0, NULL,
                                     0, NULL,
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with infinite timeout
        sdlx_get_event(-1, &event);

        // process events
        switch (event.event_id) {
        case EVID_QUIT:
            done = true;
            break;
        }
    }
}

// return value units: 1024 bytes
int get_usage_at_path(char *path, long *used)
{
    char  s[200], cmd[200];
    FILE *fp;
    int   cnt;

    // example:
    // - ezsh Camera> du -sk photos
    //   319980	photos

    s[0] = '\0';
    *used = INVALID_NUMBER;

    sprintf(cmd, "du -sk %s", path);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        return -1;
    }
    fgets(s, sizeof(s), fp);
    cnt = sscanf(s, "%ld", used);
    pclose(fp);

    return cnt == 1 ? 0 : -1;
}

// return value units: 1024 bytes
int get_filesystem_usage_info(char *path, long *size, long *used, long *avail, int *percent_used)
{
    char  s[200], cmd[200];
    FILE *fp;
    int   cnt;

    // example:
    // - ezsh Camera> df -k .
    //   Filesystem       1K-blocks     Used Available Use% Mounted on
    //   /dev/block/dm-63 233963468 53684860 180147536  23% /data/misc/profiles/ref/org.sthaid.ezApp

    s[0] = '\0';
    *used = INVALID_NUMBER;
    *avail = INVALID_NUMBER;
    *percent_used = INVALID_NUMBER;

    sprintf(cmd, "df -k %s", path);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        return -1;
    }
    fgets(s, sizeof(s), fp);
    fgets(s, sizeof(s), fp);
    cnt = sscanf(s, "%*s %ld %ld %ld %d", size, used, avail, percent_used);
    pclose(fp);

    return cnt == 4 ? 0 : -1;
}
