#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/lib/lib.h"

//
// defines
//

#define DEG2RAD              (M_PI / 180.0)
#define RAD2DEG              (180.0 / M_PI)

#define UPDATE_INTERVAL_SEC  0.01   // 10 ms

#define BALL_RADIUS          50
#define PADDLE_W             300
#define PADDLE_H             50

#define MIN_BALL_SPEED         0.5   // courts/sec
#define MAX_BALL_SPEED         3.0
#define DEFAULT_MIN_BALL_SPEED 1.0 
#define DEFAULT_MAX_BALL_SPEED 2.0

#define SKILL_EASY           0
#define SKILL_MEDIUM         1
#define SKILL_HARD           2

#define HUMAN_PADDLE         0
#define COMPUTER_PADDLE      1

#define EVID_START           1
#define EVID_PAUSE           2
#define EVID_CONT            3
#define EVID_RESET           4
#define EVID_SETTINGS        5

#define STATE_READY          0
#define STATE_SERVING        1
#define STATE_SERVING_DELAY  2
#define STATE_RUNNING        3
#define STATE_PAUSED         4

//
// variables
//

char           *progname;
char           *data_dir;

bool            param_autonomous;
bool            param_sound;
double          param_min_ball_speed;
double          param_max_ball_speed;
int             param_skill;

int             state;
double          y_top, y_bottom;
double          x, y, x_last, y_last;
double          vx, vy;
double          human_paddle_x, human_paddle_y;
double          computer_paddle_x, computer_paddle_y;
int             human_score, computer_score;
double          ball_speed_court_per_sec;
double          court_pixels;
sdlx_texture_t *ball;

char           *skill_str[3]       = { "easy", "medium", "hard" };
char           *short_skill_str[3] = { "E", "M", "H" };

//
// prototypes    
//

void run(void);
void paddle_control(int which_paddle);
void bounce_ball_off_paddle(int which_paddle);
void play_tone(int freq, int duration_ms);
double linear_interp(double v, double x1, double x2, double y1, double y2);

void init_settings(void);
void settings(void);
void clip(double *val, double min, double max);

// -----------------  MAIN  -----------------------
    
