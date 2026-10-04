#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include <sdlx.h>
#include <utils.h>
#include <profile.h>

#include "lib/lib.h"
#include "svcs/Location/location.h"

// xxx
// - view mode todo
//   - show places
// - integrate lat/long during day

//
// defines
//

// xxx temp
//#define PROFILE

#define ONE_SEC 1000000

#define MODE_VIEW 0
#define MODE_NAV  1

#define K_SMOOTH 0.1

//
// variables
//

char         *progname;
char         *data_dir;

bool         end_program = false;
double       mag_decl;
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
        printf("E %s: argc=%d is not 2\n", "Navigator", argc);
        return 1;
    }
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // get mag decl
    mag_decl = get_mag_decl();
    if (mag_decl == INVALID_NUMBER) {
        printf("E %s: failed to get mag_decl, terminating\n", progname);
        return 1;
    }

    // map location history file, v2
    // - create_if_needed = false
    // - created (return flag) = NULL
    lh2 = util_map_file("svcs/Location", LOC_HIST2_FILENAME, sizeof(loc_hist2_t), false, NULL);
    if (lh2 == NULL) {
        printf("E: %s failed to map %s\n", progname, LOC_HIST2_FILENAME);
        return 1;
    }

    // runtime loop
#ifdef PROFILE
    profile_start();
#endif
    while (!end_program) {
        if (mode == MODE_VIEW) {
            view_mode_display();
        } else {
            nav_mode_display();
        }
    }
#ifdef PROFILE
    profile_stop(1);
#endif

    // cleanup and end program
    util_unmap_file(lh2, sizeof(loc_hist2_t));
    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  VIEW MODE  --------------------------

#define EVID_NEXT 1
#define EVID_PREV 2

// xxx defines may need work
#define MAP_XL     -250
#define MAP_XR     1249
#define MAP_W      1500
#define MAP_X_CTR  750

#define MAP_YT     0
#define MAP_YB     1499
#define MAP_H      1500
#define MAP_Y_CTR  750

#define MILES_PER_DEGREE_LATITUDE       69.0
#define MILES_PER_DEGREE_LONGITUDE(lat) (cosd(lat) * 69.0)   // xxx profile problem is here

// xxx can these be local vars
double map_ctr_lat, map_ctr_long;
double map_wh_miles;
int    day, num_points;

void draw_loc_points(struct loc_hist2_day_s *lh2d);

