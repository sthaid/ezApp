#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include <sdlx.h>
#include <utils.h>
#include <profile.h>

#include "lib/lib.h"
#include "svcs/Location/location.h"

// xxx 
// - also yyy
// - go back to 30 sec interval
// - throw away outliers

//
// defines
//

//#define PROFILE

#define ONE_SEC 1000000

#define K_COMPASS_SHOOTH 0.1

#define EVID_NEXT           1
#define EVID_PREV           2
#define EVID_MAP_LOCK_NORTH 3

#define MILES_PER_DEGREE_LATITUDE       69.0
#define MILES_PER_DEGREE_LONGITUDE(lat) (cosd(lat) * 69.0)

#define METERS_TO_MILES 0.00062137  // yyy perhaps the distance should be saved in miles

// the map width and height must be the same to support map rotation
#define MAP_WH     1500            // map width and height
#define MAP_XY_CTR (MAP_WH / 2)    // map x ctr and y ctr
#define MAP_XL     (sdlx_win_width / 2 - MAP_WH / 2)
#define MAP_XR     (sdlx_win_width / 2 + MAP_WH / 2)
#define MAP_YT     0
#define MAP_YB     MAP_WH

#define WVLEN_RED 650
#define WVLEN_PURPLE  380

#define DFLT_TRACK_START_SECS  (7 * 3600)
#define DFLT_TRACK_END_SECS    (18 * 3600)
#define DFLT_MAP_WH_MILES      4.5
#define DFLT_MAP_LOCK_NORTH    true
    
//
// variables
//

char         *progname;
char         *data_dir;

bool         end_program = false;
double       mag_decl;
loc_hist2_t *lh2;

double       map_ctr_lat;
double       map_ctr_long;

// xxx use params for these
int          track_start_secs = DFLT_TRACK_START_SECS;
int          track_end_secs   = DFLT_TRACK_END_SECS;
double       map_wh_miles     = DFLT_MAP_WH_MILES;
bool         map_lock_north   = DFLT_MAP_LOCK_NORTH;

//
// prototypes
//

void display(void);
void display_loc_points(struct loc_hist2_day_s *lh2d);

void display_compass_init(void);
void display_compass_cleanup(void);
void display_compass(double true_heading);

char *heading_abbreviation(double heading);
void print_time_since_last_call(void);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    //
    // INIT
    //

    // save args
    if (argc != 2) {
        printf("E %s: argc=%d is not 2\n", "Navigator", argc);
        return 1;
    }
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // get mag decl
    // xxx this will change on a long trip, perhaps get it again every so often
    mag_decl = get_mag_decl();
    if (mag_decl == INVALID_NUMBER) {
        printf("E %s: failed to get mag_decl, terminating\n", progname);
        return 1;
    }

    // map location history file v2
    // - create_if_needed = false
    // - created (return flag) = NULL
    lh2 = util_map_file("svcs/Location", LOC_HIST2_FILENAME, sizeof(loc_hist2_t), false, NULL);
    if (lh2 == NULL) {
        printf("E: %s failed to map %s\n", progname, LOC_HIST2_FILENAME);
        return 1;
    }

    // init compass
    display_compass_init();

    //
    // RUNTIME
    //

#ifdef PROFILE
    profile_start();
#endif
    while (!end_program) {
        display();
    }
#ifdef PROFILE
    profile_stop(1);
#endif

    //
    // CLEANUP
    //

    // cleanup and end program
    util_unmap_file(lh2, sizeof(loc_hist2_t));
    display_compass_cleanup();
    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  DISPLAY HANDLER  --------------------

void map_init(void)
{
    util_get_location(&map_ctr_lat, &map_ctr_long, NULL, NULL);

    track_start_secs = DFLT_TRACK_START_SECS;
    track_end_secs   = DFLT_TRACK_END_SECS;
    map_wh_miles     = DFLT_MAP_WH_MILES;
    map_lock_north   = DFLT_MAP_LOCK_NORTH;
}

