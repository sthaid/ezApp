#include <stdio.h>
#include <stdbool.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"

// events
#define EVID_START_STOP  1
#define EVID_LAP         2
#define EVID_RESET       3

// lap list
#define MAX_LAPS         99

// fonts: TIME_FONT is "HH:MM:SS", FRAC_FONT is the hundredths
#define TIME_FONT        11
#define FRAC_FONT        18
#define BTN_FONT         24

// three buttons across the 1000-wide portrait display
#define MARGIN           40
#define BTN_W            280
#define BTN_H            180
#define BTN_GAP          40

#define RUN_REDRAW_US    30000
#define HIGHLIGHT_US     120000

#define VIBRATE_STRENGTH 0.25
#define VIBRATE_MS       30

// variables
char *progname;
char *data_dir;

bool   running;
long   base_us;
long   run_start_us;
long   laps[MAX_LAPS];
int    lap_count;
double scroll_y;
bool   scroll_drag;
int    highlight_evid;

int    status_y;
int    time_cy;
int    lap_line_y;
int    btn_y;
int    header_y;
int    rows_y;
int    rows_bottom;
int    row_h;
int    num_x;
int    split_x;
int    total_x;

// prototypes
void layout_metrics(void);
long elapsed_us(void);
long current_lap_us(void);
long lap_split_us(int idx);
long fastest_split_cs(void);
void format_parts(char *hms, int hms_len, char *frac, int frac_len, long us);
void format_elapsed(char *str, int len, long us);
void clamp_scroll(void);
int  button_x(int col);
void draw_button(int col, char *label, sdlx_color_t bg, sdlx_color_t fg,
                int evid, bool enabled);
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

    done = false;
    while (!done) {
        sdlx_display_init(COLOR_BLACK, PORTRAIT);
        draw_display();
        sdlx_display_present();

        if (highlight_evid != 0) {
            timeout = HIGHLIGHT_US;
        } else if (running) {
            timeout = RUN_REDRAW_US;
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

    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  TIME  ------------------------------------------

long elapsed_us(void)
{
    long us;

    us = base_us;
    if (running) {
        us += util_microsec_timer() - run_start_us;
    }
    if (us < 0) {
        us = 0;
    }
    return us;
}

// time since the previous lap, or since zero when no laps are stored
long current_lap_us(void)
{
    long us;

    us = elapsed_us();
    if (lap_count > 0) {
        us -= laps[lap_count - 1];
    }
    if (us < 0) {
        us = 0;
    }
    return us;
}

long lap_split_us(int idx)
{
    if (idx <= 0) {
        return laps[0];
    }
    return laps[idx] - laps[idx - 1];
}

// fastest lap in centiseconds, so laps that display the same time tie
long fastest_split_cs(void)
{
    int  i;
    long best;
    long split;

    if (lap_count < 2) {
        return -1;
    }
    best = lap_split_us(0) / 10000;
    for (i = 1; i < lap_count; i++) {
        split = lap_split_us(i) / 10000;
        if (split < best) {
            best = split;
        }
    }
    return best;
}

void format_parts(char *hms, int hms_len, char *frac, int frac_len, long us)
{
    long cs;
    long total_sec;
    int  hr, min, sec, hundredths;

    if (us < 0) {
        us = 0;
    }
    cs = us / 10000;
    hundredths = (int)(cs % 100);
    total_sec = cs / 100;
    if (total_sec >= 100L * 3600L) {
        snprintf(hms, hms_len, "99:59:59");
        snprintf(frac, frac_len, ".99");
        return;
    }
    sec = (int)(total_sec % 60);
    min = (int)((total_sec / 60) % 60);
    hr = (int)(total_sec / 3600);
    snprintf(hms, hms_len, "%02d:%02d:%02d", hr, min, sec);
    snprintf(frac, frac_len, ".%02d", hundredths);
}

void format_elapsed(char *str, int len, long us)
{
    char hms[16];
    char frac[8];

    format_parts(hms, (int)sizeof(hms), frac, (int)sizeof(frac), us);
    snprintf(str, len, "%s%s", hms, frac);
}

// -----------------  LAYOUT  ----------------------------------------

void layout_metrics(void)
{
    int time_h;
    int small_h;
    int normal_h;
    int cw;
    int panel_bottom;

    time_h = sdlx_char_height(TIME_FONT);
    small_h = sdlx_char_height(FONT_SMALL);
    normal_h = sdlx_char_height(FONT_NORMAL);
    cw = sdlx_char_width(FONT_SMALL);

    status_y = 48;
    time_cy = status_y + small_h + 24 + time_h / 2;
    lap_line_y = time_cy + time_h / 2 + 28;
    panel_bottom = lap_line_y + normal_h + 28;
    btn_y = panel_bottom + 40;
    header_y = btn_y + BTN_H + 48;
    row_h = small_h + 18;
    rows_y = header_y + small_h + 16;
    rows_bottom = sdlx_win_height - 12;

    // "#", "00:00:00.00", "00:00:00.00" with a one-character gap
    num_x = 32;
    split_x = num_x + 5 * cw;
    total_x = split_x + 12 * cw;
}

void clamp_scroll(void)
{
    int    view_h;
    int    content_h;
    double min_y;

    view_h = rows_bottom - rows_y;
    content_h = lap_count * row_h;
    if (content_h <= view_h) {
        scroll_y = 0;
        return;
    }
    min_y = (double)view_h - (double)content_h;
    if (scroll_y > 0) {
        scroll_y = 0;
    }
    if (scroll_y < min_y) {
        scroll_y = min_y;
    }
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
    char         hms[16];
    char         frac[8];
    char         lap_buf[16];
    char         lap_line[32];
    char        *state_str;
    char        *start_label;
    sdlx_color_t state_color;
    sdlx_color_t time_color;
    sdlx_color_t lap_color;
    sdlx_color_t start_bg;
    sdlx_color_t start_fg;
    long         elapsed;
    long         best_cs;
    int          hms_w;
    int          time_top;
    int          x0;
    int          panel_y;
    int          panel_h;
    int          i;
    int          idx;
    int          y;
    int          text_y;
    bool         reset_enabled;

    layout_metrics();
    elapsed = elapsed_us();

    if (running) {
        state_str = "RUNNING";
        state_color = COLOR_LIGHT_GREEN;
        time_color = COLOR_LIGHT_GREEN;
        lap_color = COLOR_LIGHT_BLUE;
    } else if (base_us == 0 && lap_count == 0) {
        state_str = "READY";
        state_color = COLOR_GRAY;
        time_color = COLOR_WHITE;
        lap_color = COLOR_GRAY;
    } else {
        state_str = "STOPPED";
        state_color = COLOR_ORANGE;
        time_color = COLOR_WHITE;
        lap_color = COLOR_GRAY;
    }

    panel_y = 24;
    panel_h = btn_y - panel_y - 28;
    sdlx_render_fill_rect(20, panel_y, sdlx_win_width - 40, panel_h, COLOR_DARK_GRAY);
    sdlx_render_rect(20, panel_y, sdlx_win_width - 40, panel_h, 3, COLOR_GRAY);

    sdlx_render_printf_ex(sdlx_win_width / 2, status_y,
                          FONT_SMALL, state_color, FLAG_X_CTR, "%s", state_str);

    format_parts(hms, (int)sizeof(hms), frac, (int)sizeof(frac), elapsed);
    hms_w = sdlx_char_width(TIME_FONT) * 8;
    x0 = (sdlx_win_width - hms_w - sdlx_char_width(FRAC_FONT) * 3) / 2;
    time_top = time_cy - sdlx_char_height(TIME_FONT) / 2;
    sdlx_render_printf_ex(x0, time_top, TIME_FONT, time_color, FLAG_NONE, "%s", hms);
    sdlx_render_printf_ex(x0 + hms_w, time_top + sdlx_char_height(TIME_FONT) - sdlx_char_height(FRAC_FONT),
                          FRAC_FONT, time_color, FLAG_NONE, "%s", frac);

    format_elapsed(lap_buf, (int)sizeof(lap_buf), current_lap_us());
    snprintf(lap_line, (int)sizeof(lap_line), "LAP %s", lap_buf);
    sdlx_render_printf_ex(sdlx_win_width / 2, lap_line_y,
                          FONT_NORMAL, lap_color, FLAG_X_CTR, "%s", lap_line);

    reset_enabled = (!running && (base_us > 0 || lap_count > 0));
    if (running) {
        start_label = "STOP";
        start_bg = COLOR_RED;
        start_fg = COLOR_WHITE;
    } else if (base_us == 0 && lap_count == 0) {
        start_label = "START";
        start_bg = COLOR_LIGHT_GREEN;
        start_fg = COLOR_BLACK;
    } else {
        start_label = "RESUME";
        start_bg = COLOR_LIGHT_GREEN;
        start_fg = COLOR_BLACK;
    }

    draw_button(0, start_label, start_bg, start_fg, EVID_START_STOP, true);
    draw_button(1, "LAP", COLOR_LIGHT_BLUE, COLOR_BLACK, EVID_LAP, running);
    draw_button(2, "RESET", COLOR_ORANGE, COLOR_BLACK, EVID_RESET, reset_enabled);

    sdlx_render_printf_ex(num_x, header_y, FONT_SMALL, COLOR_GRAY, FLAG_NONE, "#");
    sdlx_render_printf_ex(split_x, header_y, FONT_SMALL, COLOR_GRAY, FLAG_NONE, "LAP");
    sdlx_render_printf_ex(total_x, header_y, FONT_SMALL, COLOR_GRAY, FLAG_NONE, "TOTAL");
    sdlx_render_fill_rect(num_x, rows_y - 10, sdlx_win_width - num_x - 32, 3, COLOR_GRAY);

    best_cs = fastest_split_cs();
    if (lap_count == 0) {
        sdlx_render_printf_ex(sdlx_win_width / 2, rows_y + 36,
                              FONT_SMALL, COLOR_GRAY, FLAG_X_CTR,
                              "%s", "Tap LAP while running");
    }
    for (i = 0; i < lap_count; i++) {
        sdlx_color_t color;
        char         split_str[16];
        char         total_str[16];
        long         split_us;

        idx = lap_count - 1 - i;
        y = rows_y + i * row_h + (int)scroll_y;
        if (y < rows_y) {
            continue;
        }
        if (y + row_h > rows_bottom) {
            break;
        }

        if ((i % 2) == 0) {
            sdlx_render_fill_rect(20, y, sdlx_win_width - 40, row_h, COLOR_DARK_GRAY);
        }
        split_us = lap_split_us(idx);
        color = COLOR_WHITE;
        if (best_cs >= 0 && (split_us / 10000) == best_cs) {
            color = COLOR_LIGHT_GREEN;
        }
        text_y = y + (row_h - sdlx_char_height(FONT_SMALL)) / 2;
        format_elapsed(split_str, (int)sizeof(split_str), split_us);
        format_elapsed(total_str, (int)sizeof(total_str), laps[idx]);
        sdlx_render_printf_ex(num_x, text_y, FONT_SMALL, color, FLAG_NONE, "%2d", idx + 1);
        sdlx_render_printf_ex(split_x, text_y, FONT_SMALL, color, FLAG_NONE, "%s", split_str);
        sdlx_render_printf_ex(total_x, text_y, FONT_SMALL, color, FLAG_NONE, "%s", total_str);
    }

    reg_event_show_readme_file();
    sdlx_register_event(NULL, EVID_MOTION);
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
    case EVID_START_STOP:
        if (running) {
            base_us = elapsed_us();
            running = false;
            printf("I %s: stopped\n", progname);
        } else {
            run_start_us = util_microsec_timer();
            running = true;
            printf("I %s: running\n", progname);
        }
        sdlx_vibrate(VIBRATE_STRENGTH, VIBRATE_MS);
        highlight_evid = EVID_START_STOP;
        break;
    case EVID_LAP:
        if (running) {
            if (lap_count < MAX_LAPS) {
                laps[lap_count] = elapsed_us();
                lap_count++;
                scroll_y = 0;
                printf("I %s: lap %d\n", progname, lap_count);
            } else {
                sdlx_show_toast("Lap list is full");
            }
            sdlx_vibrate(VIBRATE_STRENGTH, VIBRATE_MS);
        }
        highlight_evid = EVID_LAP;
        break;
    case EVID_RESET:
        if (!running) {
            base_us = 0;
            lap_count = 0;
            scroll_y = 0;
            printf("I %s: reset\n", progname);
            sdlx_vibrate(VIBRATE_STRENGTH, VIBRATE_MS);
        }
        highlight_evid = EVID_RESET;
        break;
    case EVID_MOTION_BEGIN:
        scroll_drag = (event->u.motion_begin.y >= header_y);
        break;
    case EVID_MOTION:
        if (scroll_drag) {
            scroll_y += event->u.motion.yrel;
            clamp_scroll();
        }
        break;
    case EVID_MOTION_END:
        scroll_drag = false;
        break;
    }
    return false;
}
