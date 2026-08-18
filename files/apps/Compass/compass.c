#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include <sdlx.h>
#include <utils.h>
#include <svcs.h>
#include "svcs/Location/location.h"
#include "lib/lib.h"

#define EVID_SLCT   1
#define EVID_SHOW   2
#define EVID_README 3

#define MAGNETIC_COMPASS 0
#define TRUE_COMPASS     1

#define DEG_TO_RAD  (M_PI / 180)

// variables
char *progname;
char *data_dir;

int             view = MAGNETIC_COMPASS;
double          mag_decl_degrees = INVALID_NUMBER;
char            mag_decl_locname[21];
sdlx_texture_t *compass;
bool            show;

// prototypes
int init_compass_texture(void);
void init_mag_decl(void);
void cleanup(void);
double smooth(double newval);
char *abbreviation(double heading);
void normalize(double *angle);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    int          rc, x, y;
    sdlx_event_t event;
    double       mag_heading, true_heading, compass_heading;
    sdlx_loc_t   dest;
    bool         end_program = false;

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // initialize
    rc = init_compass_texture();
    if (rc != 0) {
        return 1;
    }
    init_mag_decl();
    view = util_get_numeric_param(data_dir, "view", MAGNETIC_COMPASS);
    show = util_get_numeric_param(data_dir, "show", false);

    // runtime loop
    while (!end_program) {
        // init the backbuffer, and init print font/color
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // read the magnetic heading sensor
#ifdef ANDROID
        sdlx_sensor_read_mag_heading(&mag_heading);
        mag_heading = smooth(mag_heading);
#else
        mag_heading = 0;
#endif

        // if magnetic heading is valid then
        //   display compass
        // else
        //   display "NO DATA"
        // endif
        if (mag_heading != INVALID_NUMBER) {
            // determine true heading
            if (mag_decl_degrees != INVALID_NUMBER) {
                true_heading = mag_heading + mag_decl_degrees;
                normalize(&true_heading);
            } else {
                true_heading = INVALID_NUMBER;
                view = MAGNETIC_COMPASS;
            }

            // display white background in the area where the compass 
            // will be displayed
            sdlx_render_fill_rect(0, 100, 1000, 1000, COLOR_WHITE);

            // draw reference mark at the top center 
            for (x = sdlx_win_width/2-3; x < sdlx_win_width/2+3; x++) {
                sdlx_render_line(x, 100, x, 150, COLOR_BLACK);
            }

            // determine compass heading, based on view selected
            compass_heading = (view == MAGNETIC_COMPASS ? mag_heading : true_heading);
            normalize(&compass_heading);

            // draw the compass rotated by compass_heading
            dest.x = 50;
            dest.y = 150;
            dest.w = 900;
            dest.h = 900;
            sdlx_render_texture_rotated(compass, NULL, &dest, -compass_heading, NULL, FLIP_NONE);

            // if show is enabled then draw a reference point at true north
            if (show && view == MAGNETIC_COMPASS && true_heading != INVALID_NUMBER) {
                x = 500 + 345 * sin((true_heading + 180) * DEG_TO_RAD);
                y = 600 + 345 * cos((true_heading + 180) * DEG_TO_RAD);
                sdlx_render_point(x, y, COLOR_BLUE, MAX_POINT_SIZE);
            }

            // print the heading and the heading abbreviation below 
            // the area where the compass is displayed
            y = 1100 + 1.0 * sdlx_char_height(FONT_LARGE);
            sdlx_render_printf_ex2(sdlx_win_width / 2, y,
                                   FONT_LARGE, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%s", view == MAGNETIC_COMPASS ? "MAG" : "TRUE");
            y += 1.5 * sdlx_char_height(FONT_LARGE);
            sdlx_render_printf_ex2(sdlx_win_width / 2, y,
                                   FONT_LARGE, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%.0f", compass_heading);
            y += 1.5 * sdlx_char_height(FONT_LARGE);
            sdlx_render_printf_ex2(sdlx_win_width / 2, y,
                                   FONT_LARGE, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%s", abbreviation(compass_heading));
            y += 1.0 * sdlx_char_height(FONT_LARGE);

            // if show is enabled and mag_decl_degrees is available then print the mag_decl_degrees
            if (show && mag_decl_degrees != INVALID_NUMBER) {
                sdlx_render_printf_ex2(sdlx_win_width / 2, y,
                                       FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, 
                                       "decl = %0.1f", mag_decl_degrees);
                y += 1.0 * sdlx_char_height(FONT_NORMAL);
                sdlx_render_printf_ex2(sdlx_win_width / 2, y,
                                       FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, 
                                       "%s", mag_decl_locname);
            }
        } else {
            sdlx_render_printf_ex2(
                sdlx_win_width / 2, 500, 
                FONT_LARGE, COLOR_WHITE, FLAG_XY_CTR, 
                "%s", "NO DATA");
        }

        // register EVID_SHOW_README_FILE event
        reg_event_show_readme_file();

        // register control event to end program
        sdlx_register_control_events(EVID_SLCT, "Slct",
                                     EVID_SHOW, (show ? "Hide" : "Show"),
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for an event with 50 ms timeout;
        // if no event, then redraw display
        sdlx_get_event(50000, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_QUIT:
            end_program = true;
            break;
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_SLCT:
            if (view == MAGNETIC_COMPASS && mag_decl_degrees != INVALID_NUMBER) {
                view = TRUE_COMPASS;
            } else {
                view = MAGNETIC_COMPASS;
            }
            util_set_numeric_param(data_dir, "view", view);
            break;
        case EVID_SHOW:
            show = !show;
            util_set_numeric_param(data_dir, "show", show);
            break;
        }
    }

    // cleanup and end program
    cleanup();
    printf("I %s: terminating\n", progname);
    return 0;
}

int init_compass_texture(void)
{
    int            rc, w, h;
    unsigned char *pixels;

    // read the compass image pixels
    rc = util_read_png_file(data_dir, "compass.png", &pixels, &w, &h);
    if (rc != 0) {
        printf("E %s failed to decode png file %s\n", progname, "compass.png");
        return -1;
    }

    // create compass image texture
    compass = sdlx_create_texture(w, h);
    if (compass == NULL) {
        printf("E %s failed to create compass texture\n", progname);
        return -1;
    }
    sdlx_set_texture_pixels(compass, (unsigned int*)pixels);

    // done with pixels
    free(pixels);

    // return success
    return 0;
}

#define MAG_DECL_JSON "mag_decl.json"
#define KEY           "zNEw7"

void init_mag_decl(void)
{
    double        latitude, longitude;
    int           rc;
    char          url[200], cmd[300];
    char         *str, *end_ptr;
    void         *json;
    int           len_ret;
    json_value_t *value;
    double        param_mag_decl_lat;
    double        param_mag_decl_long;
    double        param_mag_decl_degrees;
    bool          okay_to_use;

    // preset mag_decl to invalid
    mag_decl_degrees = INVALID_NUMBER;

    // get current latitude and longitude
    util_get_location(&latitude, &longitude, NULL, NULL);
    if (latitude == INVALID_NUMBER || longitude == INVALID_NUMBER) {
        printf("E %s: failed to get lat/long\n", progname);
        return;
    }

    // if valid mag_decl_degrees is available from params then use it
    // - read param_mag_decl_degrees, and check if the read succeeded
    param_mag_decl_degrees = util_get_numeric_param(data_dir, "mag_decl_degrees", INVALID_NUMBER);
    if (param_mag_decl_degrees != INVALID_NUMBER) {
        // - read additional params and determine if param_mag_decl_degrees is okay to use
        param_mag_decl_lat  = util_get_numeric_param(data_dir, "mag_decl_lat",  INVALID_NUMBER);
        param_mag_decl_long = util_get_numeric_param(data_dir, "mag_decl_long", INVALID_NUMBER);
        okay_to_use =  (fabs(param_mag_decl_lat - latitude) < 1.0) &&
                       (fabs(param_mag_decl_long - longitude) < 1.0);

        // - if okay to use then set global variables mag_decl_degrees and mag_decl_locname, and return
        if (okay_to_use) {
            // - set global mag_decl_degrees from param value
            mag_decl_degrees = param_mag_decl_degrees;
            // - set global mag_decl_locname from param value
            str = util_get_str_param(data_dir, "mag_decl_locname", "Loc Not Found");
            strncpy(mag_decl_locname, str, sizeof(mag_decl_locname)-1);
            free(str);
            // - debug print and return
            printf("I %s: using saved mag_decl %0.3f, loc %s\n", progname, mag_decl_degrees, mag_decl_locname);
            return;
        }
    }

    // the following code acquires the mag_decl_degrees from www.ngdc.noaa.gov

    // delete existing mag_decl.json file
    util_delete_file(data_dir, MAG_DECL_JSON);

    // execute curl cmd to get mag declination from NOAA, in json format
    sprintf(url, 
      "\"https://www.ngdc.noaa.gov/geomag-web/calculators/calculateDeclination?lat1=%0.4f&lon1=%0.4f&key=%s&resultFormat=json\"",
      latitude, longitude, KEY);
    sprintf(cmd, "curl --silent --max-time 30 --output %s/%s %s",
            data_dir, MAG_DECL_JSON, url);
    printf("I %s: RUNNING '%s'\n", progname, cmd);
    rc = system(cmd);
    rc = WEXITSTATUS(rc);
    if (rc != 0) {
        printf("E %s: curl failed, rc=0x%x\n", progname, rc);
        return;
    }

    // read MAG_DECL_JSON file
    str = util_read_file(data_dir, MAG_DECL_JSON, &len_ret);
    if (str == NULL) {
        printf("E %s: parse_info, read %s, %s\n", progname, MAG_DECL_JSON, strerror(errno));
        return;
    }

    // init json parser
    json = util_json_parse(str, &end_ptr);
    if (json == NULL) {
        printf("E %s: json parse failed\n", progname);
        free(str);
        return;
    }

    // read the declination from the json
    value = util_json_get_value(json, "result", "0", "declination", NULL);
    if (value->type != JSON_TYPE_NUMBER) {
        printf("E %s: declination value type=%d is not a number\n", progname, value->type);
        free(str);
        util_json_free(json);
        return;
    }

    // set global mag_decl variable to the value obtained from the json
    mag_decl_degrees = value->u.number;
    printf("I %s: got new mag_decl = %0.3f\n", progname, mag_decl_degrees);

    // get name of nearest city/town, and save in global variable mag_decl_locname
    char req_data[MAX_SVC_REQ_DATA];
    memset(req_data, 0, sizeof(req_data));
    *(double*)(&req_data[0]) = latitude;
    *(double*)(&req_data[8]) = longitude;
    svc_req_t *req = svc_req_init(SVC_LOCATION_REQ_GET_LOC_NAME_FROM_LAT_LONG, req_data, sizeof(req_data));
    rc = svc_make_req("Location", req, 5);
    if (rc == 0) {
        strncpy(mag_decl_locname, req->data, sizeof(mag_decl_locname)-1);
    } else {
        strncpy(mag_decl_locname, "Loc Not Found", sizeof(mag_decl_locname)-1);
    }

    // save mag_decl in params
    util_set_numeric_param(data_dir, "mag_decl_lat",     latitude);
    util_set_numeric_param(data_dir, "mag_decl_long",    longitude);
    util_set_numeric_param(data_dir, "mag_decl_degrees", mag_decl_degrees);
    util_set_str_param(data_dir,     "mag_decl_locname", mag_decl_locname);

    // cleanup and return
    util_json_free(json);
    free(str);
    util_delete_file(data_dir, MAG_DECL_JSON);
}

void cleanup(void)
{
    sdlx_destroy_texture(compass);
}

// -----------------  UTILS  ---------------------------------------------

// this routine removes jitter from the mag_heading sensor reading
double smooth(double newval)
{
    static double smoothed = INVALID_NUMBER;
    double delta;

    if (newval == INVALID_NUMBER) {
        return INVALID_NUMBER;
    }

    if (smoothed == INVALID_NUMBER) {
        smoothed = newval;
        return smoothed;
    }

    delta = newval - smoothed;
    if (delta < -180) delta += 360;
    if (delta >  180) delta -= 360;

    smoothed = smoothed + 0.1 * delta;
    if (smoothed < 0) smoothed += 360;
    if (smoothed >= 360) smoothed -= 360;

    return smoothed;
}

// this routine returns the heading abbreviation
char *abbreviation(double heading) 
{
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

void normalize(double *angle)
{
    while (*angle < 0) {
        *angle += 360;
    }

    while (*angle >= 360) {
        *angle -= 360;
    }
}
