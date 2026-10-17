#include <stdio.h>
#include <stdbool.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"

// events
#define EVID_START   1
#define EVID_PAUSE   2
#define EVID_CANCEL  3

// params state values
#define STATE_IDLE     0
#define STATE_RUNNING  1
#define STATE_PAUSED   2

// fonts: TIME_FONT matches the StopWatch clock digits
#define TIME_FONT      11
#define BTN_FONT       24

// three buttons across the 1000-wide portrait display
#define MARGIN         40
#define BTN_W          280
#define BTN_H          180
#define BTN_GAP        40

#define REDRAW_US      200000
#define HIGHLIGHT_US   120000

// Morse H: four dits, then a longer gap so each H is distinct.
// Several copies are queued because play_tones does not loop.
#define MORSE_FREQ       1000
#define DIT_MS           100
#define ELEMENT_GAP_MS   100
#define REPEAT_GAP_MS    600
#define ALARM_REPEATS    8
#define ALARM_TONE_MAX   80

#define MAX_MINUTES      999

// variables
char         *progname;
char         *data_dir;

int           state;
long          expire_sec;
long          remain_sec;
bool          alarm_on;
int           highlight_evid;

sdlx_tone_t   alarm_tones[ALARM_TONE_MAX];

int           panel_y;
int           panel_h;
int           status_y;
int           time_cy;
int           hint_y;
int           btn_y;

// prototypes
long wall_sec(void);
bool timer_expired(void);
bool parse_interval(char *s, long *interval_sec);
void save_params(void);
void load_params(void);
void init_alarm_tones(void);
void service_alarm(void);
void start_from_input(void);
void resume_timer(void);
void pause_timer(void);
void cancel_timer(void);
void on_terminate(void);
void layout_metrics(void);
int  button_x(int col);
void draw_button(int col, char *label, sdlx_color_t bg, sdlx_color_t fg,
                 int evid, bool enabled);
void format_clock(char *str, int len, long sec, bool force_neg);
void draw_display(void);
bool handle_event(sdlx_event_t *event);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    sdlx_event_t event;
    bool         done;
    long         timeout;

    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    init_alarm_tones();
    load_params();
    if (state == STATE_RUNNING) {
        printf("I %s: restored running, expire=%ld\n", progname, expire_sec);
    } else if (state == STATE_PAUSED) {
        printf("I %s: restored paused, remain=%ld\n", progname, remain_sec);
    }

    done = false;
    while (!done) {
        service_alarm();

        sdlx_display_init(COLOR_BLACK, PORTRAIT);
        draw_display();
        sdlx_display_present();

        if (highlight_evid != 0) {
            timeout = HIGHLIGHT_US;
        } else if (state == STATE_RUNNING) {
            timeout = REDRAW_US;
        } else {
            timeout = -1;
        }
        sdlx_get_event(timeout, &event);

        if (event.event_id == -1) {
            highlight_evid = 0;
            continue;
        }
        done = handle_event(&event);
    }

    on_terminate();
    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  TIME  ------------------------------------------

long wall_sec(void)
{
    return util_get_real_time_microsec() / 1000000;
}

bool timer_expired(void)
{
    if (state != STATE_RUNNING) {
        return false;
    }
    if (wall_sec() >= expire_sec) {
        return true;
    }
    return false;
}

// "5" -> 5 minutes. "5.30" or "5.3" -> minutes and seconds. "0.45" -> 45 seconds.
bool parse_interval(char *s, long *interval_sec)
{
    int  i;
    int  digit;
    long minutes;
    long seconds;
    bool saw_digit;

    i = 0;
    while (s[i] == ' ') {
        i++;
    }

    minutes = 0;
    saw_digit = false;
    while (s[i] >= '0' && s[i] <= '9') {
        digit = s[i] - '0';
        if (minutes > (MAX_MINUTES / 10)) {
            return false;
        }
        minutes = minutes * 10 + digit;
        saw_digit = true;
        i++;
    }
    if (!saw_digit) {
        return false;
    }
    if (minutes > MAX_MINUTES) {
        return false;
    }

    seconds = 0;
    if (s[i] == '.') {
        i++;
        saw_digit = false;
        while (s[i] >= '0' && s[i] <= '9') {
            digit = s[i] - '0';
            if (seconds > 5) {
                return false;
            }
            seconds = seconds * 10 + digit;
            saw_digit = true;
            i++;
        }
        if (!saw_digit) {
            return false;
        }
        if (seconds > 59) {
            return false;
        }
    }

    while (s[i] == ' ') {
        i++;
    }
    if (s[i] != '\0') {
        return false;
    }
    if (minutes == MAX_MINUTES && seconds > 0) {
        return false;
    }
    if (minutes == 0 && seconds == 0) {
        return false;
    }

    *interval_sec = minutes * 60 + seconds;
    return true;
}