void display(void)
{
    sdlx_texture_t         *map_texture;;
    sdlx_event_t            event;
    sdlx_loc_t              dest, *loc;
    double                  mag_heading, true_heading, map_heading;
    double                  map_rotate_angle;
    double                  map_width_miles;
    struct loc_hist2_day_s *lh2d;
    char                    str[100];
    int                     day;

    // init
    day = lh2->last_day;
    map_texture = sdlx_create_texture(MAP_WH,MAP_WH);
    map_init();

    while (!end_program) {
        // print time since last update yyy try this later
        // print_time_since_last_call();

        // set ptr to loc_hist2 data for 'day'
        lh2d = &lh2->day[day % MAX_LH2_DAY];

        // draw location points to the map_texture
        sdlx_set_render_target(map_texture);
        sdlx_clear_texture(map_texture, COLOR_BLACK);
        display_loc_points(lh2d);
        sdlx_set_render_target(NULL);

        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // get true_heading, from mag_heading and declination
        sdlx_sensor_read_mag_heading(&mag_heading, K_COMPASS_SHOOTH);
        if (mag_heading == INVALID_NUMBER) {
            true_heading = 0;
        } else {
            true_heading = mag_heading + mag_decl;
            if (true_heading < 0) true_heading += 360;
            if (true_heading >= 360) true_heading -= 360;
        }

        // display the texture containing the loc points;
        // rotated by 0 when map is locked north, else rotated by true_heading
        dest.x = MAP_XL;
        dest.y = MAP_YT;
        dest.w = MAP_WH;
        dest.h = MAP_WH;
        map_rotate_angle = (map_lock_north ? 0 : true_heading);
        sdlx_render_texture_rotated(map_texture, NULL, &dest, -map_rotate_angle, NULL, FLIP_NONE);

        // the rotated texture will spill over the display area below the map,
        // so clear the display area below the map
        sdlx_render_fill_rect(0, MAP_YB+1, sdlx_win_width, sdlx_win_height-MAP_YB+1, COLOR_BLACK);

        //
        // display map annotations
        //

        // - display compass abbreviatios at the top, right, bottom, and left of the map
        if (map_lock_north) {
            sprintf(str, "NORTH LOCK");
            map_heading = 0;
        } else {
            sprintf(str, "%s - %d", heading_abbreviation(true_heading), (int)true_heading);
            map_heading = true_heading;
        }
        loc = sdlx_render_printf_ex(sdlx_win_width/2, 5,   // yyy define for 5
                                    FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_X_CTR,
                                     "%s", str);
        sdlx_register_event(loc, EVID_MAP_LOCK_NORTH);

        sprintf(str, "%s", heading_abbreviation(map_heading+90));
        sdlx_render_printf_ex(sdlx_win_width-5-COL2X(strlen(str)), MAP_YB/2, 
                              FONT_NORMAL, COLOR_WHITE, FLAG_Y_CTR, 
                              "%s", str);

        sprintf(str, "%s", heading_abbreviation(map_heading+180));
        sdlx_render_printf_ex(sdlx_win_width/2, MAP_YB-ROW2Y(1), 
                              FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
                              "%s", str);

        sprintf(str, "%s", heading_abbreviation(map_heading+270));
        sdlx_render_printf_ex(5, MAP_YB/2, 
                              FONT_NORMAL, COLOR_WHITE, FLAG_Y_CTR, 
                              "%s", str);

        // - display map width scale 
        map_width_miles = map_wh_miles * ((double)sdlx_win_width / MAP_WH);
        if (map_width_miles < 0.1) {
            sdlx_render_printf_ex(sdlx_win_width/2, ROW2Y(1),
                                FONT_SMALL, COLOR_WHITE, FLAG_X_CTR,
                                "map width %.3f miles", map_width_miles);
        } else if (map_width_miles < 1) {
            sdlx_render_printf_ex(sdlx_win_width/2, ROW2Y(1),
                                FONT_SMALL, COLOR_WHITE, FLAG_X_CTR,
                                "map width %.2f miles", map_width_miles);
        } else if (map_width_miles < 10) {
            sdlx_render_printf_ex(sdlx_win_width/2, ROW2Y(1),
                                FONT_SMALL, COLOR_WHITE, FLAG_X_CTR,
                                "map width %.1f miles", map_width_miles);
        } else {
            sdlx_render_printf_ex(sdlx_win_width/2, ROW2Y(1),
                                FONT_SMALL, COLOR_WHITE, FLAG_X_CTR,
                                "map width %.0f miles", map_width_miles);
        }

        // - display compass on the map
        display_compass(true_heading);

        // - draw border around the map display area
        sdlx_render_rect(0, 0, sdlx_win_width, MAP_WH, 5, COLOR_GREEN);

        //
        // display the control & status area
        // example:  yyy check these
        //   Fri Oct 16
        //   14 miles
        //

        // - map date
        sdlx_render_printf(0, MAP_YB,
                        "%s %s %d", 
                        get_weekday_str(lh2d->year, lh2d->month, lh2d->day),
                        get_month_str(lh2d->month),
                        lh2d->day);

        // - distance  yyy compensate for start time
        double total_distance = (lh2d->max_loc == 0 
                                 ? 0
                                 : lh2d->loc[lh2d->max_loc-1].total_distance * METERS_TO_MILES);
        if (total_distance < 1) {
            sdlx_render_printf(0, MAP_YB + ROW2Y(1), "%.3f miles", total_distance);
        } else if (total_distance < 10) {
            sdlx_render_printf(0, MAP_YB + ROW2Y(1), "%.1f miles", total_distance);
        } else {
            sdlx_render_printf(0, MAP_YB + ROW2Y(1), "%.0f miles", total_distance);
        }
        
        //
        // register additional events
        //

        sdlx_register_event(NULL, EVID_PINCH);
        sdlx_register_event(NULL, EVID_MOTION);
        reg_event_show_readme_file();
        sdlx_register_control_events(EVID_PREV, "<", EVID_NEXT, ">", EVID_QUIT, "X");

        //
        // present the display
        //

        sdlx_display_present();

        //
        // wait for event, with 50 ms timeout; 
        // the short timeout is needed because the displayed map depends on device heading,
        // and because the compass needs to be periodically updated
        //

        sdlx_get_event(50000, &event);

        //
        // process events
        //

        switch (event.event_id) {
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_PREV: {
            int new_day = day - 1;
            struct loc_hist2_day_s *temp_lh2d;

            if ((new_day) >= 0 && (lh2->last_day - new_day < MAX_LH2_DAY)) {  //yyy verify this works
                temp_lh2d = &lh2->day[new_day % MAX_LH2_DAY];
                if (temp_lh2d->max_loc > 0) {
                    day = new_day;
                    map_init();
                }
            }
            break; }
        case EVID_NEXT:
            if (day < lh2->last_day) {
                day++;
                map_init();
            }
            break;
        case EVID_MOTION: {
            double xrel = event.u.motion.xrel * cosd(map_rotate_angle) - 
                          event.u.motion.yrel * sind(map_rotate_angle);
            double yrel = event.u.motion.xrel * sind(map_rotate_angle) + 
                          event.u.motion.yrel * cosd(map_rotate_angle);
            map_ctr_lat  += yrel * 
                            (map_wh_miles / MAP_WH) / 
                            MILES_PER_DEGREE_LATITUDE;
            map_ctr_long -= xrel * 
                            (map_wh_miles / MAP_WH) / 
                            MILES_PER_DEGREE_LONGITUDE(map_ctr_lat);
            break; }
        case EVID_PINCH:
            if (event.u.pinch.scale == 0) break;
            map_wh_miles /= event.u.pinch.scale;
            if (map_wh_miles < 0.0015) map_wh_miles = 0.0015;   // yyy test the range
            if (map_wh_miles > 4500) map_wh_miles = 4500;
            break;
        case EVID_MAP_LOCK_NORTH:
            map_lock_north = !map_lock_north;
            break;
        case EVID_QUIT:
            end_program = true;
            break;
        }
    }

    sdlx_destroy_texture(map_texture);
}