int main(int argc, char **argv)
{
    int          serving_delay = 0;
    sdlx_event_t event;
    long         start_us, timeout_us;
    bool         end_program = false;

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // initialization:
    // - settings
    init_settings();
    // - state
    state = STATE_READY;
    // - ball texture
    ball = sdlx_create_texture(2*BALL_RADIUS, 2*BALL_RADIUS);
    sdlx_set_render_target(ball);
    sdlx_render_fill_circle(BALL_RADIUS, BALL_RADIUS, BALL_RADIUS, COLOR_GREEN);
    sdlx_set_render_target(NULL);
    // - scores
    computer_score    = 0;
    human_score       = 0;
    // - y range of display for the full court,
    //   includes court area above and below the paddles
    y_top             = 0;
    y_bottom          = sdlx_win_height - 200;
    // - location of the centers of the paddles
    human_paddle_x    = sdlx_win_width/2;
    human_paddle_y    = y_bottom - 200;
    computer_paddle_x = sdlx_win_width/2;
    computer_paddle_y = y_top + 200;
    // - size of the court, between the paddles
    court_pixels = (human_paddle_y - computer_paddle_y);
    // - ball location and speed
    x = computer_paddle_x;
    y = computer_paddle_y + PADDLE_H/2 + BALL_RADIUS;
    ball_speed_court_per_sec = param_min_ball_speed;
    // - seed random number generation
    srandom(time(NULL));

    // runtime loop
    while (!end_program) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // if serve needed then reset variables
        if (state == STATE_SERVING) {
            double ball_speed_pixels_per_intvl, tgtx, k;

            // init ball location underneath the center of computer paddle
            x = computer_paddle_x;
            y = computer_paddle_y + PADDLE_H/2 + BALL_RADIUS;
            x_last = x;
            y_last = y;

            // determine ball speed in units of court/sec and pixels/interval
            ball_speed_court_per_sec = param_min_ball_speed;
            ball_speed_pixels_per_intvl = (court_pixels * UPDATE_INTERVAL_SEC) * ball_speed_court_per_sec;

            // choose a random direction to serve towards;
            // and cacluclate the required x and y velocities (vx,vy units are pixels/interval)
            tgtx = (random() % sdlx_win_width);
            k = (tgtx - x) / court_pixels;
            vy = ball_speed_pixels_per_intvl / sqrt(1 + k*k);
            vx = k * vy;

            // set state to cause a short delay prior to serving
            state = STATE_SERVING_DELAY;
            serving_delay = 50;
        }

        // casue short delay prior to serving
        if (state == STATE_SERVING_DELAY) {
            serving_delay--;
            if (serving_delay == 0) {
                state = STATE_RUNNING;
            }
        }

        // process ball and paddle motion
        if (state == STATE_RUNNING) {
            run();
        }

        // display scores
        sdlx_render_printf_ex1(0, 0, FONT_LARGE, COLOR_WHITE, "%d", computer_score);
        sdlx_render_printf_ex1(sdlx_win_width-2*sdlx_char_width(FONT_LARGE), 0,
                               FONT_LARGE, COLOR_WHITE, "%2d", human_score);

        // display ball speed, and skill setting
        sdlx_render_printf_ex2(sdlx_win_width/2, sdlx_char_height(FONT_NORMAL),
                               FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, 
                               "%0.2f %s", ball_speed_court_per_sec, short_skill_str[param_skill]);

        // display the ball and paddles
        sdlx_render_texture(ball, x-BALL_RADIUS, y-BALL_RADIUS);
        sdlx_render_fill_rect(human_paddle_x-PADDLE_W/2, human_paddle_y-PADDLE_H/2, PADDLE_W, PADDLE_H, COLOR_WHITE);
        sdlx_render_fill_rect(computer_paddle_x-PADDLE_W/2, computer_paddle_y-PADDLE_H/2, PADDLE_W, PADDLE_H, COLOR_WHITE);

        // register EVID_SHOW_README_FILE event
        reg_event_show_readme_file();

        // register game control events
        sdlx_register_event(NULL, EVID_MOTION);
        if (state == STATE_READY) {
            sdlx_register_control_events(EVID_START,    "START", 
                                         EVID_SETTINGS, "STG",   
                                         EVID_QUIT,     "X");
        } else if (state == STATE_SERVING || state == STATE_SERVING_DELAY || state == STATE_RUNNING) {
            sdlx_register_control_events(EVID_PAUSE,    "PAUSE", 
                                         EVID_RESET,    "RESET",   
                                         EVID_QUIT,     "X");
        } else if (state == STATE_PAUSED) {
            sdlx_register_control_events(EVID_CONT,     "CONT", 
                                         EVID_RESET,    "RESET",   
                                         EVID_QUIT,     "X");
        } else {
            printf("E %s: invalid state %d\n", progname, state);
        }

        // present the display
        sdlx_display_present();

        // process events for duration UPDATE_INTERVAL_SEC
        start_us = util_microsec_timer();
        while (true) {
            // calculate the timeout of wait for an event
            timeout_us = (UPDATE_INTERVAL_SEC * 1000000) - (util_microsec_timer() - start_us);

            // if calculated timeout is <= 0 then break out of the loop
            if (timeout_us <= 0) {
                //printf("I %s: INTVL %ld ms\n", progname, (util_microsec_timer() - start_us) / 1000);
                break;
            }

            // wait for an event, or timeout
            sdlx_get_event(timeout_us, &event);
            if (event.event_id == -1) {
                continue;
            }

            // process event, if received
            switch (event.event_id) {
            case EVID_QUIT:
                end_program = true;
                break;
            case EVID_START:
                state = STATE_SERVING;
                break;
            case EVID_PAUSE:
                state = STATE_PAUSED;
                break;
            case EVID_CONT:
                state = STATE_RUNNING;
                break;
            case EVID_RESET:
                computer_paddle_x        = sdlx_win_width/2;
                human_paddle_x           = sdlx_win_width/2;
                x                        = computer_paddle_x;
                y                        = computer_paddle_y + PADDLE_H/2 + BALL_RADIUS;
                ball_speed_court_per_sec = param_min_ball_speed;
                human_score              = 0;
                computer_score           = 0;
                state                    = STATE_READY;
                break;
            case EVID_SETTINGS:
                settings();
                break;
            case EVID_MOTION:
                human_paddle_x += event.u.motion.xrel;
                break;
            case EVID_SHOW_README_FILE:
                show_file(data_dir, "README");
                break;
            }
        }
    }

    // cleanup and end program
    sdlx_destroy_texture(ball);
    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  RUN -------------------------

