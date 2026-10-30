#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include <sdlx.h>
#include <utils.h>
#include <profile.h>

#include "lib/lib.h"
#include "svcs/Location/location.h"

// xxx  priority
// - and display map name  <============
// - add CTR cmd  <===== ctr on track start
// - also display lat/long,  MAYBE
// - improve throw away outliers
// - option to mark location with text and possibly picture

// xxx
// - bearing to select point

// xxx 
// - yyy search
// - comments throughout

//
// defines
//

//#define PROFILE

#define ONE_SEC 1000000

#define K_COMPASS_SHOOTH 0.1

#define EVID_NEXT_DAY               1
#define EVID_PREV_DAY               2
#define EVID_MAP_LOCK_NORTH         3
#define EVID_START_SECS             4
#define EVID_END_SECS               5
#define EVID_PLAYBACK               6
#define EVID_PLAYBACK_CANCEL        7
#define EVID_PLAYBACK_PAUSE         8
#define EVID_PLAYBACK_CONTINUE      9
#define EVID_PLAYBACK_MINUTE_PLUS   10
#define EVID_PLAYBACK_MINUTE_MINUS  11

// the map width and height must be the same to support map rotation
#define MAP_WH     1500            // map width and height
#define MAP_XY_CTR (MAP_WH / 2)    // map x ctr and y ctr
#define MAP_XL     (sdlx_win_width / 2 - MAP_WH / 2)
#define MAP_XR     (sdlx_win_width / 2 + MAP_WH / 2)
#define MAP_YT     0
#define MAP_YB     MAP_WH

#define WVLEN_RED     650
#define WVLEN_PURPLE  380

#define DFLT_TRACK_START_SECS  (7 * 3600)    // 7 am
#define DFLT_TRACK_END_SECS    (18 * 3600)   // 6 pm
#define DFLT_MAP_HEIGHT_MILES  7.5
#define DFLT_MAP_LOCK_NORTH    true

#define MAP_BORDER 5

#define STATE_MONITOR          0
#define STATE_PLAYBACK         1
#define STATE_PLAYBACK_PAUSED  2

#define MILES_PER_DEGREE_LATITUDE        69.0
#define MILES_PER_DEGREE_LONGITUDE(lat)  (cosd(lat) * 69.0)
#define METERS_TO_MILES                  0.00062137
#define METERS_PER_SEC_TO_MILES_PER_HOUR 2.23694

#define BOSTON_MASS_LATITUDE  42.3601
#define BOSTON_MASS_LONGITUDE -71.0589
    
//
// typedefs
//

#define PARAMS_MAGIC 0x12345678
typedef struct {
    long   magic;
    double map_ctr_lat;
    double map_ctr_lon;
    int    track_start_secs;
    int    track_end_secs;
    double map_height_miles;
    bool   map_lock_north;
} params_t;

//
// variables
//

char         *progname;
char         *data_dir;

bool         end_program = false;
loc_hist2_t *lh2;

double       mag_decl;
bool         mag_decl_valid;

params_t    *params;

//
// prototypes
//

//void get_params(void);

void display(void);
void get_latest_lat_lon_for_day(int day, double *lat, double *lon);
void get_magnetic_declination(void);
void display_loc_points(struct loc_hist2_day_s *lh2d, int end_secs, int *last_loc_secs, 
                        double *distance_meters);
void display_map_mark(double lat, double lon, double map_rotate_angle);

void display_compass_init(void);
void display_compass_cleanup(void);
void display_compass(double true_heading);

char *heading_abbreviation(double heading);
void print_time_since_last_call(void);
char *secs_to_str(int secs);
char *double_to_str(double val);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    // --------
    // INIT
    // --------

    // save args
    if (argc != 2) {
        printf("E %s: argc=%d is not 2\n", "Navigator", argc);
        return 1;
    }
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // map location history file
    lh2 = util_map_file("svcs/Location", LOC_HIST2_FILENAME, sizeof(loc_hist2_t), false, NULL);
    if (lh2 == NULL) {
        printf("E: %s failed to map %s\n", progname, LOC_HIST2_FILENAME);
        return 1;
    }

    // init compass
    display_compass_init();

    // --------
    // RUNTIME
    // --------

#ifdef PROFILE
    profile_start();
#endif
    while (!end_program) {
        display();
    }
#ifdef PROFILE
    profile_stop(1);
