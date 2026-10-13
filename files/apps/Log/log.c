#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"

//
// defines
//

#define EVID_RELOAD       1
#define EVID_SAVE         2
#define EVID_FONT_SELECT  3

#define MAX_LINES 10000

#define FONT_SMALLEST  40
#define FONT_LARGEST   25

//
// variables
//

char         *progname;
char         *data_dir;

char         *lines[MAX_LINES];
sdlx_color_t  colors[MAX_LINES];
int           num_lines;

double        x;
double        y;
int           y_top;
int           y_bottom;

int           orientation;
int           fontid;

//
// prototypes
//

void reload(void);
void cleanup(void);
void init_xy_to_end_of_log(void);
void save(void);

// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
    sdlx_event_t event;
    bool         done = false;
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
    y_bottom         = sdlx_win_height - 1.5 * sdlx_char_height_dflt;
    y                = y_top;
    x                = 0;
    orientation      = get_device_orientation();
    last_orientation = orientation;

    // init fontid
    fontid = util_get_numeric_param(data_dir, "fontid", FONT_SMALLEST);

    // load the current log; this inits variables: lines, colors, and num_lines
    reload();

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

        // display the log; 
        // num_lines should be zero only when something went wrong with reload
        if (num_lines == 0) {
            sdlx_render_printf_ex(sdlx_win_width/2, sdlx_win_height/2, 
                                   FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
                                   "Load Failed");
        } else {
            sdlx_render_multiline_text(x, y, y_top, y_bottom, fontid, lines, colors, num_lines);
        }

        // register events
        sdlx_register_event(NULL, EVID_MOTION);
        reg_event_str(0, sdlx_win_height-1.25*ROW2Y(1), COLOR_LIGHT_BLUE, "SAVE", EVID_SAVE);
        reg_event_show_readme_file();
        sdlx_register_control_events(EVID_FONT_SELECT, "FONT",  
                                     EVID_RELOAD,      "RELOAD",
                                     EVID_QUIT,        "X");

        // present the display
        sdlx_display_present();

        // wait for event, with infinite timeout
        sdlx_get_event(-1, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_QUIT:
            done = true;
            break;
        case EVID_RELOAD:
            reload();
            break;
        case EVID_SAVE:
            save();
            break;
        case EVID_FONT_SELECT:
            fontid = (fontid > FONT_LARGEST ? fontid-5 : FONT_SMALLEST);
            util_set_numeric_param(data_dir, "fontid", fontid);
            init_xy_to_end_of_log();
            break;
        case EVID_MOTION: {
            double xrel = event.u.motion.xrel;
            double yrel = event.u.motion.yrel;

            if (fabs(xrel) > fabs(yrel)*1.5) x += xrel;
            if (fabs(yrel) > fabs(xrel)*1.5) y += yrel;

            if (y >= y_top) y = y_top;
            if (x > 0) x = 0;
            break; }
        }
    }

    // cleanup and end program
    cleanup();
    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  SUPPORT  ------------------------------------------

void reload(void)
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
    cleanup();

    // display loading message
    sdlx_display_init(COLOR_BLACK, orientation);
    sdlx_render_printf_ex(sdlx_win_width/2, sdlx_win_height/2, 
                           FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
                           "Loading");
    sdlx_display_present();

    // start logcat to capture log msgs from EZAPP, SDL, SDL/APP, AndroidRuntime, and all Fatal msgs
#ifdef ANDROID
    sprintf(cmd, "logcat -s -d --format=time EZAPP:I SDL:I SDL/APP:I AndroidRuntime:I *:F | tail -n %d", MAX_LINES);
#else
    // to test when ezApp is run on Linux
    sprintf(cmd, "cat apps/Log/test.log | tail -n %d", MAX_LINES);
#endif
    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("E %s: popen failed, %s\n", progname, strerror(errno));
        return;
    }

    // read log lines, initialize array of lines and parallel array of colors
    while (fgets(s, sizeof(s), fp) != NULL) {
        if (s[19] == 'I' && (s[20] == ' ' || s[20] == '/')) {
            colors[num_lines] = COLOR_GREEN;
        } else if (((s[19] == 'E' || s[19] == 'F') && (s[20] == ' ' || s[20] == '/')) ||
                   (strcasestr(s, "beginning of crash") != NULL) ||
                   (strcasestr(s, "error") != NULL) ||
                   (strcasestr(s, "fail") != NULL) ||
                   (strcasestr(s, "fatal") != NULL))
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

    // init display view to end of log
    init_xy_to_end_of_log();
}

void cleanup(void)
{
    for (int i = 0; i < num_lines; i++) {
        free(lines[i]);
        lines[i] = NULL;
    }
    num_lines = 0;
}

void init_xy_to_end_of_log(void)
{
    int num_last_lines_to_display;

    // set x to 0
    x = 0;

    // set y to display last lines of the log
    num_last_lines_to_display = (y_bottom - y_top) / sdlx_char_height(fontid);
    if (num_lines > num_last_lines_to_display) {
        y = y_top - (num_lines - num_last_lines_to_display) * sdlx_char_height(fontid);
    } else {
        y = y_top;
    }
}

void save(void)
{
    FILE *fp;
    char date_str[50], pathname[100];
    time_t t;
    struct tm *tm;
    int i;

    // if there are no log available then return
    if (num_lines == 0) {
        return;
    }

    // construct the pathname to save the log msgs to
    t = time(NULL);
    tm = localtime(&t);
    strftime(date_str, sizeof(date_str), "%y%m%d_%H%M%S", tm);
    sprintf(pathname, "%s/logs/%s.log", data_dir, date_str);
    printf("I %s: saving log to pathname = %s\n", progname, pathname);

    // display message for 1 sec
    sdlx_display_init(COLOR_BLACK, orientation);
    sdlx_render_printf(0, sdlx_win_height/2, "Creating\n%s.log", date_str);
    sdlx_display_present();
    sleep(1);
    
    // write the log lines to pathname
    fp = fopen(pathname, "w");
    for (i = 0; i < num_lines; i++) {
        fprintf(fp, "%s", lines[i]);
    }
    fclose(fp);
}