void format_clock(char *str, int len, long sec, bool force_neg)
{
    long mag;
    int  minutes;
    int  seconds;
    bool neg;

    neg = false;
    mag = sec;
    if (sec < 0) {
        neg = true;
        mag = -sec;
    }
    if (force_neg && sec == 0) {
        neg = true;
    }
    minutes = (int)(mag / 60);
    seconds = (int)(mag % 60);
    if (neg) {
        snprintf(str, len, "-%d:%02d", minutes, seconds);
    } else {
        snprintf(str, len, "%d:%02d", minutes, seconds);
    }
}

// -----------------  PARAMS  ----------------------------------------

void save_params(void)
{
    util_set_numeric_param(data_dir, "state", state);
    util_set_numeric_param(data_dir, "expire_sec", (double)expire_sec);
    util_set_numeric_param(data_dir, "remain_sec", (double)remain_sec);
}

void load_params(void)
{
    double state_val;
    double expire_val;
    double remain_val;

    state_val = util_get_numeric_param(data_dir, "state", 0);
    expire_val = util_get_numeric_param(data_dir, "expire_sec", 0);
    remain_val = util_get_numeric_param(data_dir, "remain_sec", 0);

    state = (int)(state_val + 0.5);
    expire_sec = (long)(expire_val + 0.5);
    remain_sec = (long)(remain_val + 0.5);

    if (state == STATE_RUNNING) {
        if (expire_sec <= 0) {
            state = STATE_IDLE;
            expire_sec = 0;
            remain_sec = 0;
            save_params();
        }
        return;
    }
    if (state == STATE_PAUSED) {
        if (remain_sec < 1 || remain_sec > (long)MAX_MINUTES * 60) {
            state = STATE_IDLE;
            expire_sec = 0;
            remain_sec = 0;
            save_params();
        }
        return;
    }
    if (state != STATE_IDLE) {
        state = STATE_IDLE;
        expire_sec = 0;
        remain_sec = 0;
        save_params();
    }
}

// -----------------  ALARM  -----------------------------------------

void init_alarm_tones(void)
{
    int i;
    int rep;
    int dit;

    // 8 repeats of (4 dits + 4 element gaps + 1 longer gap) + terminator = 73
    i = 0;
    for (rep = 0; rep < ALARM_REPEATS; rep++) {
        for (dit = 0; dit < 4; dit++) {
            alarm_tones[i].freq = MORSE_FREQ;
            alarm_tones[i].intvl_ms = DIT_MS;
            i++;
            alarm_tones[i].freq = 0;
            alarm_tones[i].intvl_ms = ELEMENT_GAP_MS;
            i++;
        }
        alarm_tones[i].freq = 0;
        alarm_tones[i].intvl_ms = REPEAT_GAP_MS;
        i++;
    }
    alarm_tones[i].freq = 0;
    alarm_tones[i].intvl_ms = 0;
}

void service_alarm(void)
{
    sdlx_audio_state_t audio;

    if (!timer_expired()) {
        alarm_on = false;
        return;
    }
    if (!alarm_on) {
        alarm_on = true;
        printf("I %s: expired\n", progname);
    }
    sdlx_audio_get_state(&audio);
    if (audio.state == AUDIO_STATE_IDLE) {
        sdlx_audio_play_tones(alarm_tones);
    }
}

// -----------------  CONTROLS  --------------------------------------

