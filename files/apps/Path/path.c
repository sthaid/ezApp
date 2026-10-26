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

// xxx make loc_hist2 secs an int

// xxx on devel pc, when zoom in the current loc moves

//
// defines
//

//#define PROFILE

#define ONE_SEC 1000000

#define K_COMPASS_SHOOTH 0.1

#define EVID_NEXT_DAY       1
#define EVID_PREV_DAY       2
#define EVID_MAP_LOCK_NORTH 3
#define EVID_PLAY           4
#define EVID_START_SECS     5
#define EVID_END_SECS       6

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

#define WVLEN_RED     650
#define WVLEN_PURPLE  380

#define DFLT_TRACK_START_SECS  (7 * 3600)
#define DFLT_TRACK_END_SECS    (18 * 3600)
#define DFLT_MAP_HEIGHT_MILES  7.5
#define DFLT_MAP_LOCK_NORTH    true
    
#define STATE_MONITOR   0
#define STATE_PLAY      1

//
// variables
//

char         *progname;
char         *data_dir;

bool         end_program = false;
double       mag_decl;
loc_hist2_t *lh2;

double       map_ctr_lat;       // params ...
double       map_ctr_lon;
int          track_start_secs;
int          track_end_secs;
double       map_height_miles;
bool         map_lock_north;

//
// prototypes
//

void get_params(void);

void display(void);
int display_loc_points(struct loc_hist2_day_s *lh2d, int start_secs, int end_secs, int *last_map_loc_secs,
                       double *distance_meters);
void display_map_mark(double lat, double lon, double map_rotate_angle);

void display_compass_init(void);
void display_compass_cleanup(void);
void display_compass(double true_heading);

char *heading_abbreviation(double heading);
void print_time_since_last_call(void);
char *secs_to_str(int secs);
char *state_str(int state);

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

    // get parasm
    get_params();

    // get mag decl yyy this will change on a long trip, perhaps get it again every so often
    mag_decl = get_mag_decl();
    if (mag_decl == INVALID_NUMBER) {
        printf("E %s: failed to get mag_decl, terminating\n", progname);
        return 1;
    }

    // map location history file
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

void get_params(void)
{
    double lat, lon;

    util_get_location(&lat, &lon, NULL, NULL);

    map_ctr_lat      = util_get_numeric_param(data_dir, "map_ctr_lat",      lat);
    map_ctr_lon      = util_get_numeric_param(data_dir, "map_ctr_lon",      lon);
    track_start_secs = util_get_numeric_param(data_dir, "track_start_secs", DFLT_TRACK_START_SECS);
    track_end_secs   = util_get_numeric_param(data_dir, "track_end_secs",   DFLT_TRACK_END_SECS);
    map_height_miles = util_get_numeric_param(data_dir, "map_height_miles", DFLT_MAP_HEIGHT_MILES);
    map_lock_north   = util_get_numeric_param(data_dir, "map_lock_north",   DFLT_MAP_LOCK_NORTH);
}

// -----------------  DISPLAY HANDLER  --------------------