void display_loc_points(struct loc_hist2_day_s *lh2d)
{
    int          i, x, y, n;
    double       k_lat, k_lng, wvlen;
    sdlx_point_t points[10000];  // xxx use define from location.h
    sdlx_color_t colors[10000];

    static int color_tbl_start_secs;
    static int color_tbl_end_secs;
    static sdlx_color_t color_tbl[1441];  // indexed by minutes

    if (color_tbl_start_secs != track_start_secs || color_tbl_end_secs != track_end_secs) {
        long   start_usecs = util_microsec_timer();
        int    start_minute, end_minute, minute;
        double span_minute;
        int    span_wvlen;

        memset(color_tbl, 0xff, sizeof(color_tbl));

        start_minute = track_start_secs/60;
        end_minute   = track_end_secs/60;
        span_minute  = end_minute - start_minute + 1;
        span_wvlen   = WVLEN_PURPLE - WVLEN_RED;

        for (minute = start_minute; minute <= end_minute; minute++) {
            wvlen = WVLEN_RED + 
                    ((minute - start_minute) / span_minute) * 
                    span_wvlen;
            color_tbl[minute] = sdlx_wavelength_to_color(wvlen);
        }

        color_tbl_start_secs = track_start_secs;
        color_tbl_end_secs   = track_end_secs;

        printf("I %s: init color tbl done, duration=%ld usecs\n", 
               progname, util_microsec_timer()-start_usecs);
    }

    k_lat = MILES_PER_DEGREE_LATITUDE / map_wh_miles * MAP_WH;
    k_lng = MILES_PER_DEGREE_LONGITUDE(map_ctr_lat) / map_wh_miles * MAP_WH;

    n = 0;
    for (i = 0; i < lh2d->max_loc; i++) {
        int secs = lh2d->loc[i].secs;

        if (secs < track_start_secs) {
            continue;
        }
        if (secs >= track_end_secs) {
            break;
        }

        x = MAP_XY_CTR + (lh2d->loc[i].longitude - map_ctr_long) * k_lng;
        if (x < 0 || x > MAP_WH) continue;

        y = MAP_XY_CTR - (lh2d->loc[i].latitude - map_ctr_lat) * k_lat;
        if (y < 0 || y > MAP_WH) continue;

        points[n].x = x;
        points[n].y = y;
        colors[n] = color_tbl[secs/60];

        n++;
    }

    sdlx_render_points(points, n, colors, 0, NULL, MAX_POINT_SIZE);
}