void run(void)
{
    // update the ball velocity
    ball_speed_court_per_sec = sqrt(vx*vx + vy*vy) / (court_pixels * UPDATE_INTERVAL_SEC);
    if (ball_speed_court_per_sec < param_max_ball_speed) {
        vx *= 1.0002;
        vy *= 1.0002;
    }

    // update ball position, using the ball x,y velocity (vx, vy)
    x_last = x;
    y_last = y;
    x += vx;
    y += vy;

    // bounce ball off of side walls
    if (x < 0) {
        x = 0;
        if (vx < 0) vx = -vx;
    }
    if (x >= sdlx_win_width) {
        x = sdlx_win_width-1;
        if (vx > 0) vx = -vx;
    }

    // computer paddle control
    paddle_control(COMPUTER_PADDLE);

    // if autonomous mode then the computer will play the human paddle too
    if (param_autonomous) {
        paddle_control(HUMAN_PADDLE);
    }

    // if ball has impacted human paddle then
    // bounce ball off of human paddle
    double ball_bottom        = y + BALL_RADIUS;
    double ball_bottom_last   = y_last + BALL_RADIUS;
    double human_paddle_top   = human_paddle_y - (PADDLE_H/2);
    double human_paddle_left  = human_paddle_x - PADDLE_W/2 - BALL_RADIUS;
    double human_paddle_right = human_paddle_x + PADDLE_W/2 + BALL_RADIUS;
    if (ball_bottom >= human_paddle_top && 
        ball_bottom_last < human_paddle_top &&
        x >= human_paddle_left &&
        x <= human_paddle_right)
    {
        bounce_ball_off_paddle(HUMAN_PADDLE);
    }

    // if ball has impacted computer paddle then
    // bounce ball off of computer paddle
    double ball_top               = y - BALL_RADIUS;
    double ball_top_last          = y_last - BALL_RADIUS;
    double computer_paddle_bottom = computer_paddle_y + (PADDLE_H/2);
    double computer_paddle_left   = computer_paddle_x - PADDLE_W/2 - BALL_RADIUS;
    double computer_paddle_right  = computer_paddle_x + PADDLE_W/2 + BALL_RADIUS;
    if (ball_top <= computer_paddle_bottom && 
        ball_top_last > computer_paddle_bottom &&
        x >= computer_paddle_left &&
        x <= computer_paddle_right)
    {
        bounce_ball_off_paddle(COMPUTER_PADDLE);
    }

    // if ball is above or below the display area then 
    //   update score
    //   play tone
    //   set state to STATE_SERVING
    // endif
    if (y < y_top || y > y_bottom) {
        if (y < y_top) {
            human_score++; 
        } else {
            computer_score++;
        }
        play_tone(500,250);
        state = STATE_SERVING;
    }
}

static double skill_factor[3] = { 0.10, 0.14, 1.0 };

