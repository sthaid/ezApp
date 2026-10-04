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

#define K_SMOOTH 0.1

// variables
char *progname;
char *data_dir;

int             view = MAGNETIC_COMPASS;
double          mag_decl_degrees = INVALID_NUMBER;
sdlx_texture_t *compass;
bool            show;

// prototypes
int init_compass_texture(void);
void cleanup(void);
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
    mag_decl_degrees = get_mag_decl();  //xxx what if this fails

    // runtime loop
    while (!end_program) {
        // init the backbuffer, and init print font/color
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // read the magnetic heading sensor
#ifdef ANDROID  // xxx only run on android?
        sdlx_sensor_read_mag_heading(&mag_heading, K_SMOOTH);
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

            // print the heading and the heading abbreviation below 
            // the area where the compass is displayed
            y = 1100 + 1.0 * sdlx_char_height(FONT_LARGE);
            sdlx_render_printf_ex(sdlx_win_width / 2, y,
                                   FONT_LARGE, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%s", view == MAGNETIC_COMPASS ? "MAG" : "TRUE");
            y += 1.5 * sdlx_char_height(FONT_LARGE);
            sdlx_render_printf_ex(sdlx_win_width / 2, y,
                                   FONT_LARGE, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%.0f", compass_heading);
            y += 1.5 * sdlx_char_height(FONT_LARGE);
            sdlx_render_printf_ex(sdlx_win_width / 2, y,
                                   FONT_LARGE, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%s", abbreviation(compass_heading));
            y += 1.0 * sdlx_char_height(FONT_LARGE);

            // if show is enabled and mag_decl_degrees is available then display the mag_decl_degrees
            if (show && mag_decl_degrees != INVALID_NUMBER) {
                y = sdlx_win_height - 2 * sdlx_char_height(FONT_SMALL);
                sdlx_render_printf_ex(sdlx_win_width / 2, y,
                                       FONT_SMALL, COLOR_WHITE, FLAG_X_CTR, 
                                       "decl = %0.1f", mag_decl_degrees);
            }
        } else {
            sdlx_render_printf_ex(
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
            break;
        case EVID_SHOW:
            show = !show;
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

void cleanup(void)
{
    sdlx_destroy_texture(compass);
}

// -----------------  UTILS  ---------------------------------------------

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
