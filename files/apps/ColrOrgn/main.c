#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <libgen.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/ColrOrgn/common.h"
#include "lib/lib.h"

//
// defines
//

#define EVID_STOP                  1     // stop 
#define EVID_MON_REC               2     // monitor or record from device
#define EVID_MONITOR               3     // select monitor mode
#define EVID_RECORD                4     // select record mode
#define EVID_PAUSE                 5     // pause play file
#define EVID_CONT                  6     // continue play file
#define EVID_SHOW_CONTROLS         7     // show/hide controls
#define EVID_SETTINGS              8     // color organ settings display
#define EVID_TEST_FORCE_LANDSCAPE  9     // force landscape orientation, used when testing on linux
#define EVID_PLAY_GO_FWD          10     // playback go forward 1 minute
#define EVID_PLAY_GO_BACK         11     // playback go backward 1 minut
#define EVID_PLAY_FILE           100     // start play file, range 100-199
#define EVID_DELETE_FILE         200     // delete file, range 200-299
#define EVID_RENAME_FILE         300     // delete file, range 300-399

#define STATE_STOPPED               0
#define STATE_PLAYING_FILE          1
#define STATE_PLAYING_FILE_PAUSED   2
#define STATE_MONITORING_DEV        3
#define STATE_RECORDING_DEV         4

#define MAX_FILES 100

#define NO_APPEND    false
#define START_PAUSED true

//
// variables
//

char *files[MAX_FILES];
char *files_noext[MAX_FILES];
int   max_files;
int   y_files_list;
int   state = STATE_STOPPED;
char  playing_file[100];
bool  end_program;
bool  test_force_landscape;

//
// prototypes
//

// event handling
void process_event(sdlx_event_t *ev, sdlx_audio_state_t *as);
void register_events(void);

// utils
void get_list_of_files(void);
void rename_recording_mp3(void);
char *get_new_mp3_filename_noext(void);
char *get_state_str(void);
char *secs_to_mmss_str(int secs, char *str);
char *remove_ext(char *filename);

// -----------------  MAIN  ------------------------------------------

void display_state(sdlx_audio_state_t *);
    
int main(int argc, char **argv)
{
    sdlx_event_t       event;
    sdlx_audio_state_t as;
    long               time_start=0, time_now, cycle_dur;

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // init variables
    sprintf(files_dir, "%s/files", data_dir);
    show_controls = true;

    // there should not be a recording.mp3 file present when this
    // miniApp starts; but if there is then it is removed
    util_delete_file(data_dir, "recording.mp3");

    // initialize color organ
    color_organ_init();

    // runtime loop
    while (!end_program) {
        // if audio_state is idle then set state stopped;
        // this is needed for the case where playback of a file completes
        sdlx_audio_get_state(&as);
        if (as.state == AUDIO_STATE_IDLE) {
            state = STATE_STOPPED;
            playing_file[0] = '\0';
        }

        // get device orientation
        if (test_force_landscape) {
            orientation = LANDSCAPE;
        } else {
            orientation = get_device_orientation();
        }

        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, orientation);

        // display color organ
        if (state != STATE_STOPPED) {
            color_organ_display();
        }

        // display state line
        if (show_controls) {
            display_state(&as);
        }

        // register events
        register_events();

        // present the display
        sdlx_display_present();

        // debug print the cycle duration, 
        if (debug_flags & DEBUG_FLAG_CYCLE_DUR) {
            time_now = util_microsec_timer();
            cycle_dur = time_now - time_start;
            time_start = time_now;
            if (cycle_dur < 1000000) {
                printf("I %s: cycle_dur %ld ms\n", progname, cycle_dur/1000);
            }
        }

        // Wait for event, use 45 ms event wait timeout to allow for processing time.
        // Ideally the cycle duration should be close to 50 ms because that is
        //  the time duration of the fft.
        // If the event wait timedout then continue.
        sdlx_get_event(45000, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        process_event(&event, &as);
    }

    // cleanup
    sdlx_audio_stop();
    color_organ_cleanup();
    state = STATE_STOPPED;
    playing_file[0] = '\0';

    // if recorded file exists then rename it
    rename_recording_mp3();

    // terminate
    printf("I %s: terminating\n", progname);
    return 0;
}

