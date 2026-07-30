#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/lib/lib.h"

//
// defines
//

#define EVID_RELOAD       1
#define EVID_END          2
#define EVID_FONT_SELECT  3

#define LOG_NOT_LOADED  0
#define LOG_LOADED      1
#define LOG_LOAD_FAILED 2

#define MAX_LINES 500

#define FONT_SMALLEST  40
#define FONT_LARGEST   25

//
// variables
//

char         *progname;
char         *data_dir;

int           state = LOG_NOT_LOADED;

char         *lines[MAX_LINES];
sdlx_color_t  colors[MAX_LINES];
int           num_lines;

double        x;
double        y;
int           y_top;
int           y_bottom;

int           fontid;

//
// prototypes
//

void init_xy_to_end_of_log(void);
int log_load(void);
void log_cleanup(void);

// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
    int          rc;
    sdlx_event_t event;
    bool         done = false;
    int          orientation;
    int          last_orientation;

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // init display location of captured log prints
    y_top            = 0;
    y_bottom         = sdlx_win_height;
    y                = y_top;
    x                = 0;
    orientation      = PORTRAIT;
    last_orientation = PORTRAIT;

    // init fontid
    fontid = util_get_numeric_param(data_dir, "fontid", FONT_SMALLEST);

    // runtime loop
    while (!done) {
        // init the backbuffer
        orientation = get_device_orientation();
        sdlx_display_init(COLOR_BLACK, orientation);

        // if orientation has changed then
        // set x,y to display end of log
        if (orientation != last_orientation) {
            y_bottom = sdlx_win_height;
            init_xy_to_end_of_log();
            last_orientation = orientation;
        }

        // if log has not been loaded then
        //   display message
        // else
        //   display log
        // endif
        if (state == LOG_NOT_LOADED || state == LOG_LOAD_FAILED) {
            sdlx_render_printf_ex2(sdlx_win_width/2, sdlx_win_height/2, 
                                   FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
                                   "%s",
                                   state == LOG_NOT_LOADED ?  "Loading" : "Load Failed");
        } else {
            sdlx_render_multiline_text(x, y, y_top, y_bottom, fontid, lines, colors, num_lines);
        }

        // register control events
        sdlx_register_control_events(EVID_RELOAD,      "RELOAD",
                                     EVID_FONT_SELECT, "FONT",  
                                     EVID_QUIT,        "X");
        sdlx_register_event(NULL, EVID_MOTION);

        // present the display
        sdlx_display_present();

        // if log is not loaded then load it
        if (state == LOG_NOT_LOADED) {
            rc = log_load();
            state = (rc == 0 ? LOG_LOADED : LOG_LOAD_FAILED);
            init_xy_to_end_of_log();
        }

        // wait for event, with 50 ms timeout
        sdlx_get_event(50000, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_QUIT:
            done = true;
            break;
        case EVID_RELOAD:
            state = LOG_NOT_LOADED;
            break;
        case EVID_END:
            init_xy_to_end_of_log();
            break;
        case EVID_FONT_SELECT:
            fontid = (fontid > FONT_LARGEST ? fontid-5 : FONT_SMALLEST);
            util_set_numeric_param(data_dir, "fontid", fontid);
            init_xy_to_end_of_log();
            break;
        case EVID_MOTION:
            if (state == LOG_LOADED) {
                double xrel = event.u.motion.xrel;
                double yrel = event.u.motion.yrel;

                if (fabs(xrel) > fabs(yrel)*1.5) x += xrel;
                if (fabs(yrel) > fabs(xrel)*1.5) y += yrel;

                if (y >= y_top) y = y_top;
                if (x > 0) x = 0;
            }
            break;
        }
    }

    // cleanup and end program
    log_cleanup();
    printf("I %s: terminating\n", progname);
    return 0;
}

void init_xy_to_end_of_log(void)
{
    int num_last_lines_to_display;

    // this routine should only be called when the log has been loaded
    if (state != LOG_LOADED) {
        x = 0;
        y = 0;
        return;
    }

    // set x to 0
    x = 0;

    // set y to display last lines of the log
    num_last_lines_to_display = (y_bottom - y_top) / sdlx_char_height(fontid) - 3;
    if (num_lines > num_last_lines_to_display) {
        y = y_top - (num_lines - num_last_lines_to_display) * sdlx_char_height(fontid);
    } else {
        y = y_top;
    }
}

int log_load(void)
{
    char s[1000];
    char cmd[200];
    FILE *fp;
    static int cnt;

    // NOTES: 
    // - set -o pipefail - option in Bash ensures that a pipeline's exit status
    //   reflects the failure of any command within it, rather than just the
    //   last command's status
    // - set -o pipefail - is not currently beins used, but if the pclose exit
    //   status were to be examined then the pipefail option would be needed when
    //   making the popen call

    printf("I %s: ==== LOADING LOG %d ====\n", progname, ++cnt);

    // free previously captured log lines,
    // this call resets num_lines to zero
    log_cleanup();

    // start logcat to capture last lines for EZAPP, SDL, SDL/APP and AndroidRuntime
#ifdef ANDROID
    sprintf(cmd, "logcat -s -d --format=time EZAPP:I SDL:I SDL/APP:I AndroidRuntime:I | tail -%d", MAX_LINES);
#else
    // - to test when ezApp is run on Linux
    sprintf(cmd, "cat apps/Log/test.log | tail -%d", MAX_LINES);
#endif
    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("E %s: popen failed, %s\n", progname, strerror(errno));
        return -1;
    }

    // read log lines, initialize array of lines and parallel array of colors
    while (fgets(s, sizeof(s), fp) != NULL) {
        if (s[19] == 'I' && (s[20] == ' ' || s[20] == '/')) {
            colors[num_lines] = COLOR_GREEN;
        } else if ((s[19] == 'E' && (s[20] == ' ' || s[20] == '/')) ||
                   (strcasestr(s, "error") != NULL) ||
                   (strcasestr(s, "fail") != NULL))
        {
            colors[num_lines] = COLOR_RED;
        } else {
            colors[num_lines] = COLOR_WHITE;
        }

        lines[num_lines] = strdup(s);

        num_lines++;
        if (num_lines == MAX_LINES) {
            break;
        }
    }

    // close pipe
    pclose(fp);

    // return success if there are 1 or more log lines captured
    return (num_lines > 0 ? 0 : -1);
}

void log_cleanup(void)
{
    for (int i = 0; i < num_lines; i++) {
        free(lines[i]);
        lines[i] = NULL;
    }
    num_lines = 0;
}