void start_from_input(void)
{
    char *input;
    long  interval;

    input = sdlx_get_input_str(
        "Minutes or minutes.seconds\n(seconds optional)\nExample: 5 or 5.30",
        true, NULL);
    if (input[0] == '\0') {
        sdlx_show_toast("Canceled");
        return;
    }
    if (!parse_interval(input, &interval)) {
        sdlx_show_toast("Invalid interval");
        return;
    }

    expire_sec = wall_sec() + interval;
    remain_sec = 0;
    state = STATE_RUNNING;
    alarm_on = false;
    save_params();
    printf("I %s: running, interval=%ld sec\n", progname, interval);
}

void resume_timer(void)
{
    if (state != STATE_PAUSED) {
        return;
    }
    if (remain_sec < 1) {
        return;
    }
    expire_sec = wall_sec() + remain_sec;
    state = STATE_RUNNING;
    alarm_on = false;
    save_params();
    printf("I %s: resumed, remain=%ld sec\n", progname, remain_sec);
}

void pause_timer(void)
{
    long left;

    if (state != STATE_RUNNING) {
        return;
    }
    left = expire_sec - wall_sec();
    if (left < 1) {
        return;
    }
    remain_sec = left;
    expire_sec = 0;
    state = STATE_PAUSED;
    alarm_on = false;
    save_params();
    printf("I %s: paused, remain=%ld sec\n", progname, remain_sec);
}

void cancel_timer(void)
{
    sdlx_audio_stop();
    alarm_on = false;
    state = STATE_IDLE;
    expire_sec = 0;
    remain_sec = 0;
    save_params();
    printf("I %s: canceled\n", progname);
}

void on_terminate(void)
{
    if (timer_expired()) {
        state = STATE_IDLE;
        expire_sec = 0;
        remain_sec = 0;
        alarm_on = false;
        save_params();
        sdlx_audio_stop();
        printf("I %s: alarm canceled on exit\n", progname);
        return;
    }
    if (state == STATE_RUNNING) {
        save_params();
        printf("I %s: saved running, expire=%ld\n", progname, expire_sec);
        return;
    }
    if (state == STATE_PAUSED) {
        save_params();
        printf("I %s: saved paused, remain=%ld\n", progname, remain_sec);
        return;
    }
    state = STATE_IDLE;
    expire_sec = 0;
    remain_sec = 0;
    save_params();
}

// -----------------  LAYOUT  ----------------------------------------

void layout_metrics(void)
{
    int time_h;

    time_h = sdlx_char_height(TIME_FONT);

    panel_y = 220;
    panel_h = 980;
    status_y = panel_y + 56;
    time_cy = panel_y + panel_h / 2;
    hint_y = time_cy + time_h / 2 + 48;
    btn_y = panel_y + panel_h + 80;
}

int button_x(int col)
{
    return MARGIN + col * (BTN_W + BTN_GAP);
}

// -----------------  DRAW  ------------------------------------------

void draw_button(int col, char *label, sdlx_color_t bg, sdlx_color_t fg,
                 int evid, bool enabled)
{
    int          x;
    sdlx_color_t fill;
    sdlx_color_t text;
    sdlx_color_t edge;
    sdlx_loc_t   loc;
    bool         highlight;

    x = button_x(col);
    highlight = (highlight_evid == evid);
    if (!enabled) {
        fill = COLOR_DARK_GRAY;
        text = COLOR_GRAY;
        edge = COLOR_GRAY;
    } else if (highlight) {
        fill = COLOR_WHITE;
        text = COLOR_BLACK;
        edge = COLOR_WHITE;
    } else {
        fill = bg;
        text = fg;
        edge = COLOR_WHITE;
    }

    sdlx_render_fill_rect(x, btn_y, BTN_W, BTN_H, fill);
    sdlx_render_rect(x, btn_y, BTN_W, BTN_H, 4, edge);
    sdlx_render_printf_ex(x + BTN_W / 2, btn_y + BTN_H / 2,
                          BTN_FONT, text, FLAG_XY_CTR, "%s", label);

    if (enabled) {
        loc.x = x;
        loc.y = btn_y;
        loc.w = BTN_W;
        loc.h = BTN_H;
        sdlx_register_event(&loc, evid);
    }
}

