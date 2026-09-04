#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include <sdlx.h>
#include <utils.h>
#include <svcs.h>

#include "svcs/Location/location.h"

#include "lib/lib.h"

//
// defines
//

#define EVID_SETTINGS  10
#define EVID_GOTO_TOP  11

#define SEC 1000000

#define Y_TOP_OF_DISPLAY 50

//
// variables
//

char *progname;
char *data_dir;
int   y_history_top_reset;
    
//
// prototypes
//

void settings(void);

// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
    int          rc, y;
    sdlx_event_t event;
    bool         done = false;
    loc_hist_t  *loc_hist;

    double       y_history_top;
    int          y_history_display_begin;
    int          y_history_display_end;

    time_t       time_now;
    time_t       time_last_get_loc_info = 0;
    bool         settings_changed = false;
    int          last_loc_hist_count = -1;

    char         loc_curr[MAX_SVC_REQ_DATA] = "Not Initialized";
    char        *loc_hist_lines[MAX_LOC_HIST];

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // map location history file
    // - create_if_needed = false
    // - created (return flag) = NULL
    loc_hist = util_map_file("svcs/Location", LOC_HIST_FILENAME, sizeof(loc_hist_t), false, NULL);
    if (loc_hist == NULL) {
        printf("E: %s failed to map %s\n", progname, LOC_HIST_FILENAME);
        return 1; 
    }

    // runtime loop
    while (!done) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // get current location, this is done when:
        // - its been 10 seconds since last get
        // - settings have changed, which could have loaded a new city/town database
        // - a new loc_hist entry added
        time_now = time(NULL);
        if (time_now - time_last_get_loc_info > 10 || 
            settings_changed ||
            loc_hist->count != last_loc_hist_count)
        {
            svc_req_t *req = svc_req_init(SVC_LOCATION_REQ_GET_LOC_DATA_STR, NULL, 0);
            rc = svc_make_req("Location", req, 5);
            if (rc != 0) {
                strcpy(loc_curr, "ERROR");
            } else {
                strncpy(loc_curr, req->data, MAX_SVC_REQ_DATA);
                loc_curr[MAX_SVC_REQ_DATA-1] = '\0';
            }
            time_last_get_loc_info = time_now;
            settings_changed = false;
            last_loc_hist_count = loc_hist->count;
        }

        // display current location
        // - display "Current"
        y = Y_TOP_OF_DISPLAY;
        sdlx_render_printf_ex2(sdlx_win_width/2, y, FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, "%s", "Current");
        y += sdlx_char_height_dflt;
        // - display the current location
        if (strncmp(loc_curr, "ERROR", 5) != 0) {
            sdlx_render_printf_ex1(0, y,
                                   FONT_NORMAL, COLOR_WHITE, 
                                   "%s", loc_curr);
        } else {
            sdlx_render_printf_ex1(0, y+sdlx_char_height_dflt, 
                                   FONT_NORMAL, COLOR_RED, 
                                   "%s", "Location miniSvc\nNot Responding");
        }
        y += 5.5 * sdlx_char_height_dflt;

        // display rectangle to separate the Current and History areas
        sdlx_render_fill_rect(0, y, sdlx_win_width, 10, COLOR_BLUE);
        y += 0.5 * sdlx_char_height_dflt;

        // display the location history
        // - display "History"
        sdlx_render_printf_ex2(sdlx_win_width/2, y, FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, "History");
        y += sdlx_char_height_dflt;
        // - init variables used to display and scroll the history display
        if (y_history_top_reset == 0) {
            y_history_top_reset     = y;
            y_history_top           = y_history_top_reset;
            y_history_display_begin = y_history_top_reset;
            y_history_display_end   = sdlx_win_height;
        }
        // - display the history, starting at most recent
        int count = loc_hist->count;
        for (int i = 0; i < count; i++) {
            loc_hist_lines[i] = loc_hist->loc[count-1-i].data_str;
        }
        sdlx_render_multiline_text(0, y_history_top, y_history_display_begin, y_history_display_end, 
                                   FONT_NORMAL, loc_hist_lines, NULL, loc_hist->count);

        // register EVID_SHOW_README_FILE event
        reg_event_show_readme_file();

        // register for events
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_SETTINGS, "stg",
                                     EVID_GOTO_TOP, "top",
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with 1 second timeout
        sdlx_get_event(1*SEC, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_QUIT:
            done = true;
            break;
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_SETTINGS:
            settings();
            settings_changed = true;
            y_history_top = y_history_top_reset;
            break;
        case EVID_GOTO_TOP:
            y_history_top = y_history_top_reset;
            break;
        case EVID_MOTION:
            y_history_top += event.u.motion.yrel;
            if (y_history_top >= y_history_display_begin) {
                y_history_top = y_history_display_begin;
            }
            break;
        }
    }

    // cleanup and end program
    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  SETTINGS  ----------------------------------------

