#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include <sdlx.h>
#include <utils.h>

#include "svcs/Altitude/altitude.h"
#include "lib/lib.h"

// defines
#define ONE_SEC         1000000
#define DO_NOT_CREATE   false

#define EVID_PRIOR      1
#define EVID_NEXT       2
#define EVID_INCR_MAX_Y 3
#define EVID_DECR_MAX_Y 4

#define GRAPH_Y 600
#define GRAPH_H 1200

#define DEFAULT_MAX_Y  1000

// variables
char *progname;
char *data_dir;
bool  end_program;

altitude_file_t *altitude_file;
int              year, month, day;
int              param_max_y;
    
// prototypes
void draw_display(void);
void process_event(sdlx_event_t *event);

// -----------------  MAIN  ------------------------------------------

int initialize(void);
void cleanup(void);

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

        // wait for event, with 1 sec timeout
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
    // get the current year, month, and day
    get_current_ymd(&year, &month, &day);
    printf("I %s: initial year=%d month=%d day=%d\n", progname, year, month, day);

    // sanity check year value
    if (year < YEAR0 || year-YEAR0+1 > MAX_YEAR) {
        printf("E %s: year %d is out of range\n", progname, year);
        return -1;
    }

    // map altitude.dat file
    altitude_file = util_map_file("svcs/Altitude", ALTITUDE_FILENAME, sizeof(altitude_file_t),
                               DO_NOT_CREATE, NULL);
    if (altitude_file == NULL) {
        printf("E %s: failed to map %s\n", progname, ALTITUDE_FILENAME);
        return -1;
    }

    // validate altitude.dat version
    if (altitude_file->version != ALTITUDE_FILE_VERSION) {
        printf("E %s: unsupported altitude.dat version=%lx, expected=%lx\n",
               progname, altitude_file->version, ALTITUDE_FILE_VERSION);
        return -1;
    }

    // read max_y param value
    param_max_y  = util_get_numeric_param(data_dir, "max_y",  DEFAULT_MAX_Y);

    // success
    return 0;
}

void cleanup(void)
{
    if (altitude_file) {
        util_unmap_file(altitude_file, sizeof(altitude_file_t));
        altitude_file = NULL;
    }
}

// -----------------  DRAW DISPLAY  ------------------------------------

void draw_display(void)
{
    double       altitude_ft;
    bool         alt_is_wgs84;
    double       alt_ft[24];
    sdlx_color_t colors[24], color;

    // display current altitude
    util_get_location(NULL, NULL, &altitude_ft, &alt_is_wgs84);
    sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(1), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR,
                           "Current");
    if (altitude_ft == INVALID_NUMBER) {
        sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(2), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR,
                               "Alt = Unavailable");
    } else {
        sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(2), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR,
                               "Alt = %0.0f ft %s",
                               altitude_ft,
                               alt_is_wgs84 ? "WGS84" : "MSL");
    }

    // display horizontal bar to divide the current altitude display section from the graph section
    sdlx_render_fill_rect(sdlx_win_width/4, ROW2Y(4.25), sdlx_win_width/2, 10, COLOR_BLUE);

    // display graph title line (the graph date)
    color = (is_today(year, month, day) ? COLOR_GREEN : COLOR_WHITE);
    sdlx_render_printf_ex2(sdlx_win_width/2, GRAPH_Y-ROW2Y(1.5), 
                           FONT_NORMAL, color, FLAG_X_CTR,
                           "%s", ymd_to_str(year, month, day));

    // display the bar graph of altitude for year,month,day
    color = is_weekend(year,month,day) ? COLOR_BLUE : COLOR_GREEN;
    for (int hour = 0; hour < 24; hour++) {
        alt_ft[hour] = altitude_file->altitude_ft[year-YEAR0][month-1][day-1][hour];
        colors[hour] = color;
    }
    display_bar_graph(0, GRAPH_Y, sdlx_win_width, GRAPH_H,
                      alt_ft, colors, 24, param_max_y,
                      "00 02 04 06 08 10 12 14 16 18 20 22 ");

    // register events to increase or decrease y axis
    int y = GRAPH_Y+GRAPH_H+ROW2Y(2);
    sdlx_render_printf_ex2(sdlx_win_width/2, y,
                           FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR,
                           "%d", param_max_y);
    reg_event_str(sdlx_win_width/2-COL2X(4.5), y, COLOR_LIGHT_BLUE, "-", EVID_DECR_MAX_Y);
    reg_event_str(sdlx_win_width/2+COL2X(3.5), y, COLOR_LIGHT_BLUE, "+", EVID_INCR_MAX_Y);

    // register EVID_SHOW_README_FILE event
    reg_event_show_readme_file();

    // register control events to goto the previous or next days, and to end program
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
    case EVID_INCR_MAX_Y:
        bar_graph_increase_y_axis(&param_max_y);
        util_set_numeric_param(data_dir, "max_y", param_max_y);
        break;
    case EVID_DECR_MAX_Y:
        bar_graph_decrease_y_axis(&param_max_y);
        util_set_numeric_param(data_dir, "max_y", param_max_y);
        break;
    case EVID_PRIOR:
        set_ymd_to_prior(&year, &month, &day);
        if (year < YEAR0) {
            year = YEAR0;
            month = 1;
            day = 1;
        }
        break;
    case EVID_NEXT:
        set_ymd_to_next(&year, &month, &day);
        get_current_ymd(&y_cur, &m_cur, &d_cur);
        if (year*10000 + month*100 + day > y_cur*10000 + m_cur*100 + d_cur) {
            year = y_cur;
            month = m_cur;
            day = d_cur;
        }
        break;
    }
}
