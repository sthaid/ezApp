#include <stdio.h>
#include <stdbool.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/lib/lib.h"

// event ids
#define EVID_TAPME 1

// variables
char *progname;
char *data_dir;
    
// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
    sdlx_event_t event;
    sdlx_loc_t  *loc;
    int          tapme_count = 0;
    bool         end_program = false;

    // verify arg count
    if (argc != 2) {
        printf("E %s: argc=%d is not 2\n", "Template", argc);
        return 1;
    }

    // save args
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // runtime loop
    while (!end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // display 'Hello World'
        sdlx_render_printf_ex2(sdlx_win_width/2, sdlx_win_height*0.25,
                               FONT_LARGE, COLOR_PURPLE, FLAG_XY_CTR, 
                               "%s", "Hello\nWorld");

        // display "TAPME" in COLOR_LIGHT_BLUE; and register EVID_TAPME event
        loc = sdlx_render_printf_ex1(300, 1600, 
                                     FONT_NORMAL, COLOR_LIGHT_BLUE, 
                                     "TAPME-%d", tapme_count);
        sdlx_register_event(loc, EVID_TAPME);

        // register EVID_SHOW_README_FILE event; 
        // note: the reg_event_show_readme_file routine is defined in apps/lib/lib.c,
        //       which is automatically included in miniApps
        reg_event_show_readme_file();

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
        case EVID_SHOW_README_FILE:
            // display the README file
            show_file(data_dir, "README");
            break;
        case EVID_TAPME:
            // increment the tapme_count
            printf("I %s: got EVID_TAPME\n", progname);
            tapme_count++;
            break;
        case EVID_QUIT:
            // set end_program flag
            end_program = true;
            break;
        }
    }

    // cleanup and end program
    printf("I %s: terminating\n", progname);
    return 0;
}