// yyy move all variable declarations to top
void display(void)
{
    sdlx_texture_t         *map_texture;;
    sdlx_event_t            event;
    sdlx_loc_t              dest, *loc;
    double                  mag_heading, true_heading, map_heading;
    double                  map_rotate_angle;
    double                  distance_meters;
    struct loc_hist2_day_s *lh2d, *temp_lh2d;
    char                    str[100];
    int                     day, last_map_loc_secs;
    int                     precision;
    double                  width_miles;

    // yyy review this init section
    int state, end_secs;
    state = STATE_MONITOR;
    end_secs = track_end_secs;

    // init
    day = lh2->last_day - 2; //yyy temp
    map_texture = sdlx_create_texture(MAP_WH,MAP_WH);

    while (!end_program) {
        //
        // draw location points to the map_texture
        //

        //print_time_since_last_call();
        lh2d = &lh2->day[day % MAX_LH2_DAY];
        sdlx_set_render_target(map_texture);
        sdlx_clear_texture(map_texture, COLOR_BLACK);
        while (true) {
            if (state == STATE_MONITOR) {
                end_secs = track_end_secs;
            } else if (state == STATE_PLAY) {
                if (end_secs + 60 >= track_end_secs) {
                    state = STATE_MONITOR;
                } else {
                    end_secs += 60;
                }
            } else {
                printf("E %s: invalid state %d\n", progname, state);
            }
            int n = display_loc_points(lh2d, track_start_secs, end_secs, &last_map_loc_secs, &distance_meters);
            // yyy not sure about if this is good
            if (n != 0 || state != STATE_PLAY) {
                break;
            }
        }
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
        map_rotate_angle = (map_lock_north ? 0 : true_heading);
        sdlx_render_texture_rotated(map_texture, NULL, &dest, -map_rotate_angle, NULL, FLIP_NONE);

        // the rotated texture will spill over the display area below the map,
        // so clear the display area below the map
        sdlx_render_fill_rect(0, MAP_YB+1, sdlx_win_width, sdlx_win_height-MAP_YB+1, COLOR_BLACK);

        //
        // display map annotations
        //

        // display compass abbreviatios at the top, right, bottom, and left of the map
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

        // display map width scale 
        width_miles = map_height_miles * ((double)sdlx_win_width / MAP_WH);
        precision = (width_miles < 0.1 ? 6 :
                    (width_miles < 1   ? 2 :
                    (width_miles < 10  ? 1 :
                                         0)));
        util_double_to_str(width_miles, 0, precision, str, sizeof(str));
        sdlx_render_printf_ex(sdlx_win_width/2, ROW2Y(1),
                            FONT_SMALL, COLOR_WHITE, FLAG_X_CTR,
                            "map width %s miles", str);

        // display compass on the map
        display_compass(true_heading);

        // display mark on map at current location
        // yyy what should this do in playback mode
        temp_lh2d = &lh2->day[lh2->last_day % MAX_LH2_DAY];
        if (temp_lh2d->max_loc > 0) {
            double lat = temp_lh2d->loc[temp_lh2d->max_loc-1].latitude;
            double lon = temp_lh2d->loc[temp_lh2d->max_loc-1].longitude;
            display_map_mark(lat, lon, map_rotate_angle);
        }

        // draw border around the map display area
        sdlx_render_rect(0, 0, sdlx_win_width, MAP_WH, 5, COLOR_GREEN);

        //
        // display the control & status area
        // example:  yyy check these, or delete this comment
        //   Fri Oct 16
        //   14 miles
        //

        // date and time of the last loc
        sdlx_render_printf(0, MAP_YB,
                           "%s %s %d  %s", 
                           get_weekday_str(lh2d->year, lh2d->month, lh2d->day),
                           get_month_str(lh2d->month),
                           lh2d->day,
                           secs_to_str(last_map_loc_secs));

        // time of track start and end
        sdlx_render_printf(0, MAP_YB+ROW2Y(1), 
                           "%s - %s",
                           secs_to_str(track_start_secs),
                           secs_to_str(track_end_secs));

        // state
        sdlx_render_printf(0, MAP_YB+ROW2Y(2), 
                           "%s", 
                           state_str(state));


        // distance
        sdlx_render_printf(0, MAP_YB+ROW2Y(3), 
                           "%0.3f miles", 
                           distance_meters * METERS_TO_MILES);

        //
        // register additional events
        //

        reg_event_str(COL2X(0),  MAP_YB+ROW2Y(4), COLOR_LIGHT_BLUE, "PLAY", EVID_PLAY);
        reg_event_str(COL2X(6),  MAP_YB+ROW2Y(4), COLOR_LIGHT_BLUE, "START", EVID_START_SECS);
        reg_event_str(COL2X(12), MAP_YB+ROW2Y(4), COLOR_LIGHT_BLUE, "END", EVID_END_SECS);

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

        //
        // process events
        //

        switch (event.event_id) {
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_PREV_DAY: {
            int new_day = day - 1;

            if ((new_day) >= 0 && (lh2->last_day - new_day < MAX_LH2_DAY)) {  //yyy verify this works
                temp_lh2d = &lh2->day[new_day % MAX_LH2_DAY];
                if (temp_lh2d->max_loc > 0) {
                    day = new_day;
                }
            }
            break; }
        case EVID_NEXT_DAY:
            if (day < lh2->last_day) {
                day++;
            }
            break;
        case EVID_MOTION: {
            double xrel = event.u.motion.xrel * cosd(map_rotate_angle) - 
                          event.u.motion.yrel * sind(map_rotate_angle);
            double yrel = event.u.motion.xrel * sind(map_rotate_angle) + 
                          event.u.motion.yrel * cosd(map_rotate_angle);
            map_ctr_lat  += yrel * 
                            (map_height_miles / MAP_WH) / 
                            MILES_PER_DEGREE_LATITUDE;
            map_ctr_lon -= xrel * 
                           (map_height_miles / MAP_WH) / 
                           MILES_PER_DEGREE_LONGITUDE(map_ctr_lat);
            util_set_numeric_param(data_dir, "map_ctr_lat",  map_ctr_lat);
            util_set_numeric_param(data_dir, "map_ctr_lon", map_ctr_lon);
            break; }
        case EVID_PINCH:
            if (event.u.pinch.scale == 0) break;
            map_height_miles /= event.u.pinch.scale;
            if (map_height_miles < 0.0015) map_height_miles = 0.0015;
            if (map_height_miles > 4500) map_height_miles = 4500;
            util_set_numeric_param(data_dir, "map_height_miles", map_height_miles);
            break;
        case EVID_MAP_LOCK_NORTH:
            map_lock_north = !map_lock_north;
            util_set_numeric_param(data_dir, "map_lock_north", map_lock_north);
            break;
        case EVID_PLAY:
            state = STATE_PLAY;
            end_secs = track_start_secs;
            break;
        case EVID_START_SECS: {
            char *s, dflt[50];
            int cnt, hours, minutes;
            get_current_hms(&hours, &minutes, NULL);
            sprintf(dflt, "%02d.%02d", hours, minutes);
            s = sdlx_get_input_str("Start hh.mm", true, dflt);
            cnt = sscanf(s, "%d.%d", &hours, &minutes);
            if (cnt == 1 && hours >= 0 && hours < 24) {
                track_start_secs = hours*3600;
                util_set_numeric_param(data_dir, "track_start_secs", track_start_secs);
            } else if (cnt == 2 && hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60) {
                track_start_secs = hours*3600 + minutes*60;
                util_set_numeric_param(data_dir, "track_start_secs", track_start_secs);
            } 
            break; }
        case EVID_END_SECS: {
            char *s, dflt[50];
            int cnt, hours, minutes;
            get_current_hms(&hours, &minutes, NULL);
            sprintf(dflt, "%02d.%02d", hours, minutes);
            s = sdlx_get_input_str("End hh.mm", true, dflt);
            cnt = sscanf(s, "%d.%d", &hours, &minutes);
            if (cnt == 1 && hours >= 0 && hours < 24) {
                track_end_secs = hours*3600;
                util_set_numeric_param(data_dir, "track_end_secs", track_end_secs);
            } else if (cnt == 2 && hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60) {
                track_end_secs = hours*3600 + minutes*60;
                util_set_numeric_param(data_dir, "track_end_secs", track_end_secs);
            } 
            break; }
        case EVID_QUIT:
            end_program = true;
            break;
        }
    }

    sdlx_destroy_texture(map_texture);
}

