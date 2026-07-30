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

// todo-later - check timing of the key highlight vs the tone

// defines
#define EVID_HLOCK         1
#define EVID_PLAY_TONE_SEQ 100
#define EVID_PIANO_KEY     500

#define MAX_ITEMS_STR 100000
#define MAX_TONE_SEQ  100

// variables
char  *progname;
char  *data_dir;

int           piano_key_freq[89];      // starts at [1]
bool          piano_key_is_black[89];  // starts at [1]
char          piano_white_key_to_basic_tone[88];  // starts at [1]
unsigned char piano_freq_to_keynum[4200];

struct {
    char *title;
    char *items;
    int   octave;
} tone_seq_tbl[MAX_TONE_SEQ];
int max_tone_seq;

char *playing_tone_seq_title;

double X, Y;
bool   Xlock;

bool end_program;

// prototypes
void display_update(void);
void process_event(sdlx_event_t *event);
void read_tone_seq_file(char *filename);
void play_tone_seq(char *items, int octave);
void piano_utils_init(void);
int get_piano_keynum_spn(char *item);
int get_piano_keynum_solfege(char *item, int octave);

// -----------------  MAIN  ------------------------------------------

void debug_print_cycle_duration(void);

int main(int argc, char **argv)
{
    sdlx_event_t event;

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // initialize
    piano_utils_init();
    read_tone_seq_file("tones.seq");

    // runtime loop
    while (!end_program) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, LANDSCAPE);

        // render display and register for events
        display_update();

        // register EVID_SHOW_README_FILE event
        reg_event_show_readme_file();

        // register control event to end program
        sdlx_register_control_events(EVID_HLOCK, Xlock ? "UNLOCK" : "LOCK",
                                     0, NULL,
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // debug print the cycle duration, 
        debug_print_cycle_duration();

        // wait for event, with 20 ms timeout
        sdlx_get_event(20000, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        process_event(&event);
    }

    // stop audio
    sdlx_audio_stop();

    // cleanup 
    for (int i = 0; i < max_tone_seq; i++) {
        free(tone_seq_tbl[i].title);
        free(tone_seq_tbl[i].items);
    }

    // end program
    printf("I %s: terminating\n", progname);
    return 0;
}

void debug_print_cycle_duration(void)
{
    long time_now, cycle_dur;
    static long time_start;

    // disable
    return;

    if (time_start == 0) {
        time_start = util_microsec_timer();
        return;
    }

    time_now = util_microsec_timer();
    cycle_dur = time_now - time_start;
    time_start = time_now;
    printf("I %s: cycle_dur %ld ms\n", progname, cycle_dur/1000);
}

// -----------------  DISPLAY UPDATE  --------------------------------

void init_loc(sdlx_loc_t *loc, int x, int y, int w, int h);

