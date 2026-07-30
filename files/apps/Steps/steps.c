#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include <sdlx.h>
#include <utils.h>

#include "svcs/Steps/steps.h"
#include "apps/lib/lib.h"

// defines
#define ONE_SEC         1000000
#define DO_NOT_CREATE   false
#define INCHES_PER_MILE (5280 * 12)

#define EVID_VIEW_SELECT  1
#define EVID_PRIOR        2
#define EVID_NEXT         3
#define EVID_TODAY        4
#define EVID_SETTINGS     5

#define VIEW_DAY    0
#define VIEW_MONTH  1
#define VIEW_YEAR   2
#define VIEW_STR (view == VIEW_DAY ? "DAY" : (view == VIEW_MONTH ? "MONTH" : "YEAR"))

#define GRAPH_Y 600
#define GRAPH_H 900 

#define DFLT_MAX_MILES_PER_HOUR   3     // must be a valid_max_value
#define DFLT_MAX_MILES_PER_DAY    10    // must be a valid_max_value
#define DFLT_MAX_MILES_PER_MONTH  50    // must be a valid_max_value
#define DFLT_STEP_LEN_INCHES      30.0

// typedefs
typedef struct {
    int    max_miles_per_hour;
    int    max_miles_per_day;
    int    max_miles_per_month;
    double step_len;
} params_t;

// variables
char *progname;
char *data_dir;
bool  end_program;

steps_file_t *steps_file;
int           view;
int           year;
int           month;
int           day;
params_t      params;
    
// prototypes
int initialize(void);
void cleanup(void);
void draw_display(void);
void process_event(sdlx_event_t *event);
void settings(void);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    sdlx_event_t event;
    int rc;

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // initialize
    rc = initialize();
    if (rc != 0) {
        printf("E %s: initialize failed\n", progname);
        return 1;
    }

    // runtime loop
    while (!end_program) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // draw display
        draw_display();

        // present the display
        sdlx_display_present();

        // wait for event, with timeout
        sdlx_get_event(ONE_SEC, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        process_event(&event);
    }

    // cleanup
    cleanup();

    // end program
    printf("I %s: terminating\n", progname);
    return 0;
}

int initialize(void)
{
    // initialize the view to the current month
    view = VIEW_DAY;
    get_current_ymd(&year, &month, &day);

    // map steps.dat
    steps_file = util_map_file("svcs/Steps", STEPS_FILENAME, sizeof(steps_file_t), DO_NOT_CREATE, NULL);
    if (steps_file == NULL) {
        printf("E %s: failed to map %s\n", progname, STEPS_FILENAME);
        return -1;
    }

    // validate steps.dat version
    if (steps_file->version != STEPS_FILE_VERSION) {
        printf("E %s: unsupported steps.dat version=%lx, expected=%lx\n",
               progname, steps_file->version, STEPS_FILE_VERSION);
        return -1;
    }

    // read params
    params.step_len            = util_get_numeric_param(data_dir, "step_len", DFLT_STEP_LEN_INCHES);
    params.max_miles_per_hour  = util_get_numeric_param(data_dir, "max_miles_per_hour",  DFLT_MAX_MILES_PER_HOUR);
    params.max_miles_per_day   = util_get_numeric_param(data_dir, "max_miles_per_day",   DFLT_MAX_MILES_PER_DAY);
    params.max_miles_per_month = util_get_numeric_param(data_dir, "max_miles_per_month", DFLT_MAX_MILES_PER_MONTH);

    // success
    return 0;
}

void cleanup(void)
{
    if (steps_file) {
        util_unmap_file(steps_file, sizeof(steps_file_t));
        steps_file = NULL;
    }
}

// -----------------  DRAW DISPLAY  ------------------------------------