int display_loc_points(struct loc_hist2_day_s *lh2d, int start_secs, int end_secs, int *last_map_loc_secs,
                       double *distance_meters) 
{
    int          i, x, y, n, secs=0;
    double       k_lat, k_lon;
    sdlx_point_t points[10000];  // xxx use define from location.h
    sdlx_color_t colors[10000];
    int          start_minute, end_minute, span_minute, span_wvlen;
    long         time_execution_usec;

    *distance_meters = 0;

    start_minute = start_secs/60;
    end_minute   = end_secs/60;
    span_minute  = (end_minute - start_minute) + 1;
    span_wvlen   = WVLEN_PURPLE - WVLEN_RED;

    time_execution_usec = util_microsec_timer();

    k_lat = MILES_PER_DEGREE_LATITUDE / map_height_miles * MAP_WH;
    k_lon = MILES_PER_DEGREE_LONGITUDE(map_ctr_lat) / map_height_miles * MAP_WH;

    n = 0;
    for (i = 0; i < lh2d->max_loc; i++) {
        int tmp_secs = lh2d->loc[i].secs;
        if (tmp_secs < start_secs) {
            continue;
        }
        if (tmp_secs >= end_secs) {
            break;
        }
        secs = tmp_secs;  // xxx yyy optimize

        *distance_meters += lh2d->loc[i].distance;

        x = MAP_XY_CTR + (lh2d->loc[i].longitude - map_ctr_lon) * k_lon;
        if (x < 0 || x > MAP_WH) {
            continue;
        }

        y = MAP_XY_CTR - (lh2d->loc[i].latitude - map_ctr_lat) * k_lat;
        if (y < 0 || y > MAP_WH) {
            continue;
        }

        points[n].x = x;
        points[n].y = y;

        int wvlen = WVLEN_RED + 
                    ((secs/60.0 - start_minute) / span_minute) * 
                    span_wvlen;
        colors[n] = sdlx_wavelength_to_color(wvlen);

        n++;
    }

    *last_map_loc_secs = secs;

    sdlx_render_points(points, n, colors, 0, NULL, MAX_POINT_SIZE);

    //printf("duration = %ld ms\n", (util_microsec_timer() - time_execution_usec) / 1000);

    return n;
}

// yyy comments
void display_map_mark(double lat, double lon, double map_rotate_angle)
{
    double k_lat, k_lon, x, y, x1, y1;
    int length = 50;

    k_lat = MILES_PER_DEGREE_LATITUDE / map_height_miles * MAP_WH;
    k_lon = MILES_PER_DEGREE_LONGITUDE(map_ctr_lat) / map_height_miles * MAP_WH;

    x = MAP_XY_CTR + (lon - map_ctr_lon) * k_lon;
    if (x < 0 || x > MAP_WH) return;

    y = MAP_XY_CTR - (lat - map_ctr_lat) * k_lat;
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

char *state_str(int state)
{
    if (state == STATE_MONITOR) return "MONITOR";
    if (state == STATE_PLAY) return "PLAY";
    return "????";
}