void display_update(void)
{
    sdlx_loc_t        *loc, loc2;
    int                keynum, pos, x, y, w, h;
    sdlx_color_t       color;
    sdlx_audio_state_t as;

    static bool first_call = true;

    static int  last_play_tones_seqnum;
    static int  highlight_keynum;
    static long highlight_start;
    static int  white_key_w;
    static int  white_key_h;
    static int  black_key_w;
    static int  black_key_h;
    static int  num_white_keys;
    static int  y_octave;

    // get current audio state
    sdlx_audio_get_state(&as);

    // first call init
    if (first_call) {
        first_call = false;

        white_key_w = sdlx_win_width / 14;
        white_key_h = sdlx_win_height / 3;
        black_key_w = white_key_w * 0.5;
        black_key_h = white_key_h / 2;
        num_white_keys = 52;
        y_octave = sdlx_win_height - white_key_h - 100;

        Y     = 0;
        X     = -23 * white_key_w + 4;
        Xlock = true;

        last_play_tones_seqnum = as.play_tones_seqnum;
    }

    // display title line
    if (playing_tone_seq_title != NULL) {
        if (as.state == AUDIO_STATE_IDLE) {
            playing_tone_seq_title = NULL;
        }
        if (playing_tone_seq_title != NULL) {
            sdlx_render_printf_ex2(
                sdlx_win_width/2, 0,
                FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
                "Playing %s", playing_tone_seq_title);
        }
    }

    // if new tone is being played then request piano keynum be highlighted;
    // clear highlight after 250 ms
    if (as.play_tones_seqnum != last_play_tones_seqnum) {
        highlight_keynum = piano_freq_to_keynum[as.play_tones_freq];
        highlight_start = util_microsec_timer();
    }
    if (highlight_keynum) {
        long duration = util_microsec_timer() - highlight_start;
        if (duration > 250000) {
            highlight_keynum = 0;
            highlight_start = 0;
        }
    }
    last_play_tones_seqnum = as.play_tones_seqnum;

    // display piano white keys
    pos = 0;
    for (keynum = 1; keynum <= 88; keynum++) {
        if (!piano_key_is_black[keynum]) {
            w = white_key_w - 2;
            h = white_key_h;
            x = pos * white_key_w + 1;
            y = sdlx_win_height - white_key_h;
            if (X+x > -white_key_w && X+x < sdlx_win_width) {
                // display white key rectangle
                color = (keynum == highlight_keynum ? COLOR_GREEN : COLOR_WHITE);
                sdlx_render_fill_rect(X+x, y, w, h, color);
                init_loc(&loc2, X+x, y, w, h);
                sdlx_register_event(&loc2, EVID_PIANO_KEY+keynum);

                // display white key basic tone, A-G
                sdlx_render_printf_ex2(
                    X+x+w/2, sdlx_win_height-sdlx_char_height_dflt,
                    FONT_NORMAL, COLOR_BLACK, FLAG_XY_CTR, 
                    "%c", piano_white_key_to_basic_tone[keynum]);
            }
            pos++;
        }
    }

    // display piano black keys
    pos = 0;
    for (keynum = 1; keynum <= 88; keynum++) {
        if (piano_key_is_black[keynum]) {
            h = black_key_h;
            x = (pos + 0.5) * white_key_w + black_key_w/2;
            y = sdlx_win_height - 2 * black_key_h;
            w = black_key_w;
            if (X+x > -w && X+x < sdlx_win_width) {
                color = (keynum == highlight_keynum ? COLOR_GREEN : COLOR_BLACK);
                sdlx_render_fill_rect(X+x, y, w, h, color);
                init_loc(&loc2, X+x, y, w, h);
                sdlx_register_event(&loc2, EVID_PIANO_KEY+keynum);
            }

            int tmp = ((keynum + 8) % 12);
            if (tmp == 10 || tmp == 3) {
                pos += 2;
            } else {
                pos += 1;
            }
        }
    }

    // display piano key octaves
    // - display line above piano keyboard
    x = 0;
    y = y_octave;
    w = num_white_keys * white_key_w;
    h = 4;
    sdlx_render_fill_rect(X+x, y, w, h, COLOR_WHITE);
    // - display vertical octave divider tick marks
    for (int octave = 1; octave <= 8; octave++) {
        x = white_key_w * (7 * (octave-1) + 2) - 5;
        y = y_octave - 50;
        w = 8;
        h = 100;
        sdlx_render_fill_rect(X+x, y, w, h, COLOR_WHITE);
    }
    // - display octave numbers
    for (int octave = 1; octave <= 7; octave++) {
        y = y_octave;
        x = (white_key_w * (7 * (octave-1) + 2)) + (white_key_w * 3.5);
        sdlx_render_printf_ex2(X+x, y, FONT_NORMAL, COLOR_WHITE,
                               FLAG_XY_CTR | FLAG_BG_BLACK, 
                               " %d ", octave);
    }

    // register events to play tone sequence
    y = 0;
    for (int i = 0; i < max_tone_seq; i++) {
        if (Y+y > -sdlx_char_height_dflt && Y+y < y_octave-sdlx_char_height_dflt) {
            loc = sdlx_render_printf_ex1(COL2X(1), Y+y, FONT_NORMAL, COLOR_LIGHT_BLUE, 
                                         "%s", tone_seq_tbl[i].title);
            sdlx_register_event(loc, EVID_PLAY_TONE_SEQ+i);
        }
        y += 1.5 * sdlx_char_height(FONT_NORMAL);
    }

    // register for mouse motion events
    sdlx_register_event(NULL, EVID_MOTION);
}

void init_loc(sdlx_loc_t *loc, int x, int y, int w, int h)
{
    loc->x = x;
    loc->y = y;
    loc->w = w;
    loc->h = h;
}
        
// -----------------  PROCESS EVENT  ---------------------------------

