#include <stdio.h>
#include <stdbool.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"
#include "svcs/Location/location.h"

//
// defines
//

//
// variables
//

char *progname;
char *data_dir;

loc_hist2_t *lh2;
    
//
// prototypes
//

void draw_map(void);

// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
    sdlx_event_t event;
    bool         end_program = false;

    // save args
    if (argc != 2) {
        printf("E %s: argc=%d is not 2\n", "Example1", argc);
        return 1;
    }
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // map location history file
    // - create_if_needed = false
    // - created (return flag) = NULL
    lh2 = util_map_file("svcs/Location", LOC_HIST2_FILENAME, sizeof(loc_hist2_t), false, NULL);
    if (lh2 == NULL) {
        printf("E: %s failed to map %s\n", progname, LOC_HIST2_FILENAME);
        return 1;
    }

    // runtime loop
    while (!end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // draw map
        draw_map();

        // register EVID_SHOW_README_FILE event; 
        reg_event_show_readme_file();

        // register control event to end program
        sdlx_register_control_events(0, NULL,
                                     0, NULL,
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with infinite timeout
        sdlx_get_event(-1, &event);

        // process events
        switch (event.event_id) {
        case EVID_SHOW_README_FILE:
            // display the README file
            show_file(data_dir, "README");
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

void draw_map(void)
{
    struct tm *tm;
    long idx;
    char time_str[100];
    struct loc_hist2_entry_s *loc;

    if (lh2->tail == 0) {
        printf("E %s: no data\n", progname);
        return;
    }

    idx = lh2->tail;
    while (true) {
        if (--idx < 0) {
            break;
        }

        loc = &lh2->loc[idx % MAX_LOC_HIST2];

        tm = localtime(&loc->t);
        strftime(time_str, sizeof(time_str), "%b %d %H:%M %Z", tm);

        printf("I %s: %s %0.3f %0.3f\n",
               progname, 
               time_str, loc->latitude, loc->longitude);
    }
}
