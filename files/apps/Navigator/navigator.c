#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"
#include "svcs/Location/location.h"

// xxx
// - new data format
// - view mode
//   - controls < > X
//   - pan zoom
//   - show history
//   - show places
// 

//
// defines
//

#define ONE_SEC 1000000

#define MODE_VIEW 0
#define MODE_NAV  1

//
// variables
//

char *progname;
char *data_dir;

bool         end_program = false;
int          mode;
loc_hist2_t *lh2;
    
//
// prototypes
//

void view_mode_display(void);
void nav_mode_display(void);

// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
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
        if (mode == MODE_VIEW) {
            view_mode_display();
        } else {
            nav_mode_display();
        }
    }

    // cleanup and end program
    util_unmap_file(lh2, sizeof(loc_hist2_t));
    printf("I %s: terminating\n", progname);
    return 0;
}

// --------------------------------------------------------

#define DEG2RAD 0.0174533

// xxx utils
double cosd(double angle)
{
    return cos(angle * DEG2RAD);
}

// xxx
// - what if no data at all, somehow display NO DATA

#define EVID_NEXT 1
#define EVID_PREV 2

#define MAP_XL     0
#define MAP_XR     999
#define MAP_W      1000
#define MAP_X_CTR  500

#define MAP_YT     0
#define MAP_YB     1499
#define MAP_H      1500
#define MAP_Y_CTR  750

#define MILES_PER_DEGREE_LATITUDE       69.0
#define MILES_PER_DEGREE_LONGITUDE(lat) (cosd(lat) * 69.0)

// xxx can these be local vars
double map_ctr_lat, map_ctr_long;
double map_width_miles, map_height_miles;
int day;

void display_trail(struct loc_hist2_day_s *lh2d);

void view_mode_display(void)
{
    sdlx_event_t event;

    day = lh2->last_day;
    util_get_location(&map_ctr_lat, &map_ctr_long, NULL, NULL);
    map_width_miles  = 10;
    map_height_miles = map_width_miles * ((double)MAP_H / MAP_W);
    printf("I %s: view mode starting, map %0.3f,%0.3f WxH %0.0f %0.0f miles\n",
           progname, map_ctr_lat, map_ctr_long, map_width_miles, map_height_miles);

    do {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // xxx comment
        sdlx_render_rect(MAP_XL, MAP_YT, MAP_W, MAP_H, 5, COLOR_GREEN);
        display_trail(&lh2->day[day % MAX_LH2_DAY]);
        
        // register events
        sdlx_register_event(NULL, EVID_PINCH);
        sdlx_register_event(NULL, EVID_MOTION);
        reg_event_show_readme_file();
        sdlx_register_control_events(EVID_PREV, "<", EVID_NEXT, ">", EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with 10 sec timeout
        sdlx_get_event(10*ONE_SEC, &event);

        // process events
        switch (event.event_id) {
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_PREV:
            if (day > 0) day--;
            break;
        case EVID_NEXT:
            if (day < lh2->last_day) day++;
            break;
        case EVID_MOTION:
            map_ctr_lat  += event.u.motion.yrel * 
                            (map_height_miles / MAP_H) / 
                            MILES_PER_DEGREE_LATITUDE;
            map_ctr_long -= event.u.motion.xrel * 
                            (map_width_miles / MAP_W) / 
                            MILES_PER_DEGREE_LONGITUDE(map_ctr_lat);
            break;
        case EVID_PINCH:
            // xxx limit scaling
            if (event.u.pinch.scale == 0) break;
            map_width_miles /= event.u.pinch.scale;
            map_height_miles = map_width_miles * ((double)MAP_H / MAP_W);
            break;
        case EVID_QUIT:
            end_program = true;
            break;
        }
    } while (!end_program && mode == MODE_VIEW);
}

// xxx test and cleanup
#define HOUR 3600

void display_trail(struct loc_hist2_day_s *lh2d)
{
    int          x, y, i, num_points=0;
    sdlx_color_t color;
    double       k_lat, k_lng, wvlen;

    k_lat = MILES_PER_DEGREE_LATITUDE / map_height_miles * MAP_H;
    k_lng = MILES_PER_DEGREE_LONGITUDE(map_ctr_lat) / map_width_miles * MAP_W;

    // xxx
    // make a points array,  but then can't control color
    // consolidate pinch events
    // further optimize this code
    for (i = 0; i < lh2d->max_loc; i++) {
        y = MAP_Y_CTR - (lh2d->loc[i].latitude - map_ctr_lat) * k_lat;
        if (y < MAP_YT || y > MAP_YB) continue;

        x = MAP_X_CTR + (lh2d->loc[i].longitude - map_ctr_long) * k_lng;
        if (x < MAP_XL || x > MAP_XR) continue;

        if (lh2d->loc[i].secs < (6 * HOUR)) {
            wvlen = 380.;  // purple
        } else if (lh2d->loc[i].secs > (21 * HOUR)) {
            wvlen = 700.;  // red
        } else {
            wvlen = 380. + 
                    (lh2d->loc[i].secs - (6.0 * HOUR)) / (15.0 * HOUR) *
                    320.;
        }
        color = sdlx_wavelength_to_color(wvlen);

        sdlx_render_point(x, y, color, MAX_POINT_SIZE);

        num_points++;
    }

    printf("num points %d\n", num_points);
    sdlx_render_printf(0, MAP_YB, "num points %d\n", num_points);
}

// --------------------------------------------------------

void nav_mode_display(void)
{
}


// ====================================================
// ================= TEMP SAVE ========================
// ====================================================


#if 0
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
#endif