void display_state(sdlx_audio_state_t *as)
{
    char dur_str1[12], dur_str2[12];
    sdlx_color_t color = (state==STATE_RECORDING_DEV ? COLOR_RED : COLOR_WHITE);

    dur_str1[0] = dur_str2[0] = '\0';
    if (as->record_secs) {
        secs_to_mmss_str(as->record_secs, dur_str1);
    } else if (as->play_total_secs) {
        secs_to_mmss_str(as->play_total_secs, dur_str1);
        secs_to_mmss_str(as->play_current_secs, dur_str2);
    }

    if (orientation == PORTRAIT) {
        sdlx_render_printf_ex2(sdlx_win_width/2, COH_P,
                               FONT_NORMAL, color, FLAG_X_CTR, 
                               "%s %s %s", 
                               get_state_str(), dur_str1, dur_str2);
    } else {
        sdlx_render_printf_ex2(sdlx_win_width/2, 0,
                               FONT_NORMAL, color, FLAG_X_CTR, 
                               "%s %s %s %s", 
                               get_state_str(), remove_ext(playing_file), dur_str1, dur_str2);
    }
}

// -----------------  EVENT HANDLING  --------------------------------

void process_event(sdlx_event_t *ev, sdlx_audio_state_t *as)
{
    int new_play_file_time;

    if (ev->event_id >= EVID_PLAY_FILE && ev->event_id < EVID_PLAY_FILE+MAX_FILES) {
        // stop playing or recording
        sdlx_audio_stop();
        state = STATE_STOPPED;
        playing_file[0] = '\0';

        // if recorded file exists then rename it
        rename_recording_mp3();

        // play the selected file
        int idx = ev->event_id - EVID_PLAY_FILE;
        sdlx_audio_play_file(files_dir, files[idx]);
        state = STATE_PLAYING_FILE;
        strcpy(playing_file, files[idx]);
    } else if (ev->event_id >= EVID_DELETE_FILE && ev->event_id < EVID_DELETE_FILE+MAX_FILES) {
        // delete the selected file
        int idx;
        char str[100], *input;
        idx = ev->event_id - EVID_DELETE_FILE;
        sprintf(str, "DEL %s (y/n)", files_noext[idx]);
        input = sdlx_get_input_str(str, false, "y");
        if (strcasecmp(input, "y") == 0) {
            util_delete_file(files_dir, files[idx]);
        }
    } else if (ev->event_id >= EVID_RENAME_FILE && ev->event_id < EVID_RENAME_FILE+MAX_FILES) {
        // rename the selected file
        int idx = ev->event_id - EVID_RENAME_FILE;
        char *input = sdlx_get_input_str("RecordedFileName", false, NULL);
        char  new_name[100];
        if (input[0] != '\0') {
            sprintf(new_name, "%s.mp3", input);
            util_rename_file(files_dir, files[idx], files_dir, new_name);
        }
    } else {
        switch (ev->event_id) {
        // stop audio
        case EVID_STOP:
            // stop playing or recording
            sdlx_audio_stop();
            state = STATE_STOPPED;
            playing_file[0] = '\0';

            // if recorded file exists then rename it
            rename_recording_mp3();
            break;

        // monitor or record device, applies when in STATE_STOPPED
        case EVID_MON_REC:
            sdlx_audio_record_from_device(data_dir, "recording.mp3", NO_APPEND, START_PAUSED);
            state = STATE_MONITORING_DEV;
            playing_file[0] = '\0';
            break;

        // these apply when monitoring or recording the android device
        case EVID_MONITOR:
            sdlx_audio_pause();
            state = STATE_MONITORING_DEV;
            playing_file[0] = '\0';
            break;
        case EVID_RECORD:
            sdlx_audio_resume();
            state = STATE_RECORDING_DEV;
            playing_file[0] = '\0';
            break;

        // this apply when playing a file
        case EVID_PAUSE:
            sdlx_audio_pause();
            state = STATE_PLAYING_FILE_PAUSED;
            break;
        case EVID_CONT:
            sdlx_audio_resume();
            state = STATE_PLAYING_FILE;
            break;
        case EVID_PLAY_GO_BACK:
            if ((as->play_current_secs % 60) >= 5) {
                new_play_file_time = as->play_current_secs / 60 * 60;
            } else {
                new_play_file_time = (as->play_current_secs - 60) / 60 * 60;
            }
            if (new_play_file_time < 0) new_play_file_time = 0;
            sdlx_audio_set_play_file_time(new_play_file_time);
            break;
        case EVID_PLAY_GO_FWD:
            new_play_file_time = (as->play_current_secs + 60) / 60 * 60;
            if ((as->play_total_secs >= 3) && 
                (as->play_current_secs < as->play_total_secs-3) &&
                (new_play_file_time > as->play_total_secs-3))
            {
                new_play_file_time = as->play_total_secs-3;
            }
            sdlx_audio_set_play_file_time(new_play_file_time);
            break;

        // end program
        case EVID_QUIT:
            end_program = true;
            break;

        // show readme file
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;

        // scroll file list
        case EVID_MOTION:
            y_files_list += ev->u.motion.yrel;
            break;

        // toggle flag to show or hide the controls
        case EVID_SHOW_CONTROLS:
            show_controls = !show_controls;
            break;

        // activate the color organ settings display
        case EVID_SETTINGS:
            color_organ_settings();
            break;

#ifndef ANDROID
        // This event is only registered when this program is being tested on linux;
        // and will toggle override of the orientation so that landscape orientation
        // can be tested on linux. Linux does not have the acceleration sensor shich is
        // used on android to detect the orientation.
        case EVID_TEST_FORCE_LANDSCAPE:
            test_force_landscape = !test_force_landscape;
            break;
#endif

        // adjust color organ
        default:
            color_organ_process_event(ev);
            break;
        }
    }
}