void paddle_control(int which_paddle)
{
    double paddle_offset, k;

    if (vx < 0) {
        paddle_offset = -PADDLE_W * (fabs(vx/vy) > 0.75 ? 0.5 : 0.25);
    } else {
        paddle_offset =  PADDLE_W * (fabs(vx/vy) > 0.75 ? 0.5 : 0.25);
    }

    k = linear_interp(ball_speed_court_per_sec, MIN_BALL_SPEED, MAX_BALL_SPEED, 0.10, 0.35);
    k *= skill_factor[param_skill];

    if (which_paddle == COMPUTER_PADDLE) {
        computer_paddle_x += ((x + paddle_offset) - computer_paddle_x) * k;
    } else {
        human_paddle_x += ((x + paddle_offset) - human_paddle_x) * k;
    }
}

// angle of the paddle at its ends, in radians
#define MAX_PADDLE_ANGLE (30.0 * DEG2RAD)

void bounce_ball_off_paddle(int which_paddle)
{
    double vx1, vy1, theta;
    double paddle_x;

    // The paddles are implemented curved. The center of the paddle is
    // oriented as the paddle is displayed. The far left and right ends of the paddles
    // are oriented with angle (theta) of 30 degrees. The orientation angle varies
    // smoothly between the center and the left/right ends.

    // determine the paddle orientation angle at the location where the ball has hit
    if (which_paddle == HUMAN_PADDLE) {
        paddle_x = human_paddle_x;
        theta = (paddle_x - x) / (PADDLE_W/2) * MAX_PADDLE_ANGLE;
    } else {
        paddle_x = computer_paddle_x;
        theta = -(paddle_x - x) / (PADDLE_W/2) * MAX_PADDLE_ANGLE;
    }

    // rotate the ball velocity vector by angle theta
    vx1 = vx*cos(theta) - vy*sin(theta);
    vy1 = vx*sin(theta) + vy*cos(theta);

    // bounce the ball in the rotated reference frame
    vy1 = -vy1;

    // rotate the ball velocity back to the original reference frame
    theta = -theta;
    vx = vx1*cos(theta) - vy1*sin(theta);
    vy = vx1*sin(theta) + vy1*cos(theta);

    // if the ball x velocity is larger than the ball y velocity then
    // the ball will be bouncing off the walls too much; 
    // in this case the x and y velocities are set equal which sets the
    // ball direction to 45 degrees
    if (fabs(vx) > fabs(vy)) {
        double new_vxy = sqrt((vx*vx + vy*vy) / 2);

        vx = (vx > 0 ? new_vxy : -new_vxy);
        vy = (vy > 0 ? new_vxy : -new_vxy);

        if (which_paddle == HUMAN_PADDLE && vy > 0) vy = -vy;
        if (which_paddle == COMPUTER_PADDLE && vy < 0) vy = -vy;
    }

    // play short (100ms) tone indicating the ball has bounced off the paddle
    play_tone(1000,100);
}

void play_tone(int freq, int duration_ms)
{
    static sdlx_tone_t t[2];

    if (!param_sound) {
        return;
    }

    t[0].freq = freq;
    t[0].intvl_ms = duration_ms;
    sdlx_audio_play_tones(t);
}

double linear_interp(double v, double x1, double x2, double y1, double y2)
{
    return y1 + (v - x1) * (y2 - y1) / (x2 - x1);
}

// -----------------  SETTINGS  -------------------

#define EVID_AUTONOMOUS     1
#define EVID_SOUND          2
#define EVID_MIN_BALL_SPEED 3
#define EVID_MAX_BALL_SPEED 4
#define EVID_SKILL          5