#endif

    // --------
    // CLEANUP
    // --------

    // cleanup and end program
    util_unmap_file(lh2, sizeof(loc_hist2_t));
    display_compass_cleanup();
    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  DISPLAY HANDLER  --------------------

void display(void)
{
    sdlx_texture_t         *map_texture;;
    sdlx_event_t            event;
    sdlx_loc_t              dest, *loc;
    double                  mag_heading, true_heading, map_heading;
    double                  map_rotate_angle;
    double                  distance_meters;
    struct loc_hist2_day_s *lh2d;
    char                    str[100];
    int                     day, last_loc_secs;
    double                  width_miles;
    int                     end_secs;
    int                     state;
    double                  lat, lon;

    // init variables
    day = lh2->last_day;
    state = STATE_MONITOR;

    // create map_texture
    map_texture = sdlx_create_texture(MAP_WH,MAP_WH);

    while (!end_program) {
        //print_time_since_last_call();

        //
        // get params for the currently selected day;
        // init the params if needed
        //

        lh2d = &lh2->day[day % MAX_LH2_DAY];
        void *tmp = lh2->day[day % MAX_LH2_DAY].reserved1;
        params = tmp;
        if (params->magic != PARAMS_MAGIC) {
            printf("I %s: initializing params for %s %d %d\n",
                   progname, get_month_str(lh2d->month), lh2d->day, lh2d->year);
            get_latest_lat_lon_for_day(day, &lat, &lon);
            if (lat == INVALID_NUMBER || lon == INVALID_NUMBER) {
                lat = BOSTON_MASS_LATITUDE;
                lon = BOSTON_MASS_LONGITUDE;
            }
            params->magic            = PARAMS_MAGIC;
            params->map_ctr_lat      = lat;
            params->map_ctr_lon      = lon;
            params->track_start_secs = DFLT_TRACK_START_SECS;
            params->track_end_secs   = DFLT_TRACK_END_SECS;
            params->map_height_miles = DFLT_MAP_HEIGHT_MILES;
            params->map_lock_north   = DFLT_MAP_LOCK_NORTH;
        }

        //
        // get magnetic declination
        //

        get_magnetic_declination();

        //
        // draw location points to the map_texture
        //

        sdlx_set_render_target(map_texture);
        sdlx_clear_texture(map_texture, COLOR_BLACK);

        if (state == STATE_MONITOR) {
            end_secs = params->track_end_secs;
        } else if (state == STATE_PLAYBACK) {
            if (end_secs + 60 >= params->track_end_secs) {
                state = STATE_MONITOR;  
                end_secs = params->track_end_secs;
            } else {
                end_secs += 60;
            }
        } else if (state == STATE_PLAYBACK_PAUSED) {
            // do nothing
        }

        display_loc_points(lh2d, end_secs, &last_loc_secs, &distance_meters);
        sdlx_set_render_target(NULL);

        //
        // init the backbuffer to COLOR_BLACK
        //

        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        //
        // get true_heading, from mag_heading and declination
        //

        sdlx_sensor_read_mag_heading(&mag_heading, K_COMPASS_SHOOTH);
        if (mag_heading == INVALID_NUMBER) {
            true_heading = 0;
        } else {
            true_heading = mag_heading + mag_decl;
            if (true_heading < 0) true_heading += 360;
            if (true_heading >= 360) true_heading -= 360;
        }

        //
        // display the map_texture, which contains the location points
        //

        // display the texture containing the loc points;
        // rotated by 0 when map is locked north, else rotated by true_heading
        dest.x = MAP_XL;
        dest.y = MAP_YT;
        dest.w = MAP_WH;
        dest.h = MAP_WH;
        map_rotate_angle = (params->map_lock_north ? 0 : true_heading);
        sdlx_render_texture_rotated(map_texture, NULL, &dest, -map_rotate_angle, NULL, FLIP_NONE);

        // the rotated texture will spill over the display area below the map,
        // so clear the display area below the map
        sdlx_render_fill_rect(0, MAP_YB+1, sdlx_win_width, sdlx_win_height-MAP_YB+1, COLOR_BLACK);

        //
        // display map annotations
        //

        // display compass abbreviatios at the top, right, bottom, and left of the map
        if (params->map_lock_north) {
            sprintf(str, "NORTH LOCK");
            map_heading = 0;
        } else {
            sprintf(str, "%s - %d", heading_abbreviation(true_heading), (int)true_heading);
            map_heading = true_heading;
        }
        loc = sdlx_render_printf_ex(sdlx_win_width/2, MAP_BORDER,
                                    FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_X_CTR,
                                     "%s", str);
        sdlx_register_event(loc, EVID_MAP_LOCK_NORTH);

        sprintf(str, "%s", heading_abbreviation(map_heading+90));
        sdlx_render_printf_ex(sdlx_win_width-MAP_BORDER-COL2X(strlen(str)), MAP_YB/2, 
                              FONT_NORMAL, COLOR_WHITE, FLAG_Y_CTR, 
                              "%s", str);

        sprintf(str, "%s", heading_abbreviation(map_heading+180));
        sdlx_render_printf_ex(sdlx_win_width/2, MAP_YB-ROW2Y(1), 
                              FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
                              "%s", str);

        sprintf(str, "%s", heading_abbreviation(map_heading+270));
        sdlx_render_printf_ex(MAP_BORDER, MAP_YB/2, 
                              FONT_NORMAL, COLOR_WHITE, FLAG_Y_CTR, 
                              "%s", str);

        // display map width scale 
        width_miles = params->map_height_miles * ((double)sdlx_win_width / MAP_WH);
        sdlx_render_printf_ex(sdlx_win_width/2, ROW2Y(1),
                              FONT_SMALL, COLOR_WHITE, FLAG_X_CTR,
                              "map width %s miles", double_to_str(width_miles));

        // display compass on the map
        display_compass(true_heading);

        // display mark on map at current location
        get_latest_lat_lon_for_day(day, &lat, &lon);
        if (lat != INVALID_NUMBER && lon != INVALID_NUMBER) {
            display_map_mark(lat, lon, map_rotate_angle);
        }

        // draw border around the map display area
        sdlx_render_rect(0, 0, sdlx_win_width, MAP_WH, MAP_BORDER, COLOR_GREEN);

        //
        // display the control & status area
        //

        // 0: date and time of the last loc
        sdlx_render_printf(0, MAP_YB+ROW2Y(0),
                           "%s %s %d  %s", 
                           get_weekday_str(lh2d->year, lh2d->month, lh2d->day),
                           get_month_str(lh2d->month),
                           lh2d->day,
                           (state != STATE_MONITOR ? secs_to_str(end_secs) : ""));

        // 1: distance and speed
        sdlx_render_printf(0, MAP_YB+ROW2Y(1), 
                           "%s mi. %s mph", 
                           double_to_str(distance_meters * METERS_TO_MILES),
                           double_to_str(distance_meters / (end_secs - params->track_start_secs) * METERS_PER_SEC_TO_MILES_PER_HOUR));

        // 2: display time of track start and end, and register event to modify them
        loc = sdlx_render_printf_ex(0, MAP_YB+ROW2Y(2), 
                                    FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                    "%s",
                                    secs_to_str(params->track_start_secs));
        sdlx_register_event(loc, EVID_START_SECS);

        sdlx_render_printf(COL2X(5), MAP_YB+ROW2Y(2), "-");

        loc = sdlx_render_printf_ex(COL2X(6), MAP_YB+ROW2Y(2), 
                                    FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                    "%s",
                                    secs_to_str(params->track_end_secs));
        sdlx_register_event(loc, EVID_END_SECS);

        if (state == STATE_MONITOR) {
            loc = sdlx_render_printf_ex(COL2X(15), MAP_YB+ROW2Y(2), 
                                        FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                        "PLAY");
            sdlx_register_event(loc, EVID_PLAYBACK);
        }

        // 3: playback control
       if (state == STATE_PLAYBACK) {
            // ctrls: CANCEL, PAUSE
            loc = sdlx_render_printf_ex(COL2X(0), MAP_YB+ROW2Y(3), 
                                        FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                        "CANCEL");
            sdlx_register_event(loc, EVID_PLAYBACK_CANCEL);

            loc = sdlx_render_printf_ex(COL2X(8), MAP_YB+ROW2Y(3), 
                                        FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                        "PAUSE");
            sdlx_register_event(loc, EVID_PLAYBACK_PAUSE);
        } else if (state == STATE_PLAYBACK_PAUSED) {
            // ctrls:  CANCEL, CONT, -,  +
            loc = sdlx_render_printf_ex(COL2X(0), MAP_YB+ROW2Y(3), 
                                        FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                        "CANCEL");
            sdlx_register_event(loc, EVID_PLAYBACK_CANCEL);

            loc = sdlx_render_printf_ex(COL2X(8), MAP_YB+ROW2Y(3), 
                                        FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                        "CONT");
            sdlx_register_event(loc, EVID_PLAYBACK_CONTINUE);

            loc = sdlx_render_printf_ex(COL2X(13), MAP_YB+ROW2Y(3), 
                                        FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                        " - ");
            sdlx_register_event(loc, EVID_PLAYBACK_MINUTE_MINUS);

            loc = sdlx_render_printf_ex(COL2X(17), MAP_YB+ROW2Y(3), 
                                        FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                        " + ");
            sdlx_register_event(loc, EVID_PLAYBACK_MINUTE_PLUS);
        }

        //
        // register additional events
        //

        sdlx_register_event(NULL, EVID_PINCH);
        sdlx_register_event(NULL, EVID_MOTION);
        reg_event_show_readme_file();
        sdlx_register_control_events(EVID_PREV_DAY, "<", EVID_NEXT_DAY, ">", EVID_QUIT, "X");

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
        if (event.event_id == -1) {
            continue;
        }

        //
        // process events
        //

        switch (event.event_id) {
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_PREV_DAY: {
            int new_day = day - 1;

            state = STATE_MONITOR;
            if ((new_day) >= 0 && (lh2->last_day - new_day < MAX_LH2_DAY)) {
                if (lh2->day[new_day % MAX_LH2_DAY].max_loc > 0) {
                    day = new_day;
                }
            }
            break; }
        case EVID_NEXT_DAY:
            state = STATE_MONITOR;
            if (day < lh2->last_day) {
                day++;
            }
            break;
        case EVID_MOTION: {
            double xrel = event.u.motion.xrel * cosd(map_rotate_angle) - 
                          event.u.motion.yrel * sind(map_rotate_angle);
            double yrel = event.u.motion.xrel * sind(map_rotate_angle) + 
                          event.u.motion.yrel * cosd(map_rotate_angle);
            params->map_ctr_lat  += yrel * 
                                    (params->map_height_miles / MAP_WH) / 
                                    MILES_PER_DEGREE_LATITUDE;
            params->map_ctr_lon -= xrel * 
                                   (params->map_height_miles / MAP_WH) / 
                                   MILES_PER_DEGREE_LONGITUDE(params->map_ctr_lat);
            break; }
        case EVID_PINCH:
            if (event.u.pinch.scale == 0) break;
            params->map_height_miles /= event.u.pinch.scale;
            if (params->map_height_miles < 0.0015) params->map_height_miles = 0.0015;
            if (params->map_height_miles > 4500) params->map_height_miles = 4500;
            break;
        case EVID_MAP_LOCK_NORTH:
            params->map_lock_north = !params->map_lock_north;
            break;
        case EVID_PLAYBACK:
            state = STATE_PLAYBACK;
            end_secs = params->track_start_secs;
            break;
        case EVID_PLAYBACK_CANCEL:
            state = STATE_MONITOR;
            break;
        case EVID_PLAYBACK_PAUSE:
            state = STATE_PLAYBACK_PAUSED;
            break;
        case EVID_PLAYBACK_CONTINUE:
            state = STATE_PLAYBACK;
            break;
        case EVID_PLAYBACK_MINUTE_PLUS:
            end_secs += 60;
            if (end_secs >= params->track_end_secs) {
                end_secs = params->track_end_secs;
            }
            break;
        case EVID_PLAYBACK_MINUTE_MINUS:
            end_secs -= 60;
            if (end_secs < params->track_start_secs) {
                end_secs = params->track_start_secs;
            }
            break;
        case EVID_START_SECS: {
            char *s, *p, dflt[50];
            int cnt, hours, minutes;
            get_current_hms(&hours, &minutes, NULL);
            sprintf(dflt, "%02d:%02d", hours, minutes);
            s = sdlx_get_input_str("Start hh.mm", true, dflt);
            if ((p = strchr(s, '.'))) *p = ':';
            cnt = sscanf(s, "%d:%d", &hours, &minutes);
            if (cnt == 1 && hours >= 0 && hours < 24) {
                params->track_start_secs = hours*3600;
            } else if (cnt == 2 && hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60) {
                params->track_start_secs = hours*3600 + minutes*60;
            } 
            break; }
        case EVID_END_SECS: {
            char *s, *p, dflt[50];
            int cnt, hours, minutes;
            get_current_hms(&hours, &minutes, NULL);
            sprintf(dflt, "%02d:%02d", hours, minutes);
            s = sdlx_get_input_str("End hh.mm", true, dflt);
            if ((p = strchr(s, '.'))) *p = ':';
            cnt = sscanf(s, "%d:%d", &hours, &minutes);
            if (cnt == 1 && hours >= 0 && hours < 24) {
                params->track_end_secs = hours*3600;
            } else if (cnt == 2 && hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60) {
                params->track_end_secs = hours*3600 + minutes*60;
            } 
            break; }
        case EVID_QUIT:
            end_program = true;
            break;
        }
    }

    sdlx_destroy_texture(map_texture);
}

