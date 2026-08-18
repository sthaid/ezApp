#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/lib/lib.h"

//
// defines
//

// misc constants
#define RAD_TO_DEG (180 / M_PI)
#define DEG_TO_RAD (M_PI / 180)
#define TWO_PI     (2 * M_PI)

// texture circle radius
#define SMALL_CIRCLE_RADIUS 25
#define LARGE_CIRCLE_RADIUS 500

// vertical display orientation texture width/height
#define VERT_TEXTURE_WH 1000

// when to display indication that the tilt is near to 0
#define TILT_CLOSE_TO_ZERO 0.2

// events used by display_tilt_horizontal
#define EVID_HORIZ_INCR_MAX_BULLS_EYE 1
#define EVID_HORIZ_DECR_MAX_BULLS_EYE 2
#define EVID_HORIZ_CALIBRATE          3

// events used by display_tilt_vertical
#define EVID_VERT_MINUS      11
#define EVID_VERT_PLUS       12
#define EVID_VERT_CALIBRATE  13

// events used by cal_query
#define EVID_CAL_SAVE        21
#define EVID_CAL_UNCALIBRATE 22
#define EVID_CAL_CANCEL      23

//
// variables
//

char *progname;
char *data_dir;

bool end_program = false;

sdlx_texture_t *green_circle;
sdlx_texture_t *blue_circle;
sdlx_texture_t *red_circle;
sdlx_texture_t *gray_circle;
sdlx_texture_t *light_gray_circle;
sdlx_texture_t *vert;
    
//
// prototypes
//

void smooth(double newval, double *smoothed);
void no_accelerometer(void);
sdlx_texture_t *create_filled_circle_texture(int radius, sdlx_color_t color);
int cal_query(void);
void display_tilt_horizontal(double ax, double ay, double az, double roll, double pitch);
void display_tilt_vertical(double ax, double ay, double az, double roll, double pitch);

// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
    int    rc;
    double ax_raw, ay_raw, az_raw;
    double ax=INVALID_NUMBER, ay=INVALID_NUMBER, az=INVALID_NUMBER;
    double roll_raw, pitch_raw;
    double roll=INVALID_NUMBER, pitch=INVALID_NUMBER;

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // create textures for horizontal display
    green_circle      = create_filled_circle_texture(SMALL_CIRCLE_RADIUS, COLOR_GREEN);
    blue_circle       = create_filled_circle_texture(SMALL_CIRCLE_RADIUS, COLOR_BLUE);
    red_circle        = create_filled_circle_texture(SMALL_CIRCLE_RADIUS, COLOR_RED);
    gray_circle       = create_filled_circle_texture(LARGE_CIRCLE_RADIUS, COLOR_GRAY);
    light_gray_circle = create_filled_circle_texture(LARGE_CIRCLE_RADIUS, COLOR_LIGHT_GRAY);

    // create texture for vertical display
    vert = sdlx_create_texture(VERT_TEXTURE_WH, VERT_TEXTURE_WH);

    // set default font size and color
    sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);

#ifndef ANDROID
    // unit test on Linux
    while (!end_program) {
        //display_tilt_vertical(0, 9.8, 0,  2, 90);
        display_tilt_horizontal(0, 0, 9.8,  2, 1);
    }
    return 0;
#endif

    // runtime loop
    while (!end_program) {
        // read gravity accel sensor
        rc = sdlx_sensor_read_gravity_accel(&ax_raw, &ay_raw, &az_raw);
        if (rc != 0) {
            no_accelerometer();
            continue;
        }

        // smooth the sensor values
        smooth(ax_raw, &ax);
        smooth(ay_raw, &ay);
        smooth(az_raw, &az);

        // obtain smoothed roll/pitch values, units=degrees
        sdlx_sensor_read_roll_pitch(&roll_raw, &pitch_raw);
        smooth(roll_raw, &roll);
        smooth(pitch_raw, &pitch);

        // display the tilt
        if (az > 7) {
            display_tilt_horizontal(ax, ay, az, roll, pitch);
        } else {
            display_tilt_vertical(ax, ay, az, roll, pitch);
        }
    }

    // free allocations
    sdlx_destroy_texture(green_circle);
    sdlx_destroy_texture(blue_circle);
    sdlx_destroy_texture(red_circle);
    sdlx_destroy_texture(gray_circle);
    sdlx_destroy_texture(light_gray_circle);
    sdlx_destroy_texture(vert);

    // quit sdl subsystems and end program
    printf("I %s: terminating\n", progname);
    return 0;
}