void init_settings(void)
{
    param_autonomous     = util_get_numeric_param(data_dir, "autonomous", 0);
    param_sound          = util_get_numeric_param(data_dir, "sound", 1);
    param_min_ball_speed = util_get_numeric_param(data_dir, "min_ball_speed", DEFAULT_MIN_BALL_SPEED);
    param_max_ball_speed = util_get_numeric_param(data_dir, "max_ball_speed", DEFAULT_MAX_BALL_SPEED);
    param_skill          = util_get_numeric_param(data_dir, "skill", SKILL_HARD);
}

void settings(void)
{
    bool done = false;
    sdlx_event_t event;
    sdlx_loc_t *loc;
    char *str;
    char prompt[100];

    while (!done) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // display values, and register events to change the values
        loc = sdlx_render_printf_ex1(0, ROW2Y(2), FONT_NORMAL, COLOR_LIGHT_BLUE, "autonomous = %s", param_autonomous ? "ON" : "OFF");
        sdlx_register_event(loc, EVID_AUTONOMOUS);
        loc = sdlx_render_printf_ex1(0, ROW2Y(4), FONT_NORMAL, COLOR_LIGHT_BLUE, "sound = %s", param_sound ? "ON" : "OFF");
        sdlx_register_event(loc, EVID_SOUND);
        loc = sdlx_render_printf_ex1(0, ROW2Y(6), FONT_NORMAL, COLOR_LIGHT_BLUE, "min_speed = %G", param_min_ball_speed);
        sdlx_register_event(loc, EVID_MIN_BALL_SPEED);
        loc = sdlx_render_printf_ex1(0, ROW2Y(8), FONT_NORMAL, COLOR_LIGHT_BLUE, "max_speed = %G", param_max_ball_speed);
        sdlx_register_event(loc, EVID_MAX_BALL_SPEED);
        loc = sdlx_render_printf_ex1(0, ROW2Y(10), FONT_NORMAL, COLOR_LIGHT_BLUE, "%s", skill_str[param_skill]);
        sdlx_register_event(loc, EVID_SKILL);

        // register control event to exit the settings screen
        sdlx_register_control_events(0, NULL, 
                                     0, NULL, 
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for an event, with infinite timeout
        sdlx_get_event(-1, &event);

        // process event
        switch (event.event_id) {
        case EVID_AUTONOMOUS:
            param_autonomous = !param_autonomous;
            util_set_numeric_param(data_dir, "autonomous", param_autonomous);
            break;
        case EVID_SOUND:
            param_sound = !param_sound;
            util_set_numeric_param(data_dir, "sound", param_sound);
            break;
        case EVID_MIN_BALL_SPEED:
            sprintf(prompt, "min_ball_speed\n%.1f-%.1f", MIN_BALL_SPEED, MAX_BALL_SPEED);
            str = sdlx_get_input_str(prompt, true, NULL);
            sscanf(str, "%lf", &param_min_ball_speed);
            clip(&param_min_ball_speed, MIN_BALL_SPEED, MAX_BALL_SPEED);
            ball_speed_court_per_sec = param_min_ball_speed;
            util_set_numeric_param(data_dir, "min_ball_speed", param_min_ball_speed);
            break;
        case EVID_MAX_BALL_SPEED:
            sprintf(prompt, "max_ball_speed\n%.1f-%.1f", MIN_BALL_SPEED, MAX_BALL_SPEED);
            str = sdlx_get_input_str(prompt, true, NULL);
            sscanf(str, "%lf", &param_max_ball_speed);
            clip(&param_max_ball_speed, MIN_BALL_SPEED, MAX_BALL_SPEED);
            util_set_numeric_param(data_dir, "max_ball_speed", param_max_ball_speed);
            break;
        case EVID_SKILL:
            if (++param_skill > SKILL_HARD) {
                param_skill = SKILL_EASY;
            }
            util_set_numeric_param(data_dir, "skill", param_skill);
            break;
        case EVID_QUIT:
            done = true;
            break;
        }
    }
}

void clip(double *val, double min, double max)
{
    if (*val < min) {
        *val = min;
    } else if (*val > max) {
        *val = max;
    }
}