void draw_display(void)
{
    unsigned int steps;
    double miles;
    sdlx_loc_t *loc;
    sdlx_color_t color;
    int curr_y, curr_m, curr_d;

    // display title line, based on the current view selection
    get_current_ymd(&curr_y, &curr_m, &curr_d);
    if (view == VIEW_DAY) {
        color = (year == curr_y && month == curr_m && day == curr_d) ? COLOR_GREEN : COLOR_WHITE;
        sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(1), FONT_NORMAL, color, FLAG_X_CTR, 
                               "%s", ymd_to_str(year, month, day));
    } else if (view == VIEW_MONTH) {
        color = (year == curr_y && month == curr_m) ? COLOR_GREEN : COLOR_WHITE;
        sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(1), FONT_NORMAL, color, FLAG_X_CTR, 
                               "%s %d", get_month_str(month), year);
    } else { // year
        color = (year == curr_y) ? COLOR_GREEN : COLOR_WHITE;
        sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(1), FONT_NORMAL, color, FLAG_X_CTR, 
                               "%d", year);
    }

    // display steps and miles
    steps = (view == VIEW_DAY   ? steps_file->day[year-YEAR0][month-1][day-1] :
            (view == VIEW_MONTH ? steps_file->month[year-YEAR0][month-1] :
                                  steps_file->year[year-YEAR0]));
    miles = steps * params.step_len / INCHES_PER_MILE;

    sdlx_render_printf_ex2(COL2X(5), ROW2Y(3), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, "%d", steps);
    sdlx_render_printf_ex2(COL2X(5), ROW2Y(4), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, "Steps");
    sdlx_render_printf_ex2(COL2X(14), ROW2Y(3), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, "%0.2f", miles);
    sdlx_render_printf_ex2(COL2X(14), ROW2Y(4), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, "Miles");

    // display graph
    double       values[32];
    sdlx_color_t colors[32];
    int          max_values;
    int          max_y;
    char        *x_axis;
    double       cvt_steps_to_miles;

    cvt_steps_to_miles = params.step_len / INCHES_PER_MILE;
    if (view == VIEW_DAY) {
        max_values = 24;
        for (int i = 0; i < max_values; i++) {
            values[i] = steps_file->hour[year-YEAR0][month-1][day-1][i] * cvt_steps_to_miles;
            colors[i] = is_weekend(year, month, day) ? COLOR_BLUE : COLOR_GREEN;
        }
        max_y = params.max_miles_per_hour;
        x_axis = "00 02 04 06 08 10 12 14 16 18 20 22 ";
    } else if (view == VIEW_MONTH) {
        max_values = 31;
        for (int i = 0; i < max_values; i++) {
            values[i] = steps_file->day[year-YEAR0][month-1][i] * cvt_steps_to_miles;
            colors[i] = is_weekend(year, month, i+1) ? COLOR_BLUE : COLOR_GREEN;
        }
        max_y = params.max_miles_per_day;
        x_axis = "01 03 05 07 09 11 13 15 17 19 21 23 25 27 29 31";
    } else { // year
        max_values = 12;
        for (int i = 0; i < max_values; i++) {
            values[i] = steps_file->month[year-YEAR0][i] * cvt_steps_to_miles;
            colors[i] = COLOR_GREEN;
        }
        max_y = params.max_miles_per_month;
        x_axis = "J F M A M J J A S O N D";
    }
    display_bar_graph(0, GRAPH_Y, sdlx_win_width, GRAPH_H,
                      values, colors, max_values, max_y, x_axis);

    // register events:
    // - EVID_VIEW_SELECT: used to choose DAY, MONTH, or YEAR view
    // - EVID_TODAY:       set year, month, day to today
    // - EVID_SETTINGS:    bring up settings display
    loc = sdlx_render_printf_ex1(0, sdlx_win_height-2*sdlx_char_height_dflt, 
                                 FONT_NORMAL, COLOR_LIGHT_BLUE, "%s", VIEW_STR);
    sdlx_register_event(loc, EVID_VIEW_SELECT);

    loc = sdlx_render_printf_ex1(COL2X(8), sdlx_win_height-2*sdlx_char_height_dflt, 
                                 FONT_NORMAL, COLOR_LIGHT_BLUE, "TODAY");
    sdlx_register_event(loc, EVID_TODAY);

    loc = sdlx_render_printf_ex1(COL2X(17), sdlx_win_height-2*sdlx_char_height_dflt, 
                                 FONT_NORMAL, COLOR_LIGHT_BLUE, "STG");
    sdlx_register_event(loc, EVID_SETTINGS);

    // register EVID_SHOW_README_FILE event
    reg_event_show_readme_file();

    // register control event
    sdlx_register_control_events(EVID_PRIOR, "<",
                                 EVID_NEXT, ">",
                                 EVID_QUIT, "X");
}

// -----------------  PROCESS EVENT  ---------------------------

void process_event(sdlx_event_t *event)
{
    int y_cur,m_cur,d_cur;

    switch (event->event_id) {
    case EVID_QUIT:
        end_program = true;
        break;

    case EVID_SHOW_README_FILE:
        show_file(data_dir, "README");
        break;

    case EVID_VIEW_SELECT:
        view = (view + 1) % 3;
        break;

    case EVID_TODAY:
        view = VIEW_DAY;
        get_current_ymd(&year, &month, &day);
        break;

    case EVID_PRIOR:
        if (view == VIEW_DAY) {
            set_ymd_to_prior(&year, &month, &day);
        } else if (view == VIEW_MONTH) {
            if (--month < 1) {
                month = 12;
                year--;
            }
        } else {  // year
            year--;
        }
        if (year < YEAR0) {
            year = YEAR0;
            month = 1;
            day = 1;
        }
        break;

    case EVID_NEXT:
        if (view == VIEW_DAY) {
            set_ymd_to_next(&year, &month, &day);
        } else if (view == VIEW_MONTH) {
            if (++month > 12) {
                month = 1;
                year++;
            }
        } else {  // year
            year++;
        }
        get_current_ymd(&y_cur, &m_cur, &d_cur);
        if (year*10000 + month*100 + day > y_cur*10000 + m_cur*100 + d_cur) {
            year = y_cur;
            month = m_cur;
            day = d_cur;
        }
        break;

    case EVID_SETTINGS:
        settings();
        break;
    }
}