#define SMOOTH_K 0.95
void smooth(double newval, double *smoothed)
{
    if (*smoothed == INVALID_NUMBER) {
        *smoothed = newval;
        return;
    }

    *smoothed = SMOOTH_K * *smoothed + (1.0 - SMOOTH_K) * newval;
}

void no_accelerometer(void)
{
    sdlx_event_t event;

    sdlx_display_init(COLOR_BLACK, PORTRAIT);
    reg_event_show_readme_file();
    sdlx_register_control_events(0, NULL,
                                 0, NULL,
                                 EVID_QUIT, "X");
    sdlx_render_printf_ex2(sdlx_win_width/2, sdlx_win_height/2, 
                           FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, 
                           "%s", "No Accelerometer");
    sdlx_display_present();

    sdlx_get_event(-1, &event);

    switch (event.event_id) {
    case EVID_QUIT:
        end_program = true;
        break;
    case EVID_SHOW_README_FILE:
        show_file(data_dir, "README");
        break;
    }
}

sdlx_texture_t *create_filled_circle_texture(int radius, sdlx_color_t color)
{
    int w, h;
    sdlx_texture_t *t;

    w = h = 2 * radius;

    t = sdlx_create_texture(w, h);
    sdlx_set_render_target(t);
    sdlx_render_fill_circle(w/2, h/2, radius, color);
    sdlx_set_render_target(NULL);

    return t;
}

int cal_query(void)
{
    sdlx_loc_t *loc;
    sdlx_event_t event;

    sdlx_display_init(COLOR_BLACK, PORTRAIT);

    loc = sdlx_render_printf_ex1(0, ROW2Y(3), FONT_NORMAL, COLOR_LIGHT_BLUE, "%s", "Save");
    sdlx_register_event(loc, EVID_CAL_SAVE);
    loc = sdlx_render_printf_ex1(0, ROW2Y(6), FONT_NORMAL, COLOR_LIGHT_BLUE, "%s", "Uncalibrate");
    sdlx_register_event(loc, EVID_CAL_UNCALIBRATE);
    loc = sdlx_render_printf_ex1(0, ROW2Y(9), FONT_NORMAL, COLOR_LIGHT_BLUE, "%s", "Cancel");
    sdlx_register_event(loc, EVID_CAL_CANCEL);

    sdlx_display_present();

    sdlx_get_event(-1, &event);

    if (event.event_id == EVID_CAL_SAVE || 
        event.event_id == EVID_CAL_UNCALIBRATE || 
        event.event_id == EVID_CAL_CANCEL) 
    {
        return event.event_id;
    } else {
        printf("E %s: cal_query received unexpected event_id %d\n", progname, event.event_id);
        return EVID_CAL_CANCEL;
    }
}

// -----------------  DISPLAY TILT - HORIZONTAL ORIENTATION  -------------------

#define MAX_BULLS_EYE_DEFAULT 5