void process_event(sdlx_event_t *event)
{
    if (event->event_id >= EVID_PLAY_TONE_SEQ && event->event_id < EVID_PLAY_TONE_SEQ + MAX_TONE_SEQ) {
        // tone sequence is selected: play the sequence
        int idx = event->event_id - EVID_PLAY_TONE_SEQ;
        play_tone_seq(tone_seq_tbl[idx].items, tone_seq_tbl[idx].octave);
        playing_tone_seq_title = tone_seq_tbl[idx].title;
    } else if (event->event_id >= EVID_PIANO_KEY+1 && event->event_id <= EVID_PIANO_KEY+88) {
        // piano key is pressed: play thhe key
        int keynum = event->event_id - EVID_PIANO_KEY;
        sdlx_tone_t tones[2];
        tones[0].freq = piano_key_freq[keynum];
        tones[0].intvl_ms = 500;
        tones[1].freq = 0;
        tones[1].intvl_ms = 0;
        sdlx_audio_play_tones(tones);
    } else {
        switch (event->event_id) {
        case EVID_HLOCK:
            // toggle lock of piano keyboard horizontal scrolling
            Xlock = !Xlock;
            break;
        case EVID_MOTION:
            // handle mouse motion in the X,Y directions
            // - X scrolls the piano keyboard
            // - Y scrolls the list of tone sequence titles
            double xrel = event->u.motion.xrel;
            double yrel = event->u.motion.yrel;
            if (!Xlock) {
                if (fabs(xrel) > fabs(yrel)*1.5) X += xrel;
            }
            if (fabs(yrel) > fabs(xrel)*1.5) Y += yrel;
            if (Y > 0) Y = 0;
            break;
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_QUIT:
            // end program
            end_program = true;
            break;
        }
    }
}

// -----------------  READ TONE SEQ FILE  ----------------------------

void read_tone_seq_file(char *filename)
{
    FILE *fp;
    char  s[1000], pathname[100];
    char  title[100];
    char  items[MAX_ITEMS_STR];
    int   cnt, octave=4;

    title[0] = '\0';
    items[0] = '\0';

    // open filename
    sprintf(pathname, "%s/%s", data_dir, filename);
    fp = fopen(pathname, "r");
    if (fp == NULL) {
        printf("E %s: failed to open %s, %s\n", progname, pathname, strerror(errno));
        return;
    }

    while (fgets(s, sizeof(s), fp) != NULL) {
        // sanitize input string,
        // removes: trailing newline, comments, leading and trailing spaces
        str_sanitize(s);

        // if blank line then continue
        if (s[0] == '\0') {
            continue;
        }

        // if 's' is a title line then 
        //   if a title and tone sequence is currently under construction
        //     save the in progress title and items
        //     increment max_tone_seq
        //   endif
        //   make copy of the new title
        //   reset tone sequence items to empty string
        //   continue
        // endif
        if (strncasecmp(s, "title ", 6) == 0) {
            if (title[0] && items[0]) {
                tone_seq_tbl[max_tone_seq].title = strdup(title);
                tone_seq_tbl[max_tone_seq].items = strdup(items);
                tone_seq_tbl[max_tone_seq].octave = octave;
                //printf("I %s: tone_seq '%s' / %d = '%s'\n", 
                //       progname,
                //       tone_seq_tbl[max_tone_seq].title,
                //       tone_seq_tbl[max_tone_seq].octave,
                //       tone_seq_tbl[max_tone_seq].items);
                max_tone_seq++;
                if (max_tone_seq == MAX_TONE_SEQ) {
                    fclose(fp);
                    return;
                }
            }
            cnt = sscanf(&s[6], "%s %d", title, &octave);
            if (cnt != 2) {
                printf("E %s: both title and octave required\n", progname);
                fclose(fp);
                return;
            }
            items[0] = '\0';
            continue;
        }

        // the line contains tone sequence items, so save it;
        // append pause2 (500 ms) 
        strcat(items, s);
        strcat(items, " pause2 ");
    }

    // reached EOF, save the last title/items
    if (title[0] && items[0]) {
        tone_seq_tbl[max_tone_seq].title = strdup(title);
        tone_seq_tbl[max_tone_seq].items = strdup(items);
        tone_seq_tbl[max_tone_seq].octave = octave;
        //printf("I %s: tone_seq '%s' / %d = '%s'\n", 
        //       progname,
        //       tone_seq_tbl[max_tone_seq].title,
        //       tone_seq_tbl[max_tone_seq].octave,
        //       tone_seq_tbl[max_tone_seq].items);
        max_tone_seq++;
    }

    // close file
    fclose(fp);
}