void get_latest_lat_lon_for_day(int day, double *lat, double *lon)
{
    struct loc_hist2_day_s *lh2d;

    lh2d = &lh2->day[day % MAX_LH2_DAY];

    if (lh2d->max_loc > 0) {
        *lat = lh2d->loc[lh2d->max_loc-1].latitude;
        *lon = lh2d->loc[lh2d->max_loc-1].longitude;
    } else {
        util_get_location(lat, lon, NULL, NULL);
    }
}

void get_magnetic_declination(void)
{
    long         time_now_usec;
    static long  mag_decl_last_query_time_usec;

    time_now_usec = util_microsec_timer();
    if ((mag_decl_last_query_time_usec == 0) || 
        ((time_now_usec - mag_decl_last_query_time_usec) / 1000000 > 600))
    {
        double new_mag_decl;
        printf("I %s: calling get_mag_decl\n", progname); //yyy temp print
        new_mag_decl = get_mag_decl();
        if (new_mag_decl != INVALID_NUMBER) {
            mag_decl = new_mag_decl;
            mag_decl_valid = true;
        } else {
            printf("E %s: failed to get mag_dec, mag_decl value remains %0.1f\n", 
                   progname, mag_decl);
            mag_decl_valid = false;  // yyy display warning under compass
        }
        mag_decl_last_query_time_usec = time_now_usec;
    }
}