void register_events(void)
{
    int x_controls, y_controls;
    int y_files_list_top;
    int y_files_list_bottom;

    static int last_orientation = -1;

    if (show_controls) {
        if (orientation == PORTRAIT) {
            x_controls = 0;
            y_controls = COH_P + LINE_SPACING;

            // register EVID_MON_REC or EVID_STOP on control line 1, col 0
            if (state == STATE_STOPPED) {
                reg_event_str(x_controls, y_controls, COLOR_LIGHT_BLUE, "MON/REC", EVID_MON_REC);
            } else {
                reg_event_str(x_controls, y_controls, COLOR_LIGHT_BLUE, "STOP", EVID_STOP);
            }

            // register EVID RECORD, MONITOR, PAUSE, or CONT on control line 1 col 4
            if (state == STATE_MONITORING_DEV) {
                reg_event_str(x_controls+5*sdlx_char_width_dflt, y_controls, COLOR_LIGHT_BLUE, "REC", EVID_RECORD);
            } else if (state == STATE_RECORDING_DEV) {
                reg_event_str(x_controls+5*sdlx_char_width_dflt, y_controls, COLOR_LIGHT_BLUE, "MON", EVID_MONITOR);
            } else if (state == STATE_PLAYING_FILE) {
                reg_event_str(x_controls+5*sdlx_char_width_dflt, y_controls, COLOR_LIGHT_BLUE, "PAUSE", EVID_PAUSE);
            } else if (state == STATE_PLAYING_FILE_PAUSED) {
                reg_event_str(x_controls+5*sdlx_char_width_dflt, y_controls, COLOR_LIGHT_BLUE, "CONT", EVID_CONT);
            }
        } else {  // LANDSCAPE
            x_controls = sdlx_win_width - 8*sdlx_char_width_dflt;
            y_controls = LINE_SPACING;

            // register EVID_MON_REC or EVID_STOP on control line 1, col 0
            if (state == STATE_STOPPED) {
                reg_event_str(x_controls, y_controls, COLOR_LIGHT_BLUE, "MON/REC", EVID_MON_REC);
            } else {
                reg_event_str(x_controls, y_controls, COLOR_LIGHT_BLUE, "STOP", EVID_STOP);
            }

            // register EVID RECORD, MONITOR, PAUSE, or CONT on control line 1 col 4
            y_controls += LINE_SPACING;
            if (state == STATE_MONITORING_DEV) {
                reg_event_str(x_controls, y_controls, COLOR_LIGHT_BLUE, "REC", EVID_RECORD);
            } else if (state == STATE_RECORDING_DEV) {
                reg_event_str(x_controls, y_controls, COLOR_LIGHT_BLUE, "MON", EVID_MONITOR);
            } else if (state == STATE_PLAYING_FILE) {
                reg_event_str(x_controls, y_controls, COLOR_LIGHT_BLUE, "PAUSE", EVID_PAUSE);
            } else if (state == STATE_PLAYING_FILE_PAUSED) {
                reg_event_str(x_controls, y_controls, COLOR_LIGHT_BLUE, "CONT", EVID_CONT);
            }

            // register EVID_PLAY_GO_FWD and EVID_PLAY_GO_BACK
            y_controls += 3*LINE_SPACING;
            if (state == STATE_PLAYING_FILE || state == STATE_PLAYING_FILE_PAUSED) {
                reg_event_str(x_controls, y_controls, COLOR_LIGHT_BLUE, "< ", EVID_PLAY_GO_BACK);
                reg_event_str(x_controls+4*sdlx_char_width_dflt, y_controls, COLOR_LIGHT_BLUE, ">", EVID_PLAY_GO_FWD);
            }
        }

        // get list of mp3 files, and register events to play, rename, or delete each file
        get_list_of_files();

        if (orientation == PORTRAIT) {
            y_files_list_top    = COH_P + 2 * LINE_SPACING;
            y_files_list_bottom = sdlx_win_height;
        } else {
            y_files_list_top    = LINE_SPACING;
            y_files_list_bottom = sdlx_win_height;
        }

        // if the display orientation has changed set list of mp3 files back to top
        if (orientation != last_orientation) {
            y_files_list = y_files_list_top;
            last_orientation = orientation;
        }

        // limit file list scrolling
        if (y_files_list >= y_files_list_top) {
            y_files_list = y_files_list_top;
        }

        // disable file list scrolling if too few files
        if ((orientation == PORTRAIT && max_files <= 6) ||
            (orientation == LANDSCAPE && max_files <= 5))
        {
            y_files_list = y_files_list_top;
        }

        // loop over the list of files
        for (int i = 0; i < max_files; i++) {
            sdlx_color_t color;
            int          y;

            // handle scrolling of the files list
            y = y_files_list + i * LINE_SPACING;
            if (y < y_files_list_top) continue;
            if (y+sdlx_char_height_dflt > y_files_list_bottom) break;

            // register event to play the file;
            // the file name is displayed green if it is currently being played
            if ((state == STATE_PLAYING_FILE || state == STATE_PLAYING_FILE_PAUSED) && 
                (strcmp(playing_file, files[i]) == 0)) 
            {
                color = COLOR_GREEN;
            } else {
                color = COLOR_LIGHT_BLUE;
            }
            reg_event_str(0, y, color, files_noext[i], EVID_PLAY_FILE+i);

            // when in PORTRAIT orientation register additional events associated 
            // with each filenname
            if (orientation == PORTRAIT) {
                // register event to rename and delete the file;
                if (color == COLOR_LIGHT_BLUE) {
                    reg_event_str(sdlx_win_width-8*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, " REN", EVID_RENAME_FILE+i);
                    reg_event_str(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, " DEL", EVID_DELETE_FILE+i);
                }

                // register events to adjust playback time forward or backward
                if (color == COLOR_GREEN) {
                    reg_event_str(sdlx_win_width-8*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "  < ", EVID_PLAY_GO_BACK);
                    reg_event_str(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "  > ", EVID_PLAY_GO_FWD);
                }
            }
        }

        // register motion event, this is used to scroll the file list
        sdlx_register_event(NULL, EVID_MOTION);

#ifndef ANDROID
        // if not running on android then provide control to simulate landscape orientation;
        // this is provided for development test
        reg_event_str(COL2X(1), 0, COLOR_LIGHT_BLUE, "H", EVID_TEST_FORCE_LANDSCAPE);
#endif
    }

    // register EVID_SHOW_README_FILE event
    reg_event_show_readme_file();

    // register control events
    sdlx_register_control_events(EVID_SETTINGS, "STG",
                                 EVID_SHOW_CONTROLS, (!show_controls ? "SHOW" : "HIDE"),
                                 EVID_QUIT, "X");
}

