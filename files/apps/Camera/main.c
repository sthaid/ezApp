// xxx
// - rename util_take_picture ?  to util_take_photo
// - replace 'TAKE' with a circle
// - cleanup needed?

#include <stdio.h>
#include <stdbool.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"

// defines
#define MAX_PHOTOS 1000

#define GALLERY_VIEW  0
#define LOCATION_VIEW 1

// variables
char *progname;
char *data_dir;
bool  end_program;

int   view = GALLERY_VIEW;

int   photos[MAX_PHOTOS];
int   max_photos;
int   last_photo_num;
    
// prototypes
int init(void);
void photo_gallery(void);
void photo_location(void);
void settings(void);
int photo_take(void);
int photo_delete(int num);

// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
    int rc;

    // verify arg count
    if (argc != 2) {
        printf("E %s: argc=%d is not 2\n", "Camera", argc);
        return 1;
    }

    // save args
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // initialize
    rc = init();
    if (rc != 0) {
        printf("E %s: init failed\n", progname);
        return 1;
    }

    // runtime
    while (!end_program) {
        switch (view) {
        case GALLERY_VIEW:
            photo_gallery();
            break;
        case LOCATION_VIEW:
            photo_location();
            break;
        default:
            printf("E %s: invalid view %d\n", progname, view);
            end_program = 1;
        }
    }

    // cleanup and terminate
    printf("I %s: terminating\n", progname);
    return 0;
}

int init(void)
{
    //next_photo_num = 1;

    //if (max_list == 0) {
    //}
    return 0;
}

// ------------------ PHOTO GALLERY --------------------

#define EVID_DEL   1
#define EVID_FAV   2
#define EVID_VIEW  3
#define EVID_STG   4
#define EVID_TAKE  5

void photo_gallery(void)
{
    sdlx_event_t event;
    int y;
    bool done = false;
    bool del_mode = false;
    bool favorites_mode = false;

    while (!done && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // register events
        reg_event_show_readme_file();
        y = sdlx_win_height - sdlx_char_height_dflt;
        reg_event(0, y, COLOR_LIGHT_BLUE, "DEL", EVID_DEL);
        reg_event(sdlx_win_width/2-1.5*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "FAV", EVID_FAV);
        reg_event(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "VIEW", EVID_VIEW);
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_STG, "STG",
                                     EVID_TAKE, "TAKE",
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
            show_file(data_dir, "README");
            break;

        case EVID_STG:
            settings();
            break;
        case EVID_TAKE:
            photo_take();
            break;
        case EVID_QUIT:
            end_program = true;
            break;

        case EVID_DEL:
            del_mode = !del_mode;
            break;
        case EVID_FAV:
            favorites_mode = !favorites_mode;
            break;
        case EVID_VIEW:
            view = LOCATION_VIEW;
            done = true;
            break;

        case EVID_MOTION:
            break;
        }
    }
}

// ------------------ PHOTO LOCATION -------------------

#define EVID_CTR   6

void photo_location(void)
{
    sdlx_event_t event;
    int y;
    bool done = false;
    bool favorites_mode = false;

    while (!done && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // register events
        reg_event_show_readme_file();
        y = sdlx_win_height - sdlx_char_height_dflt;
        reg_event(0, y, COLOR_LIGHT_BLUE, "CTR", EVID_CTR);
        reg_event(sdlx_win_width/2-1.5*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "FAV", EVID_FAV);
        reg_event(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "VIEW", EVID_VIEW);
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_STG, "STG",
                                     EVID_TAKE, "TAKE",
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
            show_file(data_dir, "README");
            break;

        case EVID_STG:
            settings();
            break;
        case EVID_TAKE:
            photo_take();
            break;
        case EVID_QUIT:
            end_program = true;
            break;

        case EVID_CTR:
            // xxx
            break;
        case EVID_FAV:
            favorites_mode = !favorites_mode;
            break;
        case EVID_VIEW:
            view = GALLERY_VIEW;
            done = true;
            break;

        case EVID_MOTION:
            break;
        }
    }
}

// ------------------ SETTINGS -------------------------

void settings(void)
{
    return;
}

// ------------------ SUPPORT --------------------------

int photo_take(void)
{
    // take photo
    util_take_picture();  

    // move photo from tmp/photo.jpg to photos subdir

    // create photo meta data file

    return 0;
}
    
int photo_delete(int num)
{
    // delete photo jpg and meta files
    return 0;
}