#define EVID_DEL_COUNTRY      20  // through 24
#define EVID_ADD_COUNTRY      30
#define EVID_CLEAR_HISTORY    31
#define EVID_ENABLE_HISTORY   32
#define EVID_DISABLE_HISTORY  33

#define MAX_COUNTRIES 5

char countries[MAX_COUNTRIES][3];
int  max_countries;

void get_countries(void);

void settings(void)
{
    bool         done = false;
    sdlx_loc_t  *loc;
    sdlx_event_t event;
    int          rc;
    char         is_enabled;
    svc_req_t   *req;
    char         msg[21];
    long         msg_time = 0;
    double       row;
    double       row_init = 1;
    double       row_delta = 2.5;

    // query current state of the Location service;
    // if not enabled the Location service will not be updating the location history file
    req = svc_req_init(SVC_LOCATION_REQ_QUERY_ENABLED, NULL, 0);
    rc = svc_make_req("Location", req, 5);
    if (rc != 0) {
        sdlx_display_init(COLOR_BLACK, PORTRAIT);
        sdlx_render_printf_ex2(
            sdlx_win_width/2, sdlx_win_height/2, 
            FONT_NORMAL, COLOR_RED, FLAG_XY_CTR,
            "%s", "Location miniSvc\nNot Responding");
        sdlx_display_present();
        sleep(3);
        return;
    } 
    is_enabled = req->data[0];

    // get list of countries
    get_countries();

    while (!done) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // print in LIGHT_BLUE
        sdlx_print_set_default(FONT_NORMAL, COLOR_LIGHT_BLUE);

        // set starting row
        row = row_init;

        // register for events ...

        // - CLEAR_HISTORY
        loc = sdlx_render_printf(0, ROW2Y(row), "%s", "Clear History");
        sdlx_register_event(loc, EVID_CLEAR_HISTORY);
        row += row_delta;

        // - DISABLE/ENABLE_HISTORY
        if (is_enabled) {
            loc = sdlx_render_printf(0, ROW2Y(row), "%s", "History is Enabled");
            sdlx_register_event(loc, EVID_DISABLE_HISTORY);
        } else {
            loc = sdlx_render_printf(0, ROW2Y(row), "%s", "History is Disabled");
            sdlx_register_event(loc, EVID_ENABLE_HISTORY);
        }
        row += row_delta;

        // - ADD_COUNTRY
        if (max_countries < 5) {
            loc = sdlx_render_printf(0, ROW2Y(row), "%s", "Download Country");
            sdlx_register_event(loc, EVID_ADD_COUNTRY);
            row += row_delta;
        }

        // restore print color to WHITE
        sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);

        // display list of countries, with DEL event for each
        for (int i = 0; i < max_countries; i++) {
            sdlx_render_printf(0, ROW2Y(row), "%s", countries[i]);

            sdlx_print_set_default(FONT_NORMAL, COLOR_LIGHT_BLUE);
            loc = sdlx_render_printf(COL2X(10), ROW2Y(row), "%s", "DEL");
            sdlx_register_event(loc, EVID_DEL_COUNTRY+i);
            sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);

            row += row_delta;
        }

        // display message for 3 seconds
        if (util_microsec_timer() < msg_time+3000000) {
            sdlx_color_t color = (strcmp(msg, "okay") == 0 ? COLOR_GREEN : COLOR_RED);
            sdlx_render_printf_ex1(0, sdlx_win_height-sdlx_char_height_dflt, FONT_NORMAL, color, "%s", msg);
        }

        // register for quit event
        sdlx_register_control_events(0, NULL, 0, NULL, EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, 100 ms timeout
        sdlx_get_event(100000, &event);
        if (event.event_id == -1) {
            continue;
        }

        // clear completion message
        msg[0] = '\0';
        msg_time = 0;

        // process events
        switch (event.event_id) {
        case EVID_QUIT:
            done = true;
            break;

        case EVID_ADD_COUNTRY: {
            char *country_code;

            country_code = sdlx_get_input_str("2 Char Country Code", false, NULL);
            if (strlen(country_code) != 2) {
                snprintf(msg, sizeof(msg), "cc must be 2 chars");
                msg_time = util_microsec_timer();
                break;
            }

            printf("I %s: adding country_code '%s'\n", progname, country_code);
            req = svc_req_init(SVC_LOCATION_REQ_ADD_COUNTRY_INFO, country_code, 2);
            rc = svc_make_req("Location", req, 20);
            if (rc != 0) {
                snprintf(msg, sizeof(msg), "add %s failed", country_code);
                msg_time = util_microsec_timer();
                break;
            }

            get_countries();

            snprintf(msg, sizeof(msg), "okay");
            msg_time = util_microsec_timer();
            break; }

        case EVID_DEL_COUNTRY+0:
        case EVID_DEL_COUNTRY+1:
        case EVID_DEL_COUNTRY+2:
        case EVID_DEL_COUNTRY+3:
        case EVID_DEL_COUNTRY+4: {
            int idx = event.event_id - EVID_DEL_COUNTRY;
            char prompt[50], *yn;

            sprintf(prompt, "Delete %s (y/n)", countries[idx]);
            yn = sdlx_get_input_str(prompt, false, "y");
            if (yn[0] != 'y' && yn[0] != 'Y') {
                snprintf(msg, sizeof(msg), "cancelled");
                msg_time = util_microsec_timer();
                break;
            }

            printf("I %s: deleteing %s\n", progname, countries[idx]);
            req = svc_req_init(SVC_LOCATION_REQ_DEL_COUNTRY_INFO, countries[idx], 2);
            rc = svc_make_req("Location", req, 5);
            if (rc != 0) {
                snprintf(msg, sizeof(msg), "delete %s failed", countries[idx]);
                msg_time = util_microsec_timer();
                break;
            }

            get_countries();

            snprintf(msg, sizeof(msg), "okay");
            msg_time = util_microsec_timer();
            break; }

        case EVID_CLEAR_HISTORY: {
            char *yn;

            yn = sdlx_get_input_str("Clear History (y/n)", false, "y");
            if (yn[0] != 'y' && yn[0] != 'Y') {
                snprintf(msg, sizeof(msg), "cancelled");
                msg_time = util_microsec_timer();
                break;
            }

            printf("I %s: clearing history\n", progname);
            req = svc_req_init(SVC_LOCATION_REQ_CLEAR_HISTORY, NULL, 0);
            rc = svc_make_req("Location", req, 5);
            if (rc != 0) {
                snprintf(msg, sizeof(msg), "clear history failed");
                msg_time = util_microsec_timer();
                break;
            }

            snprintf(msg, sizeof(msg), "okay");
            msg_time = util_microsec_timer();
            break; }

        case EVID_ENABLE_HISTORY: 
        case EVID_DISABLE_HISTORY: {
            char enable = (event.event_id == EVID_ENABLE_HISTORY);

            req = svc_req_init(SVC_LOCATION_REQ_SET_ENABLED, &enable, sizeof(enable));
            rc = svc_make_req("Location", req, 5);
            if (rc != 0) {
                snprintf(msg, sizeof(msg), "%s hist failed", enable ? "enable" : "disable");
                msg_time = util_microsec_timer();
                break;
            }

            is_enabled = enable;

            snprintf(msg, sizeof(msg), "okay");
            msg_time = util_microsec_timer();
            break; }
        }
    }
}

void get_countries(void)
{
    char      *p, *p1;
    int        rc;
    svc_req_t *req;

    memset(countries, 0, sizeof(countries));
    max_countries = 0;

    req = svc_req_init(SVC_LOCATION_REQ_LIST_COUNTRY_INFO, NULL, 0);
    rc = svc_make_req("Location", req, 5);     
    if (rc != 0) {
        printf("E %s: SVC_LOCATION_REQ_LIST_COUNTRY_INFO failed, rc=%d\n", progname, rc);
        return;
    }

    p = req->data;
    while (true) {
        p1 = strchr(p, '\n');
        if (p1 == NULL) {
            break;
        }

        *p1 = '\0';
        snprintf(countries[max_countries], sizeof(countries[max_countries]), "%s", p);
        max_countries++;
        p = p1 + 1;

        if (max_countries == MAX_COUNTRIES) {
            break;
        }
    }
}