// -----------------  DISPLAY COMPASS  ---------------------------------

#define COMPASS_RADIUS 75
#define COMPASS_XL     800
#define COMPASS_YT     200

sdlx_texture_t *compass_texture;

void display_compass_init(void)
{
    compass_texture = sdlx_create_texture(2*COMPASS_RADIUS, 2*COMPASS_RADIUS);
    if (compass_texture == NULL) {
        return;
    }

    sdlx_set_render_target(compass_texture);
    sdlx_render_fill_circle(COMPASS_RADIUS, COMPASS_RADIUS, COMPASS_RADIUS, COLOR_WHITE);
    sdlx_render_point(COMPASS_RADIUS, 10, COLOR_RED, MAX_POINT_SIZE);  // xxx use triangle
    sdlx_set_render_target(NULL);
}

void display_compass_cleanup(void)
{
    if (compass_texture != NULL) {
        sdlx_destroy_texture(compass_texture);
        compass_texture = NULL;
    }
}

void display_compass(double true_heading)
{
    sdlx_loc_t dest;

    if (compass_texture == NULL) {
        return;
    }

    dest.x = COMPASS_XL;
    dest.y = COMPASS_YT;
    dest.w = 2*COMPASS_RADIUS;
    dest.h = 2*COMPASS_RADIUS;

    sdlx_render_texture_rotated(compass_texture, 
                                NULL,           // copy entire src textrue
                                &dest,          // dest rect
                                -true_heading,  // rotation angle
                                NULL,           // rotate about dest rect center
                                FLIP_NONE);
}

// -----------------  UTILS  -------------------------------------------

char *heading_abbreviation(double heading)
{
    while (heading >= 360) heading -= 360;
    while (heading < 0) heading += 360;

    if (heading >= 337.5 || heading < 22.5) {
        return "N";
    } else if (heading >= 22.5 && heading < 67.5) {
        return "NE";
    } else if (heading >= 67.5 && heading < 112.5) {
        return "E";
    } else if (heading >= 112.5 && heading < 157.5) {
        return "SE";
    } else if (heading >= 157.5 && heading < 202.5) {
        return "S";
    } else if (heading >= 202.5 && heading < 247.5) {
        return "SW";
    } else if (heading >= 247.5 && heading < 292.5) {
        return "W";
    } else if (heading >= 292.5 && heading < 337.5) {
        return "NW";
    } else {
        return "Invalid";
    }
}

void print_time_since_last_call(void)
{
    static long tlast;
    long tnow;

    tnow = util_microsec_timer();

    if (tlast == 0) {
        tlast = tnow;
        return;
    }

    printf("I %s: time since last call = %ld ms\n",
           progname, (tnow - tlast) / 1000);

    tlast = tnow;
}