void draw_display(void)
{
    char         clock[32];
    char        *state_str;
    char        *hint;
    char        *start_label;
    sdlx_color_t state_color;
    sdlx_color_t time_color;
    long         now;
    long         sec;
    bool         expired;
    bool         start_enabled;
    bool         pause_enabled;
    bool         cancel_enabled;

    layout_metrics();
    now = wall_sec();
    sec = 0;
    expired = false;
    if (state == STATE_RUNNING) {
        sec = expire_sec - now;
        if (now >= expire_sec) {
            expired = true;
        }
    } else if (state == STATE_PAUSED) {
        sec = remain_sec;
    }

    hint = NULL;
    if (expired) {
        state_str = "EXPIRED";
        state_color = COLOR_RED;
        time_color = COLOR_RED;
        hint = "Tap CANCEL to stop alarm";
    } else if (state == STATE_RUNNING) {
        state_str = "RUNNING";
        state_color = COLOR_LIGHT_GREEN;
        time_color = COLOR_LIGHT_GREEN;
        hint = NULL;
    } else if (state == STATE_PAUSED) {
        state_str = "PAUSED";
        state_color = COLOR_ORANGE;
        time_color = COLOR_ORANGE;
        hint = "Tap RESUME to continue";
    } else {
        state_str = "READY";
        state_color = COLOR_GRAY;
        time_color = COLOR_WHITE;
        hint = "Tap START, then 5 or 5.30";
    }

    sdlx_render_fill_rect(20, panel_y, sdlx_win_width - 40, panel_h, COLOR_DARK_GRAY);
    sdlx_render_rect(20, panel_y, sdlx_win_width - 40, panel_h, 3, COLOR_GRAY);

    sdlx_render_printf_ex(sdlx_win_width / 2, status_y,
                          FONT_SMALL, state_color, FLAG_X_CTR, "%s", state_str);

    format_clock(clock, (int)sizeof(clock), sec, expired);
    sdlx_render_printf_ex(sdlx_win_width / 2, time_cy,
                          TIME_FONT, time_color, FLAG_XY_CTR, "%s", clock);

    if (hint != NULL) {
        sdlx_render_printf_ex(sdlx_win_width / 2, hint_y,
                              FONT_SMALL, COLOR_GRAY, FLAG_X_CTR, "%s", hint);
    }

    if (state == STATE_PAUSED) {
        start_label = "RESUME";
        start_enabled = true;
    } else if (state == STATE_IDLE) {
        start_label = "START";
        start_enabled = true;
    } else {
        start_label = "START";
        start_enabled = false;
    }
    pause_enabled = (state == STATE_RUNNING && !expired);
    cancel_enabled = (state != STATE_IDLE);

    draw_button(0, start_label, COLOR_LIGHT_GREEN, COLOR_BLACK, EVID_START, start_enabled);
    draw_button(1, "PAUSE", COLOR_LIGHT_BLUE, COLOR_BLACK, EVID_PAUSE, pause_enabled);
    draw_button(2, "CANCEL", COLOR_RED, COLOR_WHITE, EVID_CANCEL, cancel_enabled);

    reg_event_show_readme_file();
    sdlx_register_control_events(0, NULL, 0, NULL, EVID_QUIT, "X");
}

// -----------------  EVENTS  ----------------------------------------

bool handle_event(sdlx_event_t *event)
{
    switch (event->event_id) {
    case EVID_QUIT:
        return true;
    case EVID_SHOW_README_FILE:
        show_file(data_dir, "README");
        highlight_evid = 0;
        break;
    case EVID_START:
        if (state == STATE_IDLE) {
            start_from_input();
        } else if (state == STATE_PAUSED) {
            resume_timer();
        }
        highlight_evid = EVID_START;
        break;
    case EVID_PAUSE:
        pause_timer();
        highlight_evid = EVID_PAUSE;
        break;
    case EVID_CANCEL:
        if (state != STATE_IDLE) {
            cancel_timer();
        }
        highlight_evid = EVID_CANCEL;
        break;
    }
    return false;
}