void sanitize_input(char *s)
{
    int len, i;
    char *p;

    // remove trailing newline
    len = strlen(s);
    if (len > 0 && s[len-1] == '\n') s[len-1] = '\0';

    // remove comments
    p = strchr(s, '#');
    if (p) *p = '\0';

    // remove leading spaces
    i = 0;
    while (s[i] == ' ' && s[i] != '\0') {
        i++;
    }
    memmove(s, &s[i], strlen(&s[i]));

    // remove trailing spaces
    i = strlen(s) - 1;
    while (i >= 0) {
        if (s[i] == ' ') 
            s[i] = '\0'; 
        else 
            break;
        i--;
    }
}

// -----------------  PLAY TONE SEQ  ---------------------------------

#define MAX_TONE        2000
#define TONE_INTVL_MS   500
#define TONE_GAP_MS     50
#define PAUSE_INTVL_MS  250

sdlx_tone_t tones[MAX_TONE];
int         max_tones;

void add_tone(int freq, int intvl_ms);
void add_gap(int freq);
void add_terminator(void);

void play_tone_seq(char *items, int octave)
{
    int   keynum;
    char  items_copy[MAX_ITEMS_STR];
    char *strtok_arg, *item;

    // make copy of items arg, because strtok is used
    strcpy(items_copy, items);
    strtok_arg = items_copy;

    // init max_tones to 0
    max_tones = 0;

    while (true) {
        // get next item
        item = strtok(strtok_arg, " ");
        strtok_arg = NULL;
        if (item == NULL) {
            break;
        }

        // handle item that changes octave
        if (strcasecmp(item, "up") == 0) {
            octave++;
            continue;
        } else if (strcasecmp(item, "down") == 0) {
            octave--;
            continue;
        } else if (item[0] >= '0' && item[0] <= '8' && item[1] == '\0') {
            octave = item[0] - '0';
            continue;
        }
        if (octave < 0 || octave > 8) {
            printf("E %s: octave %d out of range\n", progname, octave);
            return;
        }

        // handle pause item
        if (strcasecmp(item, "pause1") == 0) {
            add_gap(1*PAUSE_INTVL_MS);
            continue;
        }
        if (strcasecmp(item, "pause2") == 0) {
            add_gap(2*PAUSE_INTVL_MS);
            continue;
        }
        if (strcasecmp(item, "pause4") == 0) {
            add_gap(4*PAUSE_INTVL_MS);
            continue;
        }

        // handle solfege (do,re,mi,...) item
        keynum = get_piano_keynum_solfege(item, octave);
        if (keynum > 0) {
            add_tone(piano_key_freq[keynum], TONE_INTVL_MS);
            add_gap(TONE_GAP_MS);
            continue;
        }

        // handle scientific pitch notation (spn) item
        if (item[0] >= 'A' && item[0] <= 'G') {
            char spn[10];
            strncpy(spn, item, sizeof(spn));
            spn[sizeof(spn)-1] = '\0';
            if (spn[1] == '\0') {
                spn[1] = octave + '0';
                spn[2] = '\0';
            }
            keynum = get_piano_keynum_spn(spn);
            if (keynum > 0) {
                add_tone(piano_key_freq[keynum], TONE_INTVL_MS);
                add_gap(TONE_GAP_MS);
                continue;
            }
        }

        // item is invalid
        printf("E %s: invalid item '%s'\n", progname, item);
        return;
    }

    // add terminator
    add_terminator();

    // play the tones sequence, using thread
    sdlx_audio_play_tones(tones);
}

void add_tone(int freq, int intvl_ms)
{
    tones[max_tones].freq = freq;
    tones[max_tones].intvl_ms = intvl_ms;
    max_tones++;
}

void add_gap(int intvl_ms)
{
    tones[max_tones].freq = 0;
    tones[max_tones].intvl_ms = intvl_ms;
    max_tones++;
}

void add_terminator(void)
{
    tones[max_tones].freq = 0;
    tones[max_tones].intvl_ms = 0;
    max_tones++;
}

