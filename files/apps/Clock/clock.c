#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/Clock/common.h"
#include "apps/lib/lib.h"

// defines
#define XCTR_CLOCK 500
#define YCTR_CLOCK 600
#define W_CLOCK    1000
#define H_CLOCK    1000

// prototypes
static char *day_of_week(struct tm *tm);
static char *month(struct tm *tm);
static void draw_analog_clock_face(void);
static void draw_analog_clock_hands(struct tm *tm);
static void cleanup_analog_clock(void);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    sdlx_event_t    event;
    int             y;
    bool            quit = false;
    time_t          t;
    struct tm       tm;
    char            sunrise_calc[50], sunset_calc[50], midday_calc[50], daytime_calc[50];

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // calculate the sunrise, sunset, and midday (solar noon) times
    sunrise_sunset_calc(sunrise_calc, sunset_calc, midday_calc, daytime_calc);
    printf("I %s: CALC  %s %s %s %s\n", progname, sunrise_calc, midday_calc, sunset_calc, daytime_calc);

#if 0 // enable this to compare sunrise/sunset times from web api vs calculated
    // obtain, from web api, the sunrise, sunset, and midday (solar noon) times
    char sunrise_web[50], sunset_web[50], midday_web[50], daytime_web[50];
    sunrise_sunset_web(sunrise_web, sunset_web, midday_web, daytime_web);
    printf("I %s: WEB   %s %s %s %s\n", progname, sunrise_web, midday_web, sunset_web, daytime_web);
#endif

    // set default font
    sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);

    // runtime loop
    while (!quit) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // get the current time
        t = time(NULL);
        localtime_r(&t, &tm);

        // display the analog clock
        draw_analog_clock_face();
        draw_analog_clock_hands(&tm);

        // display the date and time below the analog clock, example:
        //   13:30:00 EDT
        //   Wed Oct 21 2025
        y = YCTR_CLOCK + H_CLOCK / 2 + 1.5 * sdlx_char_height_dflt;
        sdlx_render_printf_ex2(
                sdlx_win_width/2, y,
                FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, 
                "%02d:%02d:%02d %s",
                tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_zone);
        y += 1.5 * sdlx_char_height_dflt;
        sdlx_render_printf_ex2(
                sdlx_win_width/2, y, 
                FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, 
                "%s %s %d %d",
                day_of_week(&tm), month(&tm), tm.tm_mday, tm.tm_year+1900);
        y += 1.5 * sdlx_char_height_dflt;

        // display sunrise, midday, sunset times, example:
        //   RISE     MID      SET
        //   07:00   12:00   17:00
        sdlx_render_printf(sdlx_char_width_dflt/2, y, "RISE");
        sdlx_render_printf(sdlx_win_width/2-3*sdlx_char_width_dflt/2, y, "MID");
        sdlx_render_printf(sdlx_win_width-4*sdlx_char_width_dflt, y, "SET");
        y += 1.5 * sdlx_char_height_dflt;
        sdlx_render_printf(0, y, "%s", sunrise_calc);
        sdlx_render_printf(sdlx_win_width/2-5*sdlx_char_width_dflt/2, y, "%s", midday_calc);
        sdlx_render_printf(sdlx_win_width-5*sdlx_char_width_dflt, y, "%s", sunset_calc);
        y += 1.5 * sdlx_char_height_dflt;

        // display daytime length
        sdlx_render_printf_ex2(sdlx_win_width/2, y, FONT_NORMAL, COLOR_WHITE,
                               FLAG_X_CTR, 
                               "DayTime %s", daytime_calc);
    
        // register EVID_SHOW_README_FILE event
        reg_event_show_readme_file();

        // register control event to end program
        sdlx_register_control_events(0, NULL, 
                                     0, NULL, 
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for an event with 1 s timeout;
        // if no event, then redraw display
        sdlx_get_event(1000000, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_QUIT:
            quit = true;
            break;
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        }
    }

    // cleanup and end program
    cleanup_analog_clock();
    printf("I %s: terminating\n", progname);
    return 0;
}