void display_loc_points(struct loc_hist2_day_s *lh2d, int end_secs, int *last_loc_secs,
                        double *distance_meters) 
{
    int          i, x, y, n, tmp_secs, secs, wvlen;
    double       k_lat, k_lon, dist;
    sdlx_point_t points[MAX_LH2_LOC];
    sdlx_color_t colors[MAX_LH2_LOC];
    int          track_start_secs, track_end_secs, track_start_minute, track_end_minute, track_span_minute, track_span_wvlen;
    long         time_execution_usec;

    time_execution_usec = util_microsec_timer();

    track_start_secs   = params->track_start_secs;
    track_end_secs     = params->track_end_secs;
    track_start_minute = track_start_secs/60;
    track_end_minute   = track_end_secs/60;
    track_span_minute  = (track_end_minute - track_start_minute) + 1;
    track_span_wvlen   = WVLEN_PURPLE - WVLEN_RED;

    k_lat = MILES_PER_DEGREE_LATITUDE / params->map_height_miles * MAP_WH;
    k_lon = MILES_PER_DEGREE_LONGITUDE(params->map_ctr_lat) / params->map_height_miles * MAP_WH;

    n = 0;
    secs = 0;
    dist = 0;

    for (i = 0; i < lh2d->max_loc; i++) {
        tmp_secs = lh2d->loc[i].secs;
        if (tmp_secs < track_start_secs) continue;
        if (tmp_secs >= end_secs) break;

        secs = tmp_secs;
        dist += lh2d->loc[i].distance;

        x = MAP_XY_CTR + (lh2d->loc[i].longitude - params->map_ctr_lon) * k_lon;
        if (x < 0 || x > MAP_WH) {
            continue;
        }

        y = MAP_XY_CTR - (lh2d->loc[i].latitude - params->map_ctr_lat) * k_lat;
        if (y < 0 || y > MAP_WH) {
            continue;
        }

        points[n].x = x;
        points[n].y = y;
        wvlen = WVLEN_RED + ((secs/60.0 - track_start_minute) / track_span_minute) * track_span_wvlen;
        colors[n] = sdlx_wavelength_to_color(wvlen);

        n++;
        if (n == MAX_LH2_LOC) break;
    }

    *last_loc_secs   = secs;
    *distance_meters = dist;

    sdlx_render_points(points, n, colors, 0, NULL, MAX_POINT_SIZE);

    // print the duration of this routine
    //printf("duration = %ld ms\n", (util_microsec_timer() - time_execution_usec) / 1000);
}