void display_tilt_horizontal(double ax, double ay, double az, double roll_raw, double pitch_raw)
{
    int                 diameter, xctr, yctr, deg, x, y, rc;
    sdlx_texture_t     *t;
    double              roll, pitch;
    double              tilt_dir, tilt_amount;
    sdlx_event_t        event;
    sdlx_loc_t         *loc, dest;

    static bool         params_initialized;
    static int          max_bulls_eye = MAX_BULLS_EYE_DEFAULT;
    static double       cal_horiz_roll;
    static double       cal_horiz_pitch;

    // on first call, read params for the horizontal orientation
    if (!params_initialized) {
        max_bulls_eye = util_get_numeric_param(data_dir, "max_bulls_eye", MAX_BULLS_EYE_DEFAULT);
        cal_horiz_roll = util_get_numeric_param(data_dir, "cal_horiz_roll", INVALID_NUMBER);
        cal_horiz_pitch = util_get_numeric_param(data_dir, "cal_horiz_pitch", INVALID_NUMBER);
        params_initialized = true;
    }

    // init the backbuffer
    sdlx_display_init(COLOR_BLACK, PORTRAIT);

    // compenstate raw roll/pitch values using calibration values
    if (cal_horiz_roll != INVALID_NUMBER && cal_horiz_pitch != INVALID_NUMBER) {
        roll  = roll_raw - cal_horiz_roll;
        pitch = pitch_raw - cal_horiz_pitch;
    } else {
        roll  = roll_raw;
        pitch = pitch_raw;
    }

    // compute tilt direction and amount
    tilt_dir    = atan2(-pitch, roll) * RAD_TO_DEG - 90;
    tilt_amount = sqrt(roll*roll + pitch*pitch);

    // init center location of the bulls-eye
    xctr = sdlx_win_width/2;
    yctr = 100 + sdlx_win_width / 2;

    // draw bulls-eye
    t = gray_circle;
    for (deg = max_bulls_eye; deg >= 1; deg--) {
        diameter = nearbyint((double)sdlx_win_width / max_bulls_eye * deg);
        t = (t == gray_circle ? light_gray_circle : gray_circle);
        dest.x = xctr-diameter/2;
        dest.y = yctr-diameter/2;
        dest.w = diameter;
        dest.h = diameter;
        sdlx_render_texture(t, NULL, &dest);
    }

    // display max_bulls_eye radius, in degrees
    y = yctr + sdlx_win_width / 2 + 0.5 * sdlx_char_height_dflt;
    sdlx_render_printf_ex2(sdlx_win_width/2, y,
                           FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
                           "max %d deg", max_bulls_eye);

    // display tilt_amount
    y = yctr + sdlx_win_width / 2 + 3 * sdlx_char_height_dflt;
    sdlx_render_printf_ex2(xctr, y,
                           FONT_LARGE, COLOR_WHITE, FLAG_X_CTR, 
                           "%0.1f", tilt_amount);

    // if not calibrated then display 'uncalibrated' in RED
    if (cal_horiz_roll == INVALID_NUMBER || cal_horiz_pitch == INVALID_NUMBER) {
        sdlx_render_printf_ex2(sdlx_win_width / 2, y + 2.0 * sdlx_char_height(FONT_LARGE),
                               FONT_NORMAL, COLOR_RED,
                               FLAG_XY_CTR, "uncalibrated");
    }

    // register EVID_CALIBRATE
    loc = sdlx_render_printf_ex2(
                sdlx_win_width/2, y + 3.0 * sdlx_char_height(FONT_LARGE),
                FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_X_CTR, 
                "%s", "CALIBRATE");
    sdlx_register_event(loc, EVID_HORIZ_CALIBRATE);

    // limit tilt amount to the max that can be displayed on the bulls-eye
    if (tilt_amount > max_bulls_eye) {
        tilt_amount = max_bulls_eye;
    }

    // display small circle on the bulls-eye pattern, 
    // at location indicating the tilt direction and amount
    x = xctr + tilt_amount * sin(tilt_dir*DEG_TO_RAD) * ((double)(sdlx_win_width/2) / max_bulls_eye);
    y = yctr + tilt_amount * cos(tilt_dir*DEG_TO_RAD) * ((double)(sdlx_win_width/2) / max_bulls_eye);

    t = ((fabs(tilt_amount) < TILT_CLOSE_TO_ZERO) ? green_circle :
        ((fabs(tilt_amount) < max_bulls_eye)      ? blue_circle :
                                                    red_circle));

    dest.x = x-SMALL_CIRCLE_RADIUS;
    dest.y = y-SMALL_CIRCLE_RADIUS;
    dest.w = 2*SMALL_CIRCLE_RADIUS;
    dest.h = 2*SMALL_CIRCLE_RADIUS;
    sdlx_render_texture(t, NULL, &dest);

    // display dot at center of bulls_eye
    sdlx_render_point(xctr, yctr, COLOR_BLACK, 9);

    // register EVID_SHOW_README_FILE event
    reg_event_show_readme_file();

    // register control event to
    // - end program
    // - adjust max_bulls_eye
    sdlx_register_control_events(EVID_HORIZ_DECR_MAX_BULLS_EYE, "-",
                                 EVID_HORIZ_INCR_MAX_BULLS_EYE, "+",
                                 EVID_QUIT, "X");

    // present the display
    sdlx_display_present();

    // wait for event, with 10 ms timeout
    sdlx_get_event(10000, &event);

    // process events
    switch (event.event_id) {
    case EVID_HORIZ_CALIBRATE:
        rc = cal_query();
        if (rc == EVID_CAL_CANCEL) break;
        if (rc == EVID_CAL_SAVE) {
            cal_horiz_roll  = roll_raw;
            cal_horiz_pitch = pitch_raw;
        } else {
            cal_horiz_roll  = INVALID_NUMBER;
            cal_horiz_pitch = INVALID_NUMBER;
        }
        util_set_numeric_param(data_dir, "cal_horiz_roll", cal_horiz_roll);
        util_set_numeric_param(data_dir, "cal_horiz_pitch", cal_horiz_pitch);
        break;
    case EVID_HORIZ_INCR_MAX_BULLS_EYE:
        if (max_bulls_eye < 20) {
            max_bulls_eye++;
            util_set_numeric_param(data_dir, "max_bulls_eye", max_bulls_eye);
        }
        break;
    case EVID_HORIZ_DECR_MAX_BULLS_EYE:
        if (max_bulls_eye > 1) {
            max_bulls_eye--;
            util_set_numeric_param(data_dir, "max_bulls_eye", max_bulls_eye);
        }
        break;
    case EVID_SHOW_README_FILE:
        show_file(data_dir, "README");
        break;
    case EVID_QUIT:
        end_program = true;
        break;
    }
}