static char *day_of_week(struct tm *tm)
{
    static char s[30];
    strftime(s, sizeof(s), "%a", tm);
    return s;
}

static char *month(struct tm *tm)
{
    static char s[30];
    strftime(s, sizeof(s), "%b", tm);
    return s;
}

// -----------------  ANALOG CLOCK  ----------------------------------

// - - - - - - face  - - - - - - - - - - - 

static void draw_analog_clock_face(void)
{
    int hour, x, y;

    sdlx_render_fill_rect(0, 100, 1000, 1000, COLOR_WHITE);

    for (hour = 1; hour <= 12; hour++) {
        x = XCTR_CLOCK + 400 * sin(hour * 30 * (M_PI / 180));
        y = YCTR_CLOCK - 400 * cos(hour * 30 * (M_PI / 180));
        sdlx_render_printf_ex2(
            x, y, 
            FONT_NORMAL, COLOR_BLACK, FLAG_XY_CTR, 
            "%d", hour);
    }
}

// - - - - - - hands - - - - - - - - - - - 

#define W_HH  34  // width of the hour-hand
#define H_HH  280 // height of the hour-hand
#define O_HH  40  // amount of the hour-hand that extends beyond the clock center

#define W_MH  17  // minute hand defines
#define H_MH  375
#define O_MH  60

#define W_SH  4   // second hand defines
#define H_SH  425
#define O_SH  100

sdlx_texture_t *hour_hand;
sdlx_texture_t *minute_hand;
sdlx_texture_t *second_hand;

static sdlx_texture_t * create_rectangle_texture(int w, int h, sdlx_color_t color);

static void draw_analog_clock_hands(struct tm *tm)
{
    static bool first_call = true;

    double hour_hand_angle, minute_hand_angle, second_hand_angle;
    long   secs;

    if (first_call) {
        hour_hand = create_rectangle_texture(W_HH, H_HH, COLOR_BLACK);
        minute_hand = create_rectangle_texture(W_MH, H_MH, COLOR_BLACK);
        second_hand = create_rectangle_texture(W_SH, H_SH, COLOR_RED);
        first_call = false;
    }

    secs = 3600 * tm->tm_hour + 60 * tm->tm_min + tm->tm_sec;

    hour_hand_angle   = secs * (360. / (12 * 3600));
    minute_hand_angle = secs * (360. / 3600);
    second_hand_angle = secs * (360. / 60);

    sdlx_render_texture_ex3(hour_hand,                                   // texture
                            XCTR_CLOCK-(W_HH/2), YCTR_CLOCK-H_HH+O_HH,   // x,y
                            W_HH, H_HH,                                  // w,h
                            hour_hand_angle,                             // angle
                            W_HH/2, H_HH-O_HH);                          // rotation center

    sdlx_render_texture_ex3(minute_hand,
                            XCTR_CLOCK-(W_MH/2), YCTR_CLOCK-H_MH+O_MH, 
                            W_MH, H_MH, 
                            minute_hand_angle, 
                            W_MH/2, H_MH-O_MH);

    sdlx_render_texture_ex3(second_hand,
                            XCTR_CLOCK-(W_SH/2), YCTR_CLOCK-H_SH+O_SH, 
                            W_SH, H_SH, 
                            second_hand_angle,
                            W_SH/2, H_SH-O_SH);

    sdlx_render_point(XCTR_CLOCK, YCTR_CLOCK, COLOR_RED, 9);
}
    
static sdlx_texture_t * create_rectangle_texture(int w, int h, sdlx_color_t color)
{
    sdlx_texture_t *t;

    t = sdlx_create_texture(w,h);
    sdlx_set_render_target(t);
    sdlx_render_fill_rect(0, 0, w, h, color);
    sdlx_set_render_target(NULL);

    return t;
}

// - - - - - - cleanup - - - - - - - - - - 

static void cleanup_analog_clock(void)
{
    sdlx_destroy_texture(hour_hand);
    sdlx_destroy_texture(minute_hand);
    sdlx_destroy_texture(second_hand);
}
