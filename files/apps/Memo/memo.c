#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/lib/lib.h"

//
// defines
//

#define EVID_NEW      1
#define EVID_STOP     2
#define EVID_GOTO_TOP 3
#define EVID_PLAY     100
#define EVID_APPEND   200
#define EVID_DELETE   300

#define MAX_FILENAME 100

#define FONT_CUSTOM 23  // char size fits 23 chars over display width

#define BAR_AREA_HEIGHT 150
#define BAR_HEIGHT 75

#define AUTO_STOP_SECS 3

//
// variables
//

char *progname;
char *data_dir;

char *filename[MAX_FILENAME];
char *friendlyname[MAX_FILENAME];
int   max_filename;

//
// prototypes
//

void get_list_of_files(void);
void cleanup_filename_allocations(void);
void substring(char *s, int start, int len, char *substring);

// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
    sdlx_event_t       event;
    bool               end_program = false;
    int                y_top;
    int                y_bottom;
    double             y;
    int                y2;
    sdlx_audio_state_t audio_state;
    char               active_filename[100];

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // init active_filenam variable to empty string
    active_filename[0] = '\0';

    // init variables which define the vertical region of the display
    // being used for the filename list
    y_top    = 150;
    y_bottom = sdlx_win_height - BAR_AREA_HEIGHT;
    y        = y_top;

    // set default font
    sdlx_print_set_default(FONT_CUSTOM, COLOR_WHITE);

    // runtime loop
    while (!end_program) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // get list of audio files
        get_list_of_files();

        // get current audio state
        sdlx_audio_get_state(&audio_state);

        // if audio state has become idle then there is no active filename
        if (audio_state.state == AUDIO_STATE_IDLE) {
            active_filename[0] = '\0';
        }

        // display the audio filename, followed by events to append, or delete
        for (int idx = 0; idx < max_filename; idx++) {
            sdlx_loc_t  *loc;
            sdlx_color_t color;

            // if all filenames will fit on the display then disable scrolling
            if (max_filename <= 14) {
                y = y_top;
            }

            // if y display location is above the top of the display region,
            // or below the bottom of the display region, then either continue or break
            y2 = y + ROW2Y(2*idx);
            if (y2 < y_top - sdlx_char_height_dflt) continue;
            if (y2 > y_bottom) break;

            // determine the color in which the filename[idx] will be displayed
            // - GREEN:      playing
            // - RED:        recording
            // - LIGHT_BLUE: idle
            if (strcmp(active_filename, filename[idx]) == 0) {
                color = (audio_state.state == AUDIO_STATE_PLAY_FILE       ? COLOR_GREEN :
                        (audio_state.state == AUDIO_STATE_RECORD_FROM_MIC ? COLOR_RED :
                                                                            COLOR_LIGHT_BLUE));
            } else {
                color = COLOR_LIGHT_BLUE;
            }

            // display the friendly filename in the color determined above
            loc = sdlx_render_printf_ex1(0, y2, FONT_CUSTOM, color, "%s", friendlyname[idx]);
            if (color == COLOR_LIGHT_BLUE || color == COLOR_GREEN) {
                sdlx_register_event(loc, EVID_PLAY+idx);
            }

            // if color is light blue (idle) then
            //   register append and delete events
            // else  (must be either playing or recording)
            //   register stop event, to stop the inprogress playback or record
            // endif
            if (color == COLOR_LIGHT_BLUE) {
                loc = sdlx_render_printf_ex1(COL2X(16.5), y2, FONT_CUSTOM, COLOR_LIGHT_BLUE, "%s", "+");
                sdlx_register_event(loc, EVID_APPEND+idx);

                loc = sdlx_render_printf_ex1(COL2X(21), y2, FONT_CUSTOM, COLOR_LIGHT_BLUE, "%s", "X");
                sdlx_register_event(loc, EVID_DELETE+idx);
            } else {
                loc = sdlx_render_printf_ex1(COL2X(15.5), y2, FONT_CUSTOM, COLOR_LIGHT_BLUE, "%s", "STOP");
                sdlx_register_event(loc, EVID_STOP);
            }
        }

        // display bar which indicates:
        // - playback: the amount of the file that has played
        // - record: the record volume
        y2 = y_bottom;
        sdlx_render_fill_rect(0, y2, sdlx_win_width, BAR_AREA_HEIGHT, COLOR_BLACK);
        y2 += (BAR_AREA_HEIGHT - BAR_HEIGHT) / 2;
        if (audio_state.state == AUDIO_STATE_PLAY_FILE) {
            int bar_value_w = sdlx_win_width * audio_state.play_current_secs / audio_state.play_total_secs;
            sdlx_render_fill_rect(0, y2, bar_value_w, BAR_HEIGHT, COLOR_GREEN);
            sdlx_render_rect(0, y2, sdlx_win_width, BAR_HEIGHT, 2, COLOR_WHITE);
        } else if (audio_state.state == AUDIO_STATE_RECORD_FROM_MIC) {
            int bar_value_w =  sdlx_win_width * audio_state.volume;
            sdlx_render_printf(sdlx_win_width-COL2X(4), y2, "%4.2f", audio_state.volume);
            sdlx_render_fill_rect(0, y2, bar_value_w, BAR_HEIGHT, COLOR_RED);
            sdlx_render_rect(0, y2, sdlx_win_width, BAR_HEIGHT, 2, COLOR_WHITE);
        }

        // register EVID_SHOW_README_FILE event
        reg_event_show_readme_file();

        // register motion and control events
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_NEW, "+",
                                     EVID_GOTO_TOP, "TOP",
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with 100ms timeout
        sdlx_get_event(100000, &event);

        // process events
        if (event.event_id == -1) {
            // no event received, must be timeout
        } else if (event.event_id == EVID_MOTION) {
            y += event.u.motion.yrel;
            if (y >= y_top) {
                y = y_top;
            }
        } else if (event.event_id == EVID_GOTO_TOP) {
            y = y_top;
        } else if (event.event_id == EVID_NEW) {
            time_t t = time(NULL);
            struct tm tm;
            char new_filename[100];

            localtime_r(&t, &tm);
            sprintf(new_filename, "%04d%02d%02d%02d%02d%02d.mp3",
                    tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
            // append = false, start_paused = false
            sdlx_audio_record_from_mic(data_dir, new_filename, AUTO_STOP_SECS, false, false);
            strcpy(active_filename, new_filename);
        } else if (event.event_id == EVID_STOP) {
            sdlx_audio_stop();
        } else if (event.event_id >= EVID_PLAY && event.event_id < EVID_PLAY + max_filename) {
            int idx = event.event_id - EVID_PLAY;
            sdlx_audio_play_file(data_dir, filename[idx]);
            strcpy(active_filename, filename[idx]);
        } else if (event.event_id >= EVID_APPEND && event.event_id < EVID_APPEND + max_filename) {
            int idx = event.event_id - EVID_APPEND;
            // append = true, start_paused  = false
            sdlx_audio_record_from_mic(data_dir, filename[idx], AUTO_STOP_SECS, true, false);
            strcpy(active_filename, filename[idx]);
        } else if (event.event_id >= EVID_DELETE && event.event_id < EVID_DELETE + max_filename) {
            int idx = event.event_id - EVID_DELETE;
            util_delete_file(data_dir, filename[idx]);
        } else if (event.event_id == EVID_SHOW_README_FILE) {
            show_file(data_dir, "README");
        } else if (event.event_id == EVID_QUIT) {
            end_program = true;
        }
    }

    // cleanup and end program
    sdlx_audio_stop();
    cleanup_filename_allocations();
    printf("I %s: terminating\n", progname);
    return 0;
}