// xxx comments needed
void view_mode_display(void)
{
    sdlx_event_t event;
    sdlx_texture_t *t;
    sdlx_loc_t dest;
    double mag_heading, true_heading;
    struct loc_hist2_day_s *lh2d;
    long t_now, t_last;

    day = lh2->last_day;
    util_get_location(&map_ctr_lat, &map_ctr_long, NULL, NULL);
    map_wh_miles = 3;
    t = sdlx_create_texture(1500,1500);
    t_last = util_microsec_timer();
    printf("I %s: view mode starting, map %0.3f,%0.3f wh %0.0f miles\n",
           progname, map_ctr_lat, map_ctr_long, map_wh_miles);

    do {
        // print time since last update
        t_now = util_microsec_timer();
        //printf("I %s: interval %ld ms\n", progname, (t_now - t_last) / 1000);
        t_last = t_now;

        // set ptr to loc_hist2 data for 'day'
        lh2d = &lh2->day[day % MAX_LH2_DAY];

        // draw location points to texture 't'
        sdlx_set_render_target(t);
        sdlx_clear_texture(t, COLOR_BLACK);
        draw_loc_points(lh2d);
        sdlx_set_render_target(NULL);

        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // get true_heading, from mag_heading and declination
        // xxx how to handle failure case?
        sdlx_sensor_read_mag_heading(&mag_heading, K_SMOOTH);
        if (mag_heading == INVALID_NUMBER) {
            //printf("E %s: failed to get mag_heading\n", progname);
            true_heading = 0;
        } else {
            true_heading = mag_heading + mag_decl;
            if (true_heading < 0) true_heading += 360;
            if (true_heading >= 360) true_heading -= 360;
        }
        //printf("I %s: mag_heading %0.0f mag_decl %0.0f true_heading %0.0f\n",
        //       progname, mag_heading, mag_decl, true_heading);

        // display the texture containint the loc points, rotated by true_heading
        dest.x = -250;
        dest.y = 0;
        dest.w = 1500;
        dest.h = 1500;
        sdlx_render_texture_rotated(t, NULL, &dest, -true_heading, NULL, FLIP_NONE);

        // the rotated texture will spill over the display area below the map,
        // so clear the display area below the map
        sdlx_render_fill_rect(0, MAP_YB+1, 1000, 2000-MAP_YB+1, COLOR_BLACK);

        // add annotations to map
        // - display true heading at top center of map
        // - display map width at bottom of map
        sdlx_render_printf_ex(500, 100, FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, "%0.0f", true_heading);
        sdlx_render_printf_ex(500, MAP_YB-sdlx_char_height(FONT_SMALL), FONT_SMALL, COLOR_WHITE, FLAG_X_CTR,
                              "<-- %0.1f miles -->", map_wh_miles * 1000 / 1500);

        // draw map border  xxx use defines
        sdlx_render_rect(0, 0, 1000, 1500, 5, COLOR_GREEN);

        // display info in the control/status area that is just below the map
        // xxx needs work
        sdlx_render_printf(0, MAP_YB + ROW2Y(0),
                        "pts %d day %d\n", 
                        num_points, day);
        sdlx_render_printf(0, MAP_YB + ROW2Y(1),
                        "%s %s %d", 
                        get_weekday_str(lh2d->year, lh2d->month, lh2d->day),
                        get_month_str(lh2d->month),
                        lh2d->day);
        
        // register events
        sdlx_register_event(NULL, EVID_PINCH);
        sdlx_register_event(NULL, EVID_MOTION);
        reg_event_show_readme_file();
        sdlx_register_control_events(EVID_PREV, "<", EVID_NEXT, ">", EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with 50 ms timeout; 
        // the short timeout is needed because the displayed map depends on device heading
        sdlx_get_event(50000, &event);

        // process events
        switch (event.event_id) {
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_PREV: {
            int new_day = day - 1;
            // xxx picoc doesnt support short circuit eval
            if ((new_day) >= 0 && (lh2->last_day - new_day < MAX_LH2_DAY)) {
                lh2d = &lh2->day[new_day % MAX_LH2_DAY];
                if (lh2d->max_loc > 0) {
                    day = new_day;
                }
            }
            break; }
        case EVID_NEXT:
            if (day < lh2->last_day) day++;
            break;
        case EVID_MOTION: {
            double xrel = event.u.motion.xrel * cosd(true_heading) - event.u.motion.yrel * sind(true_heading);
            double yrel = event.u.motion.xrel * sind(true_heading) + event.u.motion.yrel * cosd(true_heading);
            map_ctr_lat  += yrel * 
                            (map_wh_miles / MAP_H) / 
                            MILES_PER_DEGREE_LATITUDE;
            map_ctr_long -= xrel * 
                            (map_wh_miles / MAP_W) / 
                            MILES_PER_DEGREE_LONGITUDE(map_ctr_lat);
            break; }
        case EVID_PINCH:
            // xxx limit scaling
            if (event.u.pinch.scale == 0) break;
            map_wh_miles /= event.u.pinch.scale;
            break;
        case EVID_QUIT:
            end_program = true;
            break;
        }
    } while (!end_program && mode == MODE_VIEW);

    sdlx_destroy_texture(t);
}

#define HOUR 3600
#define MIN_WAVELEN  380.0  // purple
#define MAX_WAVELEN  650.0  // red

// xxx optimize
void draw_loc_points(struct loc_hist2_day_s *lh2d)
{
    int          x, y, i;
    sdlx_color_t color;
    double       k_lat, k_lng, wvlen;

    num_points = 0;

    k_lat = MILES_PER_DEGREE_LATITUDE / map_wh_miles * MAP_H;
    k_lng = MILES_PER_DEGREE_LONGITUDE(map_ctr_lat) / map_wh_miles * MAP_W;

    // xxx optimize
    // - make a points array,  but then can't control color
    for (i = 0; i < lh2d->max_loc; i++) {
        y = MAP_Y_CTR - (lh2d->loc[i].latitude - map_ctr_lat) * k_lat;
        if (y < 0 || y > 1500) continue;

        x = MAP_X_CTR + (lh2d->loc[i].longitude - map_ctr_long) * k_lng;
        if (x < 0 || x > 1500) continue;

        if (lh2d->loc[i].secs < (6 * HOUR)) {
            wvlen = MIN_WAVELEN;
        } else if (lh2d->loc[i].secs > (20 * HOUR)) {
            wvlen = MAX_WAVELEN;
        } else {
            wvlen = MIN_WAVELEN + 
                    (lh2d->loc[i].secs - (6.0 * HOUR)) / (14.0 * HOUR) *
                    (MAX_WAVELEN - MIN_WAVELEN);
        }
        color = sdlx_wavelength_to_color(wvlen);

        sdlx_render_point(x, y, color, MAX_POINT_SIZE);

        num_points++;
    }
}

// --------------------------------------------------------

void nav_mode_display(void)
{
}