// -----------------  SETTINGS  -----------------------------------

#define EVID_STEP_LEN     1
#define EVID_MAX_MPH_DECR 2
#define EVID_MAX_MPH_INCR 3
#define EVID_MAX_MPD_DECR 4
#define EVID_MAX_MPD_INCR 5
#define EVID_MAX_MPM_DECR 6
#define EVID_MAX_MPM_INCR 7

void settings(void)
{
    bool         done = false;
    sdlx_event_t event;
    int          y;
    double       value;
    char        *str;
    sdlx_loc_t  *loc;

    while (!done) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // display title line
        sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(1), 
                               FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
                               "SETTINGS");

        // register event to change params.step_len
        y = 3*sdlx_char_height_dflt;
        loc = sdlx_render_printf_ex1(0, y, FONT_NORMAL, COLOR_LIGHT_BLUE, "step_len = %g", params.step_len);
        sdlx_register_event(loc, EVID_STEP_LEN);
        y += 2*sdlx_char_height_dflt;

        // register events to change graph max y values
        sdlx_render_printf(0, y, "max_graph miles per:");
        y += 2*sdlx_char_height_dflt;

        // - params.max_miles_per_hour
        sdlx_render_printf(0, y, "hour  = %d", params.max_miles_per_hour);
        loc = sdlx_render_printf_ex1(COL2X(13), y, FONT_NORMAL, COLOR_LIGHT_BLUE, "-");
        sdlx_register_event(loc, EVID_MAX_MPH_DECR);
        loc = sdlx_render_printf_ex1(COL2X(17), y, FONT_NORMAL, COLOR_LIGHT_BLUE, "+");
        sdlx_register_event(loc, EVID_MAX_MPH_INCR);
        y += 2*sdlx_char_height_dflt;

        // - params.max_miles_per_day
        sdlx_render_printf(0, y, "day   = %d", params.max_miles_per_day);
        loc = sdlx_render_printf_ex1(COL2X(13), y, FONT_NORMAL, COLOR_LIGHT_BLUE, "-");
        sdlx_register_event(loc, EVID_MAX_MPD_DECR);
        loc = sdlx_render_printf_ex1(COL2X(17), y, FONT_NORMAL, COLOR_LIGHT_BLUE, "+");
        sdlx_register_event(loc, EVID_MAX_MPD_INCR);
        y += 2*sdlx_char_height_dflt;

        // - params.max_miles_per_month
        sdlx_render_printf(0, y, "month = %d", params.max_miles_per_month);
        loc = sdlx_render_printf_ex1(COL2X(13), y, FONT_NORMAL, COLOR_LIGHT_BLUE, "-");
        sdlx_register_event(loc, EVID_MAX_MPM_DECR);
        loc = sdlx_render_printf_ex1(COL2X(17), y, FONT_NORMAL, COLOR_LIGHT_BLUE, "+");
        sdlx_register_event(loc, EVID_MAX_MPM_INCR);
        y += 2*sdlx_char_height_dflt;

        // register control event to exit settings display
        sdlx_register_control_events(0, NULL, 0, NULL, EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, infinite timeout
        sdlx_get_event(-1, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_STEP_LEN:
            str = sdlx_get_input_str("step_len", true, NULL);
            if (sscanf(str, "%lf", &value) == 1) {
                params.step_len = value;
                util_set_numeric_param(data_dir, "step_len", params.step_len);
            }
            break;

        case EVID_MAX_MPH_DECR:
            bar_graph_decrease_y_axis(&params.max_miles_per_hour);
            util_set_numeric_param(data_dir, "max_miles_per_hour", params.max_miles_per_hour);
            break;
        case EVID_MAX_MPH_INCR:
            bar_graph_increase_y_axis(&params.max_miles_per_hour);
            util_set_numeric_param(data_dir, "max_miles_per_hour", params.max_miles_per_hour);
            break;

        case EVID_MAX_MPD_DECR:
            bar_graph_decrease_y_axis(&params.max_miles_per_day);
            util_set_numeric_param(data_dir, "max_miles_per_day", params.max_miles_per_day);
            break;
        case EVID_MAX_MPD_INCR:
            bar_graph_increase_y_axis(&params.max_miles_per_day);
            util_set_numeric_param(data_dir, "max_miles_per_day", params.max_miles_per_day);
            break;

        case EVID_MAX_MPM_DECR:
            bar_graph_decrease_y_axis(&params.max_miles_per_month);
            util_set_numeric_param(data_dir, "max_miles_per_month", params.max_miles_per_month);
            break;
        case EVID_MAX_MPM_INCR:
            bar_graph_increase_y_axis(&params.max_miles_per_month);
            util_set_numeric_param(data_dir, "max_miles_per_month", params.max_miles_per_month);
            break;

        case EVID_QUIT:
            done = true;
            break;
        }
    }
}