// -----------------  DISPLAY TILT - VERTICAL ORIENTATION  -------------------

#define Y_OFFSET             30
#define CHORD_LEN            850
#define MAX_ARC              100
#define ARC_SPAN_DEG_DEFAULT 20

void display_tilt_vertical(double ax, double ay, double az, double roll, double pitch)
{
    int             rotate_deg;
    double          angle_deg, angle_rad, angle_uncal_deg;
    sdlx_event_t    event;
    sdlx_point_t    points[MAX_ARC];
    int             i, j, x, y, x_offset, rc;
    int             tick_deg, tick_delta_deg;
    sdlx_texture_t *vert_circle_texture;
    char            cal_param_name[50];
    sdlx_loc_t     *loc, tmp_loc, dest;

    // statics
    static double       arc_span_deg, arc_span_rad, arc_radius, arc_radius_squared;
    static double       arc_initialized;
    static sdlx_point_t arc[MAX_ARC];
    static int          max_arc;
    static double       cal[4];
    static bool         params_initialized;

    // on first call, read params for the vertical orientation
    if (!params_initialized) {
        printf("I %s: reading vertical orientation params\n", progname);

        arc_span_deg = util_get_numeric_param(data_dir, "arc_span_deg", ARC_SPAN_DEG_DEFAULT);
        arc_span_rad = arc_span_deg * DEG_TO_RAD;

        for (i = 0; i < 4; i++) {
            sprintf(cal_param_name, "cal_vertical_%d", 90*i);
            cal[i] = util_get_numeric_param(data_dir, cal_param_name, INVALID_NUMBER);
        }

        params_initialized = true;
    }

    // init the backbuffer
    sdlx_display_init(COLOR_BLACK, PORTRAIT);

    // set render target to the rendering texture, and
    // clear the rendering texture
    sdlx_set_render_target(vert);
    sdlx_clear_texture(vert, COLOR_BLACK);

    // determine the rotation needed for the device orientation
    if (fabs(ay) > fabs(ax)) {
        rotate_deg = (ay > 0 ? 0 : 180);
    } else {
        rotate_deg = (ax > 0 ? 90 : 270);
    }

    // determine the tilt angle;
    // the roll/pitch values give good results even when the device 
    // orientation deviates from vertical
    switch (rotate_deg) {
    case 0:   angle_uncal_deg = -roll;  break;
    case 90:  angle_uncal_deg = -pitch; break;
    case 180: angle_uncal_deg =  roll;  break;
    case 270: angle_uncal_deg =  pitch; break;
    }
    angle_deg = angle_uncal_deg - ((cal[rotate_deg/90] != INVALID_NUMBER) ? cal[rotate_deg/90] : 0);
    angle_rad = angle_deg * DEG_TO_RAD;

    // initialize arc points for the current selected arc_span
    x_offset = (sdlx_win_width - CHORD_LEN) / 2;
    if (arc_initialized != arc_span_rad) {
        printf("I %s: initializing arc points for span %0.0f\n", progname, arc_span_deg);
        arc_radius = (CHORD_LEN/2) / sin(arc_span_rad/2);
        arc_radius_squared = arc_radius * arc_radius;
        max_arc = 0;
        for (x = -CHORD_LEN/2; x <= CHORD_LEN/2; x+= 10) {
            y = sqrt(arc_radius_squared - x*x);
            arc[max_arc].x = x + CHORD_LEN/2 + x_offset;
            arc[max_arc].y = arc_radius - y + Y_OFFSET;
            max_arc++;
            if (max_arc == MAX_ARC) {
                printf("E %s: too many arc points\n", progname);
                break;
            }
        }
        arc_initialized = arc_span_rad;
    }

    // draw arc, made up of 5 arc lines for a better display appearance
    for (i = -2; i <= 2; i++) {
        for (j = 0; j < max_arc; j++) {
            points[j].x = arc[j].x;
            points[j].y = arc[j].y + i;
        }
        sdlx_render_lines(points, max_arc, COLOR_WHITE);
    }

    // draw small cirle on the arc, at the vertical location;
    // use green circle when within 0.2 degrees of arc center
    vert_circle_texture = (fabs(angle_deg) < 0.2) ? green_circle : blue_circle;

    x = arc_radius * sin(angle_rad) + CHORD_LEN / 2 + x_offset;
    y = arc_radius - arc_radius * cos(angle_rad) + Y_OFFSET;

    dest.x = x-SMALL_CIRCLE_RADIUS;
    dest.y = y-SMALL_CIRCLE_RADIUS;
    dest.w = 2*SMALL_CIRCLE_RADIUS;
    dest.h = 2*SMALL_CIRCLE_RADIUS;
    sdlx_render_texture(vert_circle_texture, NULL, &dest);

    // print tilt angle at both ends of the arc, and at arc center
    x = arc_radius * sin(-arc_span_rad/2) + CHORD_LEN / 2 + x_offset;
    y = arc_radius - arc_radius * cos(-arc_span_rad/2) + sdlx_char_height(FONT_SMALL);
    sdlx_render_printf_ex2(x, y, FONT_SMALL, COLOR_WHITE, FLAG_X_CTR, "%g", -arc_span_deg/2);

    x = arc_radius * sin(arc_span_rad/2) + CHORD_LEN / 2 + x_offset;
    y = arc_radius - arc_radius * cos(arc_span_rad/2) + sdlx_char_height(FONT_SMALL);
    sdlx_render_printf_ex2(x, y, FONT_SMALL, COLOR_WHITE, FLAG_X_CTR, "%g", arc_span_deg/2);

    x = arc_radius * sin(0) + CHORD_LEN / 2 + x_offset;
    y = arc_radius - arc_radius * cos(0) + sdlx_char_height(FONT_SMALL);
    sdlx_render_printf_ex2(x, y, FONT_SMALL, COLOR_WHITE, FLAG_X_CTR, "0");

    // add interval marks on the arc
    tick_delta_deg = (arc_span_deg <= 5  ? 1 :
                     (arc_span_deg <= 20 ? 5   
                                         : 10));
    for (tick_deg = 0; tick_deg <= arc_span_deg/2; tick_deg += tick_delta_deg) {
        double tick_rad = tick_deg * DEG_TO_RAD;

        x = arc_radius * sin(tick_rad) + CHORD_LEN / 2 + x_offset;
        y = arc_radius - arc_radius * cos(tick_rad);
        sdlx_render_point(x, y+Y_OFFSET, COLOR_PINK, MAX_POINT_SIZE);

        x = arc_radius * sin(-tick_rad) + CHORD_LEN / 2 + x_offset;
        y = arc_radius - arc_radius * cos(tick_rad);
        sdlx_render_point(x, y+Y_OFFSET, COLOR_PINK, MAX_POINT_SIZE);
    }

    // print the tilt angle at the center of the rendering texture
    y = VERT_TEXTURE_WH/4;
    sdlx_render_printf_ex2(VERT_TEXTURE_WH/2, y,
                           FONT_LARGE, COLOR_WHITE,
                           FLAG_X_CTR, "%0.1f", angle_deg);

    // if not calibrated then display 'uncalibrated' in RED
    y += 2 * sdlx_char_height(FONT_LARGE);
    if (cal[rotate_deg/90] == INVALID_NUMBER) {
        sdlx_render_printf_ex2(VERT_TEXTURE_WH/2, y,
                               FONT_NORMAL, COLOR_RED,
                               FLAG_XY_CTR, "uncalibrated");
    }

    // print the CALIBRATE event; 
    // this event will be registered below, after the render target is set to the display
    y += 2 * sdlx_char_height(FONT_LARGE);
    loc = sdlx_render_printf_ex2(
                VERT_TEXTURE_WH/2, VERT_TEXTURE_WH/2+1.5*sdlx_char_height(FONT_LARGE),
                FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_X_CTR, 
                "%s", "CALIBRATE");

    // set render target back to the display
    sdlx_set_render_target(NULL);

    // render the rendering texture to the display, centered and rotated
    x = 0;
    y = (sdlx_win_height - VERT_TEXTURE_WH) / 2;
    dest.x = x;
    dest.y = y;
    dest.w = VERT_TEXTURE_WH;
    dest.h = VERT_TEXTURE_WH;
    sdlx_render_texture_rotated(vert, NULL, &dest, rotate_deg, NULL, FLIP_NONE);

    // Register the CALIBRATE event.
    // This event was printed to the vert texture.
    // Since the vert texture is rendered rotated by angle rotate_deg,
    // the loc of the event needs to be adjusted to account for that rotation.
    if (rotate_deg == 0) {
        loc->y = y + loc->y;
    } else if (rotate_deg == 180) {
        loc->y = y + (VERT_TEXTURE_WH - (loc->y + loc->h));
    } else if (rotate_deg == 90) {
        tmp_loc = *loc;
        loc->x = VERT_TEXTURE_WH - (tmp_loc.y + tmp_loc.h);
        loc->y = y + tmp_loc.x;
        loc->w = tmp_loc.h;
        loc->h = tmp_loc.w;
    } else {  // rotate_deg == 270
        tmp_loc = *loc;
        loc->x = tmp_loc.y;
        loc->y = y + tmp_loc.x;
        loc->w = tmp_loc.h;
        loc->h = tmp_loc.w;
    }
    sdlx_register_event(loc, EVID_VERT_CALIBRATE);

    // register EVID_SHOW_README_FILE event
    if (rotate_deg == 0) {
        reg_event_show_readme_file();
    }

    // register control event to adjust the arc span and end-program
    sdlx_register_control_events(EVID_VERT_MINUS, "-",
                                 EVID_VERT_PLUS, "+",
                                 EVID_QUIT, "X");

    // present the display
    sdlx_display_present();

    // wait for event, with 10 ms timeout
    sdlx_get_event(10000, &event);

    // process events
    switch (event.event_id) {
    case EVID_VERT_CALIBRATE:
        rc = cal_query();
        if (rc == EVID_CAL_CANCEL) break;
        cal[rotate_deg/90] = (rc == EVID_CAL_SAVE ? angle_uncal_deg : INVALID_NUMBER);
        sprintf(cal_param_name, "cal_vertical_%d", rotate_deg);
        util_set_numeric_param(data_dir, cal_param_name, cal[rotate_deg/90]);
        break;
    case EVID_VERT_MINUS:
        arc_span_deg -= 5;
        if (arc_span_deg < 5) arc_span_deg = 5;
        arc_span_rad = arc_span_deg * DEG_TO_RAD;
        util_set_numeric_param(data_dir, "arc_span_deg", arc_span_deg);
        break;
    case EVID_VERT_PLUS:
        arc_span_deg += 5;
        if (arc_span_deg > 90) arc_span_deg = 90;
        arc_span_rad = arc_span_deg * DEG_TO_RAD;
        util_set_numeric_param(data_dir, "arc_span_deg", arc_span_deg);
        break;
    case EVID_SHOW_README_FILE:
        show_file(data_dir, "README");
        break;
    case EVID_QUIT:
        end_program = true;
        break;
    }
}