void get_list_of_files(void)
{
    FILE *fp;
    int   i;
    char  cmd[100], s[100];
    long  mtime;

    static long mtime_last;

    // return if no change
    mtime = util_file_mtime(data_dir, NULL);
    if (mtime == mtime_last) {
        return;
    }
    mtime_last = mtime;

    // debug print
    printf("I %s: updating filenames\n", progname);

    // cleanup current filename string allocations
    cleanup_filename_allocations();

    // run 'ls -lr' to get reverse sorted list of filenames,
    // starting with the most recent
    sprintf(cmd, "cd %s; /bin/ls -1r *.mp3", data_dir);
    fp = popen(cmd, "r");
    while (fgets(s, sizeof(s), fp)) {
        str_remove_trailing_newline(s);
        filename[max_filename++] = strdup(s);
    }
    pclose(fp);

    // create friendly filenames, for example:
    // - filename:    20251219071933.mp3
    // - friendlyname: Dec19-07:19
    for (i = 0; i < max_filename; i++) {
        char month[8], day[8], hour[8], minute[8], month_abbrev[16];
        char friendly[50];
        struct tm tm;

        substring(filename[i], 4, 2, month);
        substring(filename[i], 6, 2, day);
        substring(filename[i], 8, 2, hour);
        substring(filename[i], 10, 2, minute);
        memset(&tm, 0, sizeof(tm));
        tm.tm_mon = atoi(month)-1;
        strftime(month_abbrev, sizeof(month_abbrev), "%b", &tm);
        sprintf(friendly, "%s%s-%s%s", month_abbrev, day, hour, minute);
        friendlyname[i] = strdup(friendly);
    }

    // print new list of filenames and friendlynames
    for (i = 0; i < max_filename; i++) {
        printf("I %s:   %d '%s' '%s'\n", progname, i, filename[i], friendlyname[i]);
    }
}

void cleanup_filename_allocations(void)
{
    int i;

    for (i = 0; i < max_filename; i++) {
        free(filename[i]);
        filename[i] = NULL;
        free(friendlyname[i]);
        friendlyname[i] = NULL;
    }
    max_filename = 0;
}

void substring(char *s, int start, int len, char *substring)
{
    memcpy(substring, s+start, len);
    substring[len] = '\0';
}
