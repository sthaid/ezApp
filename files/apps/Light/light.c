#include <stdio.h>
#include <stdbool.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/lib/lib.h"

// defines
#define EVID_SET_COLOR_WHITE 1
#define EVID_SET_COLOR_RED   2

// variables
char *progname;
char *data_dir;
    
// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
    bool done = false;
    sdlx_color_t color;
    sdlx_event_t event;

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // get color from param store; set to COLOR_WHITE if not in params
    color = util_get_numeric_param(data_dir, "color", COLOR_WHITE);

    // runtime loop
    while (!done) {
        // init the backbuffer
        sdlx_display_init(color, PORTRAIT);

        // register EVID_SHOW_README_FILE event
        reg_event_show_readme_file();

        // register control events to
        // - set color either to white or red, or
        // - end program
        sdlx_register_control_events(EVID_SET_COLOR_WHITE, "W",
                                     EVID_SET_COLOR_RED,   "R",
                                     EVID_QUIT,            "X");

        // present the display
        sdlx_display_present();

        // wait for event, with infinite timeout
        sdlx_get_event(-1, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_SET_COLOR_WHITE:
            color = COLOR_WHITE;
            util_set_numeric_param(data_dir, "color", color);
            break;
        case EVID_SET_COLOR_RED:
            color = COLOR_RED;
            util_set_numeric_param(data_dir, "color", color);
            break;
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_QUIT:
            done = true;
            break;
        }
    }

    // cleanup and end program
    printf("I %s: terminating\n", progname);
    return 0;
}