void display_map_mark(double lat, double lon, double map_rotate_angle)
{
    double k_lat, k_lon, x, y, x1, y1;
    int length = 50;

    k_lat = MILES_PER_DEGREE_LATITUDE / params->map_height_miles * MAP_WH;
    k_lon = MILES_PER_DEGREE_LONGITUDE(params->map_ctr_lat) / params->map_height_miles * MAP_WH;

    x = MAP_XY_CTR + (lon - params->map_ctr_lon) * k_lon;
    if (x < 0 || x > MAP_WH) return;

    y = MAP_XY_CTR - (lat - params->map_ctr_lat) * k_lat;
    if (y < 0 || y > MAP_WH) return;

    x -= 750;
    y -= 750;
    x1 = x * cosd(-map_rotate_angle) - y * sind(-map_rotate_angle);
    y1 = x * sind(-map_rotate_angle) + y * cosd(-map_rotate_angle);
    x = x1 + 750;
    y = y1 + 750;

    x -= 250;

    sdlx_render_line(x-length, y-2, x+length, y-2, COLOR_WHITE);
    sdlx_render_line(x-length, y-1, x+length, y-1, COLOR_WHITE);
    sdlx_render_line(x-length, y, x+length, y, COLOR_WHITE);
    sdlx_render_line(x-length, y+1, x+length, y+1, COLOR_WHITE);
    sdlx_render_line(x-length, y+2, x+length, y+2, COLOR_WHITE);

    sdlx_render_line(x-2, y-length, x-2, y+length, COLOR_WHITE);
    sdlx_render_line(x-1, y-length, x-1, y+length, COLOR_WHITE);
    sdlx_render_line(x, y-length, x, y+length, COLOR_WHITE);
    sdlx_render_line(x+1, y-length, x+1, y+length, COLOR_WHITE);
    sdlx_render_line(x+2, y-length, x+2, y+length, COLOR_WHITE);
}