// -----------------  UTILS  -----------------------------------------

void get_list_of_files(void)
{
    static long saved_mtime;

    char s[200], cmd[300], *p;
    FILE *fp;
    long  mtime;

    // if files_dir has been modified then
    // update list of mp3 and wav files
    mtime = util_file_mtime(files_dir, NULL);
    if (mtime != saved_mtime) {
        saved_mtime = mtime;
        printf("I %s: generate list of files\n", progname);

        // free existing list
        for (int i = 0; i < max_files; i++) {
            free(files[i]);
            files[i] = NULL;
            free(files_noext[i]);
            files_noext[i] = NULL;
        }
        max_files = 0;

        // create new list of files
        sprintf(cmd, "/bin/ls -1 %s", files_dir);
        fp = popen(cmd, "r");
        while (fgets(s, sizeof(s), fp) != NULL) {
            str_remove_trailing_newline(s);

            files[max_files] = strdup(s);

            if ((p = strchr(s, '.')) != NULL) *p = '\0';
            files_noext[max_files] = strdup(s);

            printf("I %s: files[%d] = %s\n", progname, max_files, files[max_files]);
            max_files++;
        }
        pclose(fp);
    }
}

void rename_recording_mp3(void)
{
    char new_name[100], *dflt_new_name, *input;

    // must be called when audio is stopped
    if (state != STATE_STOPPED) {
        printf("E %s: rename_recording_mp3 must be called in STATE_STOPPED\n", progname);
        return;
    }

    // if recording.mp3 file doesn't exist then return
    if (!util_file_exists(data_dir, "recording.mp3")) {
        return;
    }

    // get new file name
    dflt_new_name = get_new_mp3_filename_noext();
    input = sdlx_get_input_str("RecordedFileName", false, dflt_new_name);

    // if new file name is provided then
    //   rename recording.mp3 to the new file name
    // else
    //   delete the recording.mp3 file
    // endif
    if (input[0] != '\0') {
        sprintf(new_name, "%s.mp3", input);
        util_rename_file(data_dir, "recording.mp3", files_dir, new_name);
    } else {
        util_delete_file(data_dir, "recording.mp3");
    }
}

char *get_new_mp3_filename_noext(void)
{
    int i, cnt, n, max_n=0;
    static char new_mp3_filename[20];
    
    for (i = 0; i < max_files; i++) {
        cnt = sscanf(files[i], "new%d.mp3", &n);
        if (cnt == 1 && n > max_n) {
            max_n = n;
        }
    }

    sprintf(new_mp3_filename, "new%d", max_n+1);
    return new_mp3_filename;
}

char *get_state_str(void)
{
    switch (state) {
    case STATE_STOPPED:
        return "Stopped";
    case STATE_PLAYING_FILE:
        return "Playing";
    case STATE_PLAYING_FILE_PAUSED:
        return "Paused ";
    case STATE_MONITORING_DEV:
        return "MonDev ";
    case STATE_RECORDING_DEV:
        return "RecDev ";
    default:
        return "????   ";
    }
}

char *secs_to_mmss_str(int secs, char *str)
{
    int minutes;

    minutes = secs / 60;
    secs -= minutes * 60;

    sprintf(str, "%d:%02d", minutes, secs);
    return str;
}

char *remove_ext(char *filename)
{
    static char str[100];
    char *p;

    strcpy(str, filename);
    p = strchr(str, '.');
    if (p) *p = '\0';
    return str;
}