// -----------------  PIANO UTILS  -----------------------------------

//#define DEBUG_PIANO_UTILS_INIT

void piano_utils_init(void)
{
    double factor = pow(2,1.0/12);
    double f;
    int    i;

    printf("I %s: factor %f\n", progname, factor);

    // init piano_key_freq tbl
    f = 27.5;
    for (i = 1; i <= 88; i++) {
        piano_key_freq[i] = nearbyint(f);
        f *= factor;
    }

    // init piano_key_is_black tbl
    for (i = 1; i <= 88; i++) {
        int tmp = (i + 8) % 12;
        piano_key_is_black[i] = (tmp == 1 || tmp == 3 || tmp == 6 || tmp == 8 || tmp == 10);
    }

    // init tbl to convert from freq to keynum
    for (i = 1; i <= 88; i++) {
        piano_freq_to_keynum[piano_key_freq[i]] = i;
    }

    // init tbl to convert white keys to basic tone letter
    char note = 'A';
    for (i = 1; i <= 88; i++) {
        if (!piano_key_is_black[i]) {
            piano_white_key_to_basic_tone[i] = note;
            if (++note == 'H') note = 'A';
        }
    }

#ifdef DEBUG_PIANO_UTILS_INIT
    // debug print the freq and is_black tables
    for (i = 1; i <= 88; i++) {
        printf("I %s: %2d %4d %s\n", 
               progname, i, piano_key_freq[i],
               piano_key_is_black[i] ? "black" : "");
    }

    // debug print the freq to keynum conversion tbl
    for (int freq = 27; freq < 4200; freq++) {
        if (piano_freq_to_keynum[freq]) {
            printf("I %s: freq %d  keynum %d\n", progname, freq, piano_freq_to_keynum[freq]);
        }
    }

    printf("I %s: testing get_piano_keynum_spn\n", progname);
    printf("I %s: C1 %d\n", progname, get_piano_keynum_spn("C1"));
    printf("I %s: D1 %d\n", progname, get_piano_keynum_spn("D1"));
    printf("I %s: E1 %d\n", progname, get_piano_keynum_spn("E1"));
    printf("I %s: F1 %d\n", progname, get_piano_keynum_spn("F1"));
    printf("I %s: G1 %d\n", progname, get_piano_keynum_spn("G1"));
    printf("I %s: A1 %d\n", progname, get_piano_keynum_spn("A1"));
    printf("I %s: B1 %d\n", progname, get_piano_keynum_spn("B1"));

    printf("I %s: testing get_piano_keynum_solfege\n", progname);
    printf("I %s: do  %d\n", progname, get_piano_keynum_solfege("do",4));
    printf("I %s: re  %d\n", progname, get_piano_keynum_solfege("re",4));
    printf("I %s: mi  %d\n", progname, get_piano_keynum_solfege("mi",4));
    printf("I %s: fa  %d\n", progname, get_piano_keynum_solfege("fa",4));
    printf("I %s: sol %d\n", progname, get_piano_keynum_solfege("sol",4));
    printf("I %s: la  %d\n", progname, get_piano_keynum_solfege("la",4));
    printf("I %s: ti  %d\n", progname, get_piano_keynum_solfege("ti",4));
#endif
}

int spn_tbl[7] = { 9, 11, 0, 2, 4, 5, 7 };

int get_piano_keynum_spn(char *item)
{
    int note, octave, key;

    if (item[2] != '\0') {
        return 0;
    }

    note   = item[0] - 'A';
    octave = item[1] - '0';

    key = -8 + (octave * 12) + spn_tbl[note];
    if (key < 1 || key > 88) {
        printf("E %s: invalid spn '%s'\n", progname, item);
        return 0;
    }

    return key;
}

char *solfege_tbl[7] = { "do", "re", "mi", "fa", "sol", "la", "ti" };

int get_piano_keynum_solfege(char *item, int octave)
{
    int i;
    char spn[3];

    for (i = 0; i < 7; i++) {
        if (strcasecmp(item, solfege_tbl[i]) == 0) {
            break;
        }
    }
    if (i == 7) {
        return 0;
    }

    spn[0] = 'C' + i;
    if (spn[0] > 'G') spn[0] -= 7;
    spn[1] = '0' + octave;
    spn[2] = '\0';

    return get_piano_keynum_spn(spn);
}