// -----------------  DISPLAY COMPASS  ---------------------------------

#define COMPASS_RADIUS 75
#define COMPASS_XL     800
#define COMPASS_YT     200

sdlx_texture_t *compass_texture;

void render_triangle(int x_top, int y_top, int width, int height, sdlx_color_t color)
{
    int i, y, x1, x2;
    double w;

    y = y_top;
    for (i = 0; i < height; i++) {
        w = width * ((double)i / height) + 1;
        x1 = nearbyint(x_top - w / 2);
        x2 = nearbyint(x_top + w / 2);
        sdlx_render_line(x1, y, x2, y, color);
        y++;
    }
}

void display_compass_init(void)
{
    compass_texture = sdlx_create_texture(2*COMPASS_RADIUS, 2*COMPASS_RADIUS);
    if (compass_texture == NULL) {
        return;
    }

    sdlx_set_render_target(compass_texture);
    sdlx_render_fill_circle(COMPASS_RADIUS, COMPASS_RADIUS, COMPASS_RADIUS, COLOR_WHITE);
    render_triangle(COMPASS_RADIUS, 0, COMPASS_RADIUS*0.50, COMPASS_RADIUS*0.85, COLOR_RED);
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

char *secs_to_str(int secs)
{
    int hour = secs/3600;
    int minute = (secs - 3600*hour) / 60;
    char *s;

    static char strs[4][50];
    static int idx;

    idx = (idx + 1) % 4;
    s = strs[idx];

    sprintf(s, "%02d:%02d", hour, minute);
    return s;
}

char *double_to_str(double val)
{
    int precision;
    char *s;

    static char strs[4][20];
    static int idx;

    precision = (val <  1 ? 3 :
                (val <  5 ? 2 :
                (val < 10 ? 1 :
                            0)));

    s = strs[idx];
    idx = (idx + 1) % 4;

    return util_double_to_str(val, 0, precision, s, 20);
}
