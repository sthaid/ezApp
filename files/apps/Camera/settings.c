#include "apps/Camera/common.h"

// -----------------  SETTINGS  ---------------------

void settings(void)
{
    // runtime loop
    while (!done) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // xxx
        get_storage_usage(photos_dir);
        get_storage_avail();

        // display 'Hello World'
        sdlx_render_printf_ex(sdlx_win_width/2, sdlx_win_height*0.25,
                               FONT_LARGE, COLOR_PURPLE, FLAG_XY_CTR, 
                               "%s", "Hello\nWorld");

        // register control event to end program
        sdlx_register_control_events(0, NULL,
                                     0, NULL,
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with infinite timeout
        sdlx_get_event(-1, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_QUIT:
            done = true;
            break;
        }
    }
    // xxx todo
    return;
}

unsigned long get_storage_usage(char *path)
{
    unsigned long k;
    char s[200];

    s[0] = '\0';
    k = 0;
    sprintf(cmd, "du -s %s", path);
    fp = popen(cmd, "r");
    fgets(s, sizeof(s), fp);
    sscanf(s, "%ld", &k);
    pclose(fp);

    return k;
}

unsigned long get_storage_avail(void)
{
}
