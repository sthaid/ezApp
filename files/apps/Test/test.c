#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <libgen.h>
#include <errno.h>
#include <fcntl.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/Test/common.h"
#include "apps/lib/lib.h"
#include "svcs/Template/template.h"

//
// defines
//

//
// variables
//

static bool  end_program;
static int   orientation;

//
// prototypes
//

static void page_hndlr(void);

static void page_0_draw(void);
static void page_0_process_event(sdlx_event_t *event);

static void page_1_draw(void);

static void page_2_draw(void);

static void page_3_init(void);
static void page_3_draw(void);
static void page_3_process_event(sdlx_event_t *event);
static void page_3_exit(void);

static void page_4_draw(void);

static void page_5_init(void);
static void page_5_draw(void);
static void page_5_exit(void);

static void page_6_draw(void);

static void page_7_init(void);
static void page_7_draw(void);
static void page_7_process_event(sdlx_event_t *event);
static void page_7_exit(void);

static void page_8_init(void);
static void page_8_draw(void);
static void page_8_process_event(sdlx_event_t *event);
static void page_8_exit(void);

static void page_9_init(void);
static void page_9_draw(void);

static void page_10_draw(void);

static void page_11_draw(void);
static void page_11_process_event(sdlx_event_t *event);

static void page_12_draw(void);
static void page_12_process_event(sdlx_event_t *event);

static void page_13_init(void);
static void page_13_draw(void);
static void page_13_exit(void);

static void page_14_init(void);
static void page_14_draw(void);
static void page_14_process_event(sdlx_event_t *event);
static void page_14_exit(void);

static void page_15_init(void);
static void page_15_draw(void);
static void page_15_process_event(sdlx_event_t *event);
static void page_15_exit(void);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // print window and char sized, these are global variables from sdl.c;
    // the initial char size provides 20 chars across the display width
    printf("I %s: sdlx_win_width/height  = %d %d\n", progname, sdlx_win_width, sdlx_win_height);
    printf("I %s: sdlx_char_width/height = %d %d\n", progname, sdlx_char_width_dflt, sdlx_char_height_dflt);

    // test calling a routine that is defined in another file
    test1_proc();

    // test calling lib proc
    orientation = get_device_orientation();
    printf("I %s: orientation is %s\n",
           progname,
           (orientation == PORTRAIT ? "PORTRAIT" : "LANDSCAPE"));

    // test reading a file in the 'data_dir' dir
    int file_len;
    char *file_content = util_read_file(data_dir, "common.h", &file_len);
    if (file_content == NULL) {
        printf("E %s: failed to read file common.h\n", data_dir);
    } else {
        printf("I %s: read file common.h okay, file_len = %d\n", data_dir, file_len);
    }

    // toast test
    sdlx_show_toast("TOAST TEST");

    // call handler routine for the current page
    while (true) {
        page_hndlr();
        if (end_program) {
            break;
        }
    }

    // return success
    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  SUPPORT PROCS FOR ALL PAGES  ------------

// NOTE: picoc does not support this being static, causes crash;
//      if declared static, the number of array elements must be provided
char *page_title[] = {     // Page
        "TEST",            //   0
        "Font",            //   1
        "Sizeof",          //   2
        "Multi Lines",     //   3
        "Drawing",         //   4
        "Textures",        //   5
        "Colors",          //   6
        "Audio",           //   7
        "Sensor Info",     //   8
        "Sensor Values",   //   9
        "Location",        //  10
        "Text Rotate",     //  11
        "Landscape",       //  12
        "SvcMakeReq",      //  13
        "Camera",          //  14
        "Pinch",           //  15
            };
static int pagenum = 0;

#define LAST_PAGE (sizeof(page_title)/sizeof(char*)-1)

#define EVID_PREV_PAGE 1
#define EVID_NEXT_PAGE 2

static void page_hndlr()
{
    sdlx_event_t event;
    int         new_pagenum = -1;

    // call the page specific init routine, if provided
    switch (pagenum) {
    case 3: page_3_init(); break;
    case 5: page_5_init(); break;
    case 7: page_7_init(); break;
    case 8: page_8_init(); break;
    case 9: page_9_init(); break;
    case 13: page_13_init(); break;
    case 14: page_14_init(); break;
    case 15: page_15_init(); break;
    }

    while (true) {
        // init the backbuffer, and print font/color
        // xxx comments
        if ((pagenum == 12) || 
            (pagenum == 14 && get_device_orientation() == LANDSCAPE) ||
            (pagenum == 15 && get_device_orientation() == LANDSCAPE)
                )
        {
            orientation = LANDSCAPE;
        } else {
            orientation = PORTRAIT;
        }

        sdlx_display_init(COLOR_BLACK, orientation);
        sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);

        // draw title line
        sdlx_render_printf_ex2(sdlx_win_width/2, 50, FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, "%s", page_title[pagenum]);

        // draw display
        switch (pagenum) {
        case 0: page_0_draw(); break;
        case 1: page_1_draw(); break;
        case 2: page_2_draw(); break;
        case 3: page_3_draw(); break;
        case 4: page_4_draw(); break;
        case 5: page_5_draw(); break;
        case 6: page_6_draw(); break;
        case 7: page_7_draw(); break;
        case 8: page_8_draw(); break;
        case 9: page_9_draw(); break;
        case 10: page_10_draw(); break;
        case 11: page_11_draw(); break;
        case 12: page_12_draw(); break;
        case 13: page_13_draw(); break;
        case 14: page_14_draw(); break;
        case 15: page_15_draw(); break;
        default:
            printf("E %s: invalid pagenum %d\n", progname, pagenum);
            end_program = true;
            return;
        }

        // register EVID_SHOW_README_FILE event
        reg_event_show_readme_file();

        // register control events
        // "<" - previous page
        // ">" - next page
        // 'X' - end prorgram
        sdlx_register_control_events(EVID_PREV_PAGE, "<",
                                     EVID_NEXT_PAGE, ">",
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for an event with 50 ms timeout;
        // if no event available, then redraw display
        sdlx_get_event(50000, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process common events
        switch (event.event_id) {
        case EVID_QUIT:
            end_program = true;
            break;      
        case EVID_PREV_PAGE:
            new_pagenum = pagenum - 1;
            if (new_pagenum < 0) {
                new_pagenum = LAST_PAGE;
            }
            break;      
        case EVID_NEXT_PAGE:
            new_pagenum = pagenum + 1;
            if (new_pagenum > LAST_PAGE) {
                new_pagenum = 0;
            }
            break;      
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        }

        // if the page has been changed or the program is terminating
        // then break out of the loop
        if (new_pagenum != -1 || end_program) {
            break;
        }

        // it wasn't a common event;
        // call the page specific event hndlr, if provided
        switch (pagenum) {
        case 0: page_0_process_event(&event); break;
        case 3: page_3_process_event(&event); break;
        case 7: page_7_process_event(&event); break;
        case 8: page_8_process_event(&event); break;
        case 11: page_11_process_event(&event); break;
        case 12: page_12_process_event(&event); break;
        case 14: page_14_process_event(&event); break;
        case 15: page_15_process_event(&event); break;
        }
    }

    // call the page specific exit routine, if provided
    switch (pagenum) {
    case 3: page_3_exit(); break;
    case 5: page_5_exit(); break;
    case 7: page_7_exit(); break;
    case 8: page_8_exit(); break;
    case 13: page_13_exit(); break;
    case 14: page_14_exit(); break;
    case 15: page_15_exit(); break;
    }

    // update pagenum
    pagenum = new_pagenum;
}

// -----------------  PAGE 0: CLOCK  --------------------------

#define EVID_CRASH 10

int page_0_x, page_0_y;
double page_0_xrel, page_0_yrel;

static void page_0_draw(void)
{
    time_t t;
    struct tm *tm;
    char str[MAX_TIME_STR];
    long usecs, delta_ms;
    char ipaddr_str[30];
    static long usecs_last, usecs_first;
    
    // draw rect around sdlx_win perimeter
    sdlx_render_rect(0, 0, sdlx_win_width, sdlx_win_height, 3, COLOR_BLUE);

    // print the time, hh:mm:ss
    time(&t);
    tm = localtime(&t);
    sprintf(str, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
    sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(5), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, "%s", str);

    // print the time in microsecs
    usecs = util_get_real_time_microsec();
    util_time2str(str, usecs, false, true, false);
    sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(7), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, "%s", str);

    // print microsecs since this page is first viewed, and
    // print the delta time since last display update
    usecs = util_microsec_timer();
    if (usecs_first == 0) {
        usecs_first = usecs;
    }
    delta_ms = (usecs - usecs_last) / 1000;
    usecs_last = usecs;
    sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(9), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, "%0.3f delta=%ld ms", 
        (usecs-usecs_first)/1000000., delta_ms);

    // print ipaddr
    sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(11), 
                           FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
                           "%s", util_get_ipaddr(ipaddr_str));

    // test mouse motion
    sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(13),
                           FONT_NORMAL, COLOR_WHITE,
                           FLAG_X_CTR, 
                           "WxH = %d %d", sdlx_win_width, sdlx_win_height);
    sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(14), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
           "xrel = %0.3f", page_0_xrel);
    sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(15), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
           "yrel = %0.3f", page_0_yrel);
    sdlx_render_point(page_0_x, page_0_y, COLOR_WHITE, 9);
    sdlx_register_event(NULL, EVID_MOTION);

    // register event to crash this app
    sdlx_loc_t *loc = sdlx_render_printf_ex1(0, sdlx_win_height-2*sdlx_char_height_dflt, 
                                             FONT_NORMAL, COLOR_LIGHT_BLUE, "CRASHME");
    sdlx_register_event(loc, EVID_CRASH);
}

static void page_0_process_event(sdlx_event_t *ev)
{
    switch (ev->event_id) {
    case EVID_MOTION:
        page_0_x = nearbyint(ev->u.motion.x);
        page_0_y = nearbyint(ev->u.motion.y);
        page_0_xrel = ev->u.motion.xrel;
        page_0_yrel = ev->u.motion.yrel;
        break;
    case EVID_CRASH: {
        static int *null_ptr = (void*)1;
        char *yn = sdlx_get_input_str("Crash EZAPP (y/n)", false, NULL);
        bool do_crash = (strcasecmp(yn, "y") == 0);
        sdlx_show_toast(do_crash ? "CRASHING EZAPP" : "ABORTED");
        if (do_crash) {
            printf("I %s: dereference invalid ptr\n", progname);
            printf("I %s: %d\n", progname, *null_ptr);
        }
        break; }
    }
}

// -----------------  PAGE 1: FONT  ---------------------------

static void page_1_draw(void)
{
    int i, ch=0;
    char str[32];

    for (i = 0; i < 16; i++) {
        sprintf(str, "%02x %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
                i*16,
                ch+0, ch+1, ch+2, ch+3, ch+4, ch+5, ch+6, ch+7,
                ch+8, ch+9, ch+10, ch+11, ch+12, ch+13, ch+14, ch+15);

        sdlx_render_printf(0, ROW2Y(i+2), "%s", str);
        ch += 16;
    }
}

// -----------------  PAGE 2: SIZEOF  -------------------------

static void page_2_draw(void)
{
    int r = 2;

    sdlx_render_printf(0, ROW2Y(r++), "sizoef(char)   = %zd", sizeof(char));
    sdlx_render_printf(0, ROW2Y(r++), "sizoef(short)  = %zd", sizeof(short));
    sdlx_render_printf(0, ROW2Y(r++), "sizoef(int)    = %zd", sizeof(int));
    sdlx_render_printf(0, ROW2Y(r++), "sizoef(long)   = %zd", sizeof(long));
    sdlx_render_printf(0, ROW2Y(r++), "sizoef(bool)   = %zd", sizeof(bool));
    sdlx_render_printf(0, ROW2Y(r++), "sizoef(size_t) = %zd", sizeof(size_t));
    sdlx_render_printf(0, ROW2Y(r++), "sizoef(off_t)  = %zd", sizeof(off_t));
    sdlx_render_printf(0, ROW2Y(r++), "sizoef(time_t) = %zd", sizeof(time_t));
    sdlx_render_printf(0, ROW2Y(r++), "sizoef(clock_t)= %zd", sizeof(clock_t));
    sdlx_render_printf(0, ROW2Y(r++), "sizoef(float)  = %zd", sizeof(float));
    sdlx_render_printf(0, ROW2Y(r++), "sizoef(double) = %zd", sizeof(double));
    sdlx_render_printf(0, ROW2Y(r++), "sizeof(1)      = %zd", sizeof(1));
    sdlx_render_printf(0, ROW2Y(r++), "sizeof(1L)     = %zd", sizeof(1L));
}

// -----------------  PAGE 3: MULTI LINE TEXT  ----------------

#define MAX_LINES 20

static double x_mlt;
static double y_mlt;
static int    y_mlt_top;
static int    y_mlt_bottom;
static char  *lines[MAX_LINES];

static void page_3_init(void)
{
    for (int i = 0; i < MAX_LINES; i++) {
        lines[i] = malloc(50);
        sprintf(lines[i], "Line %d <cr>\n  Hello World abcdefghijklmnopqrstuvwxyz\n", i);
    }

    x_mlt = 0;
    y_mlt = ROW2Y(2); 
    y_mlt_top = ROW2Y(2);
    y_mlt_bottom = sdlx_win_height-3*sdlx_char_height_dflt;
}

static void page_3_draw(void)
{
    sdlx_register_event(NULL, EVID_MOTION);

    sdlx_render_multiline_text(x_mlt, y_mlt, y_mlt_top, y_mlt_bottom, FONT_NORMAL, lines, NULL, MAX_LINES);
}

static void page_3_process_event(sdlx_event_t *event)
{
    if (event->event_id == EVID_MOTION) {
        y_mlt += event->u.motion.yrel;
        if (y_mlt >= y_mlt_top) y_mlt = y_mlt_top;

        x_mlt += event->u.motion.xrel;
        if (x_mlt > 0)  x_mlt = 0;
    }
}

static void page_3_exit(void)
{
    for (int i = 0; i < MAX_LINES; i++) {
        free(lines[i]);
        lines[i] = NULL;
    }
}

// -----------------  PAGE 4: DRAWING  ------------------------

static void add_point(sdlx_point_t **p, int x, int y);

static void page_4_draw(void)
{
    // draw rect around perimeter
    sdlx_render_rect(0, 0, sdlx_win_width, sdlx_win_height, 2, COLOR_PURPLE);

    // draw fill rect, y = 170 .. 400
    sdlx_render_fill_rect(100, 170, 800, 230, COLOR_RED);

    // draw circles, y = 400 .. 500
    sdlx_render_circle(1*sdlx_win_width/4, 450, 50, 3, COLOR_YELLOW);
    sdlx_render_circle(2*sdlx_win_width/4, 450, 50, 3, COLOR_YELLOW);
    sdlx_render_circle(3*sdlx_win_width/4, 450, 50, 3, COLOR_YELLOW);

    // draw 6 lines, y = 500 .. 600
    for (int y = 500; y <= 600; y += 20) {
        sdlx_render_line(0, y, 1000, y, COLOR_WHITE);
    }

    // draw 3 lines to make a triangle, y = 600 .. 800
    sdlx_point_t pts[4], *ptsx=pts;
    add_point(&ptsx, 500, 600);
    add_point(&ptsx, 700, 800);
    add_point(&ptsx, 300, 800);
    add_point(&ptsx, 500, 600);
    sdlx_render_lines(pts, 4, COLOR_RED);

    // draw 2 squares and vary intensity and wavelen, y = 800 .. 900
    static double inten;
    sdlx_color_t color;
    inten = inten + 0.01;
    if (inten > 1) inten = 0;
    color = sdlx_scale_color(COLOR_YELLOW, inten);
    sdlx_render_fill_rect(100, 800, 100, 100, color);

    static double wavelen = 750;
    wavelen -= 2;
    if (wavelen < 440) wavelen = 750;
    color = sdlx_wavelength_to_color(wavelen);
    sdlx_render_fill_rect(800, 800, 100, 100, color);

    // draw points with varying size, y = 1000
    color = sdlx_create_color(0, 255, 0, 255);
    for (int pointsize = 0; pointsize <= MAX_POINT_SIZE; pointsize++) {
        sdlx_render_point(pointsize*100+50, 1000, color, pointsize);
    }

    // draw 10 points of the same size, y = 1100
    sdlx_point_t points[10];
    for (int i = 0; i < 10; i++) {
        points[i].x = i*100+50;
        points[i].y = 1100;
    }
    sdlx_render_points(points, 10, COLOR_PURPLE, 5);

    // draw filled circle, y_ctr = 1200 + radius = 1200 + 250 = 1450
    sdlx_render_fill_circle(sdlx_win_width/2, 1450, 250, COLOR_PURPLE);
}

static void add_point(sdlx_point_t **p, int x, int y)
{
    (*p)->x = x;
    (*p)->y = y;
    (*p)++;
}

// -----------------  PAGE 5: TEXTURES  -----------------------

static sdlx_texture_t *texture1;
static sdlx_texture_t *texture2;

static void page_5_init(void)
{
    int w, h, i;

    //
    // test set/get textrue pixels
    //

    // create texture2, and set its pixels
    unsigned int *pixels;
    texture2 = sdlx_create_texture(200, 200);
    pixels = malloc(200*200*BYTES_PER_PIXEL);
    for (i = 0; i < 200*200; i++) {
        pixels[i] = COLOR_BLUE;
    }
    sdlx_set_texture_pixels(texture2, pixels);
    free(pixels);

    // get the pixels from texture2
    pixels = sdlx_get_texture_pixels(texture2, &w, &h);
    printf("I %s: get_pixels ret w,h %d %d\n", progname, w, h);
    if (w != 200 || h != 200) {
        printf("E %s: incorrect w,h (%d,%d)  returned from sdlx_get_texture_pixels\n", progname, w, h);
        free(pixels);
        return;
    }

    // verify pixels read back
    for (i = 0; i < w*h; i++) {
        if (pixels[i] != COLOR_BLUE) {
            printf("E %s: incorrect pixel value returned, pixels[%d]=%08x\n", progname, i, pixels[i]);
            break;
        }
    }
    if (i == w*h) {
        printf("I %s: pixel readback test, okay\n", progname);
    }

    // free pixels
    free(pixels);
}

static void page_5_exit(void)
{
    printf("I %s: destroying textures\n", progname);
    sdlx_destroy_texture(texture1);
    sdlx_destroy_texture(texture2);
}

static void page_5_draw(void)
{
    int w, h;

    // note: creating texture1 here is not efficient, the texture would 
    // normally be initialized just once; it is done this way here for testing

    // create texture1
    texture1 = sdlx_create_texture(1000, 1000);
    // - query texture1, and validate
    sdlx_query_texture(texture1, &w, &h);
    // - set render target to texture1
    sdlx_set_render_target(texture1);
    // - draw to texture1
    sdlx_render_rect(0, 0, w, h, 5, COLOR_WHITE);
    sdlx_render_fill_circle(w/2, h/2, w/2, COLOR_YELLOW);
    sdlx_render_printf_ex2(w/2, h/2, FONT_NORMAL, COLOR_RED, FLAG_XY_CTR, "%s", "Hello");
    // - set render target back to the display
    sdlx_set_render_target(NULL);

    // render textur1 to the display coords 0,200, without scaling
    sdlx_render_texture(texture1, 0, 200);

    // render texture1 to the display coords 0, 1300, scaling to half size
    sdlx_render_texture_ex1(texture1, 0, 1300, 500, 500);

    // render texture 2, a blue square, just to the right of the 
    // previous rendering of textur1
    sdlx_render_texture(texture2, 500, 1300);

    // destroy texture1
    sdlx_destroy_texture(texture1);
    texture1 = NULL;
}

// -----------------  PAGE 6: COLORS  -------------------------

static void color_test(int idx, char *color_name, sdlx_color_t color);
static void alpha_test(int idx, char *test_name, sdlx_color_t bg_color, sdlx_color_t fg_color);

static void page_6_draw(void)
{
    int idx = 0;

    color_test(idx++, "WHITE", COLOR_WHITE);
    color_test(idx++, "RED",   COLOR_RED);
    color_test(idx++, "ORANGE", COLOR_ORANGE);
    color_test(idx++, "YELLOW", COLOR_YELLOW);
    color_test(idx++, "GREEN", COLOR_GREEN);
    color_test(idx++, "BLUE", COLOR_BLUE);
    color_test(idx++, "INDIGO", COLOR_INDIGO);
    color_test(idx++, "VIOLET", COLOR_VIOLET);
    color_test(idx++, "PURPLE", COLOR_PURPLE);
    color_test(idx++, "LIGHT_BLUE", COLOR_LIGHT_BLUE);
    color_test(idx++, "LIGHT_GREEN", COLOR_LIGHT_GREEN);
    color_test(idx++, "PINK", COLOR_PINK);
    color_test(idx++, "TEAL", COLOR_TEAL);
    color_test(idx++, "LIGHT_GRAY", COLOR_LIGHT_GRAY);
    color_test(idx++, "GRAY", COLOR_GRAY);
    color_test(idx++, "DARK_GRAY", COLOR_DARK_GRAY);
    alpha_test(idx++, "ALPHA_TEST", COLOR_WHITE, COLOR_BLUE);
}

static void color_test(int idx, char *color_name, sdlx_color_t color)
{
    int y = 2 * sdlx_char_height_dflt + idx * 100;

    sdlx_render_printf(0, y, "%s", color_name);
    sdlx_render_fill_rect(500, y, 500, sdlx_char_height_dflt, color);
}

static void alpha_test(int idx, char *test_name, sdlx_color_t bg_color, sdlx_color_t fg_color)
{
    int y = 2 * sdlx_char_height_dflt + idx * 100;
    int alpha, x;
    sdlx_color_t color;

    sdlx_render_printf(0, y, "%s", test_name);
    sdlx_render_fill_rect(500, y, 500, sdlx_char_height_dflt, bg_color);

    for (x = 500; x < 1000; x+=2) {
        alpha = (x - 500) / 2;  // will range from 0 to 249
        color = sdlx_set_color_alpha(fg_color, alpha);
        sdlx_render_line(x, y, x, y+sdlx_char_height_dflt, color);
        sdlx_render_line(x+1, y, x+1, y+sdlx_char_height_dflt, color);
    }

}

// -----------------  PAGE 7: AUDIO  --------------------------

#define EVID_AUDIO_STOP                    10
#define EVID_AUDIO_PAUSE                   11
#define EVID_AUDIO_RESUME                  12
#define EVID_AUDIO_PLAY_TONE_GO            13
#define EVID_AUDIO_PLAY_TONE_GET_FREQ      14
#define EVID_AUDIO_PLAY_TONE_FREQ_UP       15
#define EVID_AUDIO_PLAY_TONE_FREQ_DOWN     16
#define EVID_AUDIO_PLAY_TONE_CHAN_LRB      17
#define EVID_AUDIO_RECORD_FROM_MIC         18
#define EVID_AUDIO_RECORD_FROM_MIC_APPEND  19
#define EVID_AUDIO_RECORD_FROM_DEV         20
#define EVID_AUDIO_PLAY_MONO_BUFF          21
#define EVID_AUDIO_PLAY_STEREO_BUFF        22
#define EVID_AUDIO_PLAY_TONES_SEQUENCE     23
#define EVID_AUDIO_PLAY_WHITE_NOISE        24
#define EVID_AUDIO_PLAY_MIC_MP3            25
#define EVID_AUDIO_PLAY_DEV_MP3            26

#define TONE_BOTH_CHANNELS 0
#define TONE_LEFT_CHANNEL  1
#define TONE_RIGHT_CHANNEL 2

#define MIN_TONE_FREQ  100
#define MAX_TONE_FREQ  6000

#define TWO_PI  (2.0 * M_PI)

static int tone_freq = 1000;
static int tone_lrb = TONE_BOTH_CHANNELS;

static char *audio_state_str(sdlx_audio_state_t *as, bool *is_recording)
{
    char total_secs_str[40];
    static char str[60];

    *is_recording = false;

    switch (as->state) {
    case AUDIO_STATE_IDLE:
        return "IDLE";
    case AUDIO_STATE_PLAY_FILE:
    case AUDIO_STATE_PLAY_TONES_SEQUENCE:
    case AUDIO_STATE_PLAY_BUFF:
        if (as->play_total_secs == 0) {
            sprintf(total_secs_str, "INF");
        } else {
            sprintf(total_secs_str, "%d", as->play_total_secs);
        }
        sprintf(str, "%s %d %s",
                (as->state == AUDIO_STATE_PLAY_FILE ? "PLAY_FILE" :
                 (as->state == AUDIO_STATE_PLAY_TONES_SEQUENCE ? "PLAY_TONES" : "PLAY_BUFF")),
                as->play_current_secs,
                total_secs_str);
        return str;
    case AUDIO_STATE_RECORD_FROM_MIC:
    case AUDIO_STATE_RECORD_FROM_DEVICE:
        if (!as->paused) {
            sprintf(str, "%s %d",
                    as->state == AUDIO_STATE_RECORD_FROM_MIC ? "REC_MIC" : "REC_DEVICE",
                    as->record_secs);
            *is_recording = true;
        } else {
            sprintf(str, "%s %d",
                    as->state == AUDIO_STATE_RECORD_FROM_MIC ? "MON_MIC" : "MON_DEVICE",
                    as->record_secs);
        }
        return str;
    default:
        return "INVLD";
    }
}

static void add_tone(sdlx_tone_t **t, int freq, int intvl_ms)
{       
    (*t)->freq = freq;
    (*t)->intvl_ms = intvl_ms;
    *t = *t + 1;
}       
            
#ifdef NOTDEF
static void add_gap(sdlx_tone_t **t, int intvl_ms)
{       
    (*t)->freq = 0;
    (*t)->intvl_ms = intvl_ms;
    *t = *t + 1;
}           
#endif
        
static void add_terminator(sdlx_tone_t **t)
{       
    (*t)->freq = 0;
    (*t)->intvl_ms = 0;
    *t = *t + 1;
}

static void page_7_init(void)
{
}

static void helper(float low_freq, float high_freq, float *fft, float delta_f, sdlx_color_t color, int x, int y);

static void page_7_draw(void)
{
    sdlx_audio_state_t state;
    char              *state_str;
    bool               is_recording;
    sdlx_loc_t        *loc;
    double             row=2;

    // get audio state
    sdlx_audio_get_state(&state);

    // display state
    state_str = audio_state_str(&state, &is_recording);
    if (!is_recording) {
        sdlx_render_printf(0, ROW2Y(row), "%s", state_str);
    } else {
        sdlx_render_printf_ex1(0, ROW2Y(row), FONT_NORMAL, COLOR_RED, "%s", state_str);
    }
    row += 2;

    // controls: stop, pause, resume
    loc = sdlx_render_printf_ex1(0, ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "STOP");
    sdlx_register_event(loc, EVID_AUDIO_STOP);
    loc = sdlx_render_printf_ex1(COL2X(6), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "PAUSE");
    sdlx_register_event(loc, EVID_AUDIO_PAUSE);
    loc = sdlx_render_printf_ex1(COL2X(13), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "RESUME");
    sdlx_register_event(loc, EVID_AUDIO_RESUME);
    row += 2.5;

    // controls: play tone at specific frequency
    sdlx_render_printf(0, ROW2Y(row), "TONE");

    loc = sdlx_render_printf_ex1(COL2X(5), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "G");
    sdlx_register_event(loc, EVID_AUDIO_PLAY_TONE_GO);

    loc = sdlx_render_printf_ex1(COL2X(7), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "%d", tone_freq);
    sdlx_register_event(loc, EVID_AUDIO_PLAY_TONE_GET_FREQ);

    loc = sdlx_render_printf_ex1(COL2X(12), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "-");
    sdlx_register_event(loc, EVID_AUDIO_PLAY_TONE_FREQ_DOWN);

    loc = sdlx_render_printf_ex1(COL2X(15), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "+");
    sdlx_register_event(loc, EVID_AUDIO_PLAY_TONE_FREQ_UP);

    loc = sdlx_render_printf_ex1(
                COL2X(18), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "%s",
                tone_lrb == TONE_LEFT_CHANNEL ? "L" : (tone_lrb == TONE_RIGHT_CHANNEL ? "R" : "B"));
    sdlx_register_event(loc, EVID_AUDIO_PLAY_TONE_CHAN_LRB);
    row += 2.5;

    // controls: record from mic and device
    sdlx_render_printf(0, ROW2Y(row), "REC");
    loc = sdlx_render_printf_ex1(COL2X(5), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "MIC");
    sdlx_register_event(loc, EVID_AUDIO_RECORD_FROM_MIC);
    loc = sdlx_render_printf_ex1(COL2X(10), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "DEV");
    sdlx_register_event(loc, EVID_AUDIO_RECORD_FROM_DEV);
    loc = sdlx_render_printf_ex1(COL2X(15), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "MICAP");
    sdlx_register_event(loc, EVID_AUDIO_RECORD_FROM_MIC_APPEND);
    row += 2.5;

    // controls: play from buff and play tones
    sdlx_render_printf(0, ROW2Y(row), "PLAY");
    loc = sdlx_render_printf_ex1(COL2X(5), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "MON");
    sdlx_register_event(loc, EVID_AUDIO_PLAY_MONO_BUFF);
    loc = sdlx_render_printf_ex1(COL2X(9), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "STR");
    sdlx_register_event(loc, EVID_AUDIO_PLAY_STEREO_BUFF);
    loc = sdlx_render_printf_ex1(COL2X(13), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "SEQ");
    sdlx_register_event(loc, EVID_AUDIO_PLAY_TONES_SEQUENCE);
    loc = sdlx_render_printf_ex1(COL2X(17), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "WN");
    sdlx_register_event(loc, EVID_AUDIO_PLAY_WHITE_NOISE);
    row += 2.5;      

    // controls: play recorded mic or device files 
    loc = sdlx_render_printf_ex1(0, ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "mic.mp3");
    sdlx_register_event(loc, EVID_AUDIO_PLAY_MIC_MP3);
    loc = sdlx_render_printf_ex1(COL2X(10), ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, "dev.mp3");
    sdlx_register_event(loc, EVID_AUDIO_PLAY_DEV_MP3);
    row += 2;

    // display volume bar
    int y, w;
    if (state.state != AUDIO_STATE_IDLE) {
        y = sdlx_win_height - 2*sdlx_char_height_dflt - 10;
        w = state.volume * 1000;
        sdlx_render_fill_rect(0, y, w, sdlx_char_height_dflt, COLOR_WHITE);
    }

    // display color organ bars
    if (state.state != AUDIO_STATE_IDLE) {
        int    num_downsample = 4;
        int    num_samples = nearbyint(FRAMES_PER_SEC / num_downsample * 0.050);  // equals 600
        float  samples[600], fft[301];
        float  delta_f;

        delta_f = ((double)FRAMES_PER_SEC/num_downsample) / num_samples;
        y       = sdlx_win_height - sdlx_char_height_dflt - 10;

        sdlx_get_audio_samples(num_samples, num_downsample, GET_SAMPLES_LEFT_CHANNEL, samples);
        util_fft_real_to_real(num_samples, samples, fft, true);
        helper(60,  150,  fft, delta_f, COLOR_RED,   0,   y);
        helper(200, 600,  fft, delta_f, COLOR_GREEN, 333, y);
        helper(800, 2200, fft, delta_f, COLOR_BLUE,  666, y);

        y += sdlx_char_height_dflt/2;
        sdlx_get_audio_samples(num_samples, num_downsample, GET_SAMPLES_RIGHT_CHANNEL, samples);
        util_fft_real_to_real(num_samples, samples, fft, true);
        helper(60,  150,  fft, delta_f, COLOR_RED,   0,   y);
        helper(200, 600,  fft, delta_f, COLOR_GREEN, 333, y);
        helper(800, 2200, fft, delta_f, COLOR_BLUE,  666, y);
    }

    // run util_fft_test, and display result
    int  rc;
    long duration_usec;
    rc = util_fft_test(&duration_usec);
    if (rc == 0) {
        sdlx_render_printf_ex1(0, sdlx_win_height - 4 * sdlx_char_height_dflt,
                               FONT_NORMAL, COLOR_GREEN, 
                               "FftTest OK %ld usec", duration_usec);
    } else {
        sdlx_render_printf_ex1(0, sdlx_win_height - 4 * sdlx_char_height_dflt,
                               FONT_NORMAL, COLOR_RED, 
                               "FftTest FAILED");
    }
}

#define COLOR_ORGAN_SCALE  30
static void helper(float low_freq, float high_freq, float *fft, float delta_f, sdlx_color_t color, int x, int y)
{
    int first_bin, last_bin, w;
    float band_vol;

    first_bin = nearbyint(low_freq/delta_f);
    last_bin = nearbyint(high_freq/delta_f);
    band_vol = util_rms_float(&fft[first_bin], last_bin-first_bin+1);
    w = band_vol * 333 * COLOR_ORGAN_SCALE;
    if (w > 333) w = 333;
    sdlx_render_fill_rect(x, y, w, sdlx_char_height_dflt/2, color);
}

static void page_7_process_event(sdlx_event_t *ev)
{
    printf("I %s: event %d\n", progname, ev->event_id);

    switch (ev->event_id) {
    case EVID_AUDIO_STOP:
        sdlx_audio_stop();
        break;
    case EVID_AUDIO_PAUSE:
        sdlx_audio_pause();
        break;
    case EVID_AUDIO_RESUME:
        sdlx_audio_resume();
        break;
    case EVID_AUDIO_PLAY_TONE_GO:
    case EVID_AUDIO_PLAY_TONE_GET_FREQ:
    case EVID_AUDIO_PLAY_TONE_FREQ_UP:
    case EVID_AUDIO_PLAY_TONE_FREQ_DOWN:
    case EVID_AUDIO_PLAY_TONE_CHAN_LRB: {
        char  *str;
        int    num_samples, i;
        float *samples;

        // update tone_freq and channel if requested
        if (ev->event_id == EVID_AUDIO_PLAY_TONE_FREQ_UP) {
            tone_freq += 100;
        } else if (ev->event_id == EVID_AUDIO_PLAY_TONE_FREQ_DOWN) {
            tone_freq -= 100;
        } else if (ev->event_id == EVID_AUDIO_PLAY_TONE_GET_FREQ) {
            str = sdlx_get_input_str("Frequency", true, NULL);
            if (sscanf(str, "%d", &tone_freq) != 1) {
                break;
            }
        } else if (ev->event_id == EVID_AUDIO_PLAY_TONE_CHAN_LRB) {
            tone_lrb = ((tone_lrb + 1) % 3);
        }

        // limit tone_freq
        if (tone_freq < MIN_TONE_FREQ) tone_freq = MIN_TONE_FREQ;
        if (tone_freq > MAX_TONE_FREQ) tone_freq = MAX_TONE_FREQ;

        // allocate buffer for 100 sine waves of stereo pcm
        num_samples = FRAMES_PER_SEC * 100 / tone_freq;
        num_samples *= 2;
        samples = calloc(num_samples, sizeof(float));

        // init buffer with 100 sine waves
        for (i = 0; i < num_samples; i+=2) {
            if (tone_lrb == TONE_LEFT_CHANNEL) {
                samples[i] = sin(TWO_PI * i * 100 / num_samples);
            } else if (tone_lrb == TONE_RIGHT_CHANNEL) {
                samples[i+1] = sin(TWO_PI * i * 100 / num_samples);
            } else {  // TONE_BOTH_CHANNELS
                samples[i] = sin(TWO_PI * i * 100 / num_samples);
                samples[i+1] = samples[i];
            }
        }

        // play buffer
        int num_channels = 2;
        int num_loops = 0;  // infinite
        sdlx_audio_play_buff(samples, num_samples, num_channels, num_loops, true);
        break; }
    case EVID_AUDIO_RECORD_FROM_MIC:
        // auto_stop_secs = 3
        // append         = false
        // start_paused   = false
        sdlx_audio_record_from_mic(data_dir, "mic.mp3", 3, false, false);
        break;
    case EVID_AUDIO_RECORD_FROM_MIC_APPEND:
        // auto_stop_secs = 3
        // append         = true
        // start_paused   = false
        sdlx_audio_record_from_mic(data_dir, "mic.mp3", 3, true, false);
        break;
    case EVID_AUDIO_RECORD_FROM_DEV:
        // append       = false
        // start_paused = true        
        sdlx_audio_record_from_device(data_dir, "dev.mp3", false, true);
        break;
    case EVID_AUDIO_PLAY_MONO_BUFF: {
        int    secs = 4;
        int    num_channels = 1;
        int    num_samples = secs * FRAMES_PER_SEC * num_channels;
        int    loops = 2;
        float *samples;

        samples = malloc(num_samples * sizeof(float));
        for (int i = 0; i < num_samples; i++) {
            samples[i] = sin(2 * M_PI * 500.0 * i / FRAMES_PER_SEC);
        }
        sdlx_audio_play_buff(samples, num_samples, num_channels, loops, true);
        break; }
    case EVID_AUDIO_PLAY_STEREO_BUFF: {
        int    secs = 4;
        int    num_channels = 2;
        int    num_samples = secs * FRAMES_PER_SEC * num_channels;
        int    loops = 2;
        float *samples;
        int    j = 0;

        samples = malloc(num_samples * sizeof(float));
        for (int i = 0; i < num_samples / 4; i++) {
            samples[j++] = sin(2 * M_PI * 500.0 * i / FRAMES_PER_SEC);
            samples[j++] = 0;
        }
        for (int i = 0; i < num_samples / 4; i++) {
            samples[j++] = 0;
            samples[j++] = sin(2 * M_PI * 500.0 * i / FRAMES_PER_SEC);
        }
        sdlx_audio_play_buff(samples, num_samples, num_channels, loops, true);
        break; }
    case EVID_AUDIO_PLAY_TONES_SEQUENCE: {
        sdlx_tone_t tones[100], *t;

        t = tones;
        for (int f = 0; f <= 3000; f += 100) {
            printf("adding tone %d\n", f);
            add_tone(&t, f, 1000);
        }
        add_terminator(&t);
        sdlx_audio_play_tones(tones);
        break; }
    case EVID_AUDIO_PLAY_WHITE_NOISE: {
        int    secs = 4;
        int    num_channels = 1;
        int    num_samples = secs * FRAMES_PER_SEC * num_channels;
        int    loops = 2;
        float *samples;

        samples = malloc(num_samples * sizeof(float));
        for (int i = 0; i < num_samples; i++) {
            samples[i] = ((double)random() / 0x40000000) - 1;
        }
        sdlx_audio_play_buff(samples, num_samples, num_channels, loops, true);
        break; }
    case EVID_AUDIO_PLAY_MIC_MP3:
        sdlx_audio_play_file(data_dir, "mic.mp3");
        break;
    case EVID_AUDIO_PLAY_DEV_MP3:
        sdlx_audio_play_file(data_dir, "dev.mp3");
        break;
    }
}

static void page_7_exit(void)
{
    sdlx_audio_stop();
}

// -----------------  PAGE 8: SENSOR INFO TBL -----------------

static sdlx_sensor_info_t *sit;
static int                 max_sit;
static char               *sit_lines[100];

static void page_8_init(void)
{
    char str[200];

    sit = sdlx_sensor_get_info_tbl(&max_sit);
    if (sit == NULL) {
        printf("E %s: sdlx_sensor_get_info_tbl failed\n", progname);
    }

    y_mlt_top    = 4 * sdlx_char_height(FONT_SMALL);
    y_mlt_bottom = sdlx_win_height;
    x_mlt        = 0;
    y_mlt        = y_mlt_top;

    sit_lines[0] = strdup("ID TYPE     NAME");
    for (int i = 0; i < max_sit; i++) {
        sprintf(str, "%2d %4d %s", sit[i].id, sit[i].type, sit[i].name);
        sit_lines[i+1] = strdup(str);
    }
}

static void page_8_draw(void)
{
    sdlx_render_multiline_text(x_mlt, y_mlt, y_mlt_top, y_mlt_bottom, 
                               FONT_SMALL, sit_lines, NULL, max_sit+1);

    sdlx_register_event(NULL, EVID_MOTION);
}

static void page_8_process_event(sdlx_event_t *event)
{
    double xrel = event->u.motion.xrel;
    double yrel = event->u.motion.yrel;

    if (fabs(xrel) > fabs(yrel)*1.5) x_mlt += xrel;
    if (fabs(yrel) > fabs(xrel)*1.5) y_mlt += yrel;

    if (y_mlt >= y_mlt_top) y_mlt = y_mlt_top;
    if (x_mlt > 0) x_mlt = 0;
}

static void page_8_exit(void)
{
    for (int i = 0; i < max_sit+1; i++) {
        free(sit_lines[i]);
    }
}

// -----------------  PAGE 9: SENSOR DATA ---------------------

#define ASENSOR_TYPE_ACCELEROMETER       1    // device + gravity accel
#define ASENSOR_TYPE_MAGNETIC_FIELD      2
#define ASENSOR_TYPE_GYROSCOPE           4
#define ASENSOR_TYPE_LIGHT               5
#define ASENSOR_TYPE_PRESSURE            6
#define ASENSOR_TYPE_PROXIMITY           8
#define ASENSOR_TYPE_GRAVITY             9    // gravity accel only
#define ASENSOR_TYPE_LINEAR_ACCELERATION 10   // device accel only
#define ASENSOR_TYPE_ROTATION_VECTOR     11
#define ASENSOR_TYPE_RELATIVE_HUMIDITY   12   // not supported on most smartphone devices
#define ASENSOR_TYPE_AMBIENT_TEMPERATURE 13   // not supported on most smartphone devices
#define ASENSOR_TYPE_SIGNIFICANT_MOTION  17
#define ASENSOR_TYPE_STEP_DETECTOR       18
#define ASENSOR_TYPE_STEP_COUNTER        19

struct raw_sensor_s {
    int   id;
    int   type;
    char *short_name;
} raw_sensor_tbl[20];
int max_raw_sensor_tbl;

static void add_to_raw_sensor_tbl(int type, char *short_name)
{
    int id = sdlx_sensor_find(type);
    if (id == -1) return;

    raw_sensor_tbl[max_raw_sensor_tbl].id = id;
    raw_sensor_tbl[max_raw_sensor_tbl].short_name = short_name;
    max_raw_sensor_tbl++;
}

static void page_9_init(void)
{
    max_raw_sensor_tbl = 0;
    add_to_raw_sensor_tbl(ASENSOR_TYPE_GRAVITY,             "a_grv");   // gravity accel only
    add_to_raw_sensor_tbl(ASENSOR_TYPE_LINEAR_ACCELERATION, "a_dev");   // device accel only
    add_to_raw_sensor_tbl(ASENSOR_TYPE_ACCELEROMETER,       "a_tot");   // device + gravity accel
    add_to_raw_sensor_tbl(ASENSOR_TYPE_MAGNETIC_FIELD,      "magf ");
    add_to_raw_sensor_tbl(ASENSOR_TYPE_GYROSCOPE,           "gyro ");
    add_to_raw_sensor_tbl(ASENSOR_TYPE_LIGHT,               "light");
    add_to_raw_sensor_tbl(ASENSOR_TYPE_PRESSURE,            "press");
    add_to_raw_sensor_tbl(ASENSOR_TYPE_PROXIMITY,           "prox ");
    add_to_raw_sensor_tbl(ASENSOR_TYPE_ROTATION_VECTOR,     "rot  ");
    add_to_raw_sensor_tbl(ASENSOR_TYPE_RELATIVE_HUMIDITY,   "humid");
    add_to_raw_sensor_tbl(ASENSOR_TYPE_AMBIENT_TEMPERATURE, "temp ");
    add_to_raw_sensor_tbl(ASENSOR_TYPE_SIGNIFICANT_MOTION,  "mtion");
    add_to_raw_sensor_tbl(ASENSOR_TYPE_STEP_DETECTOR,       "stepd");
    add_to_raw_sensor_tbl(ASENSOR_TYPE_STEP_COUNTER,        "stepc");
}

static void page_9_draw(void)
{
    int           rc;
    unsigned long step_count;
    double        mag_heading, roll, pitch, millibars;
    double        ax, ay, az;
    int           row = 2;

    //
    // 'High-Level Sensor Access' test
    //

    rc = sdlx_sensor_read_step_counter(&step_count);
    if (rc == 0) {
        sdlx_render_printf(0, ROW2Y(row++), "stepc= %ld", step_count);
    }

    rc = sdlx_sensor_read_mag_heading(&mag_heading);
    if (rc == 0) {
        sdlx_render_printf(0, ROW2Y(row++), "magh =% 3.0f", mag_heading);
    }

    rc = sdlx_sensor_read_gravity_accel(&ax, &ay, &az);
    if (rc == 0) {
        sdlx_render_printf(0, ROW2Y(row++), "a_grv=% 4.1f % 4.1f % 4.1f", ax, ay, az);;
    }

    rc = sdlx_sensor_read_device_accel(&ax, &ay, &az);
    if (rc == 0) {
        sdlx_render_printf(0, ROW2Y(row++), "a_dev=% 4.1f % 4.1f % 4.1f", ax, ay, az);;
    }

    rc = sdlx_sensor_read_roll_pitch(&roll, &pitch);
    if (rc == 0) {
        sdlx_render_printf(0, ROW2Y(row++), "r/p  =% 4.1f % 4.1f", roll, pitch);
    }

    rc = sdlx_sensor_read_pressure(&millibars);
    if (rc == 0) {
        sdlx_render_printf(0, ROW2Y(row++), "press=% 5.0f", millibars);
    }
    
    //
    // 'Low-Level Sensor Access' test
    //

    row++;
    for (int i = 0; i < max_raw_sensor_tbl; i++) {
        struct raw_sensor_s *x = &raw_sensor_tbl[i];
        float data[3];

        sdlx_sensor_read_raw(x->id, data, 3);
        if (strcmp(x->short_name, "stepc") != 0) {
            sdlx_render_printf_ex1(0, ROW2Y(row++), 
                                   FONT_SMALL, COLOR_WHITE,
                                   "%s %7.2f %7.2f %7.2f", 
                                   x->short_name, data[0], data[1], data[2]);
        } else {
            unsigned long stepc;
            memcpy(&stepc, data, 8);
            sdlx_render_printf_ex1(0, ROW2Y(row++), 
                                   FONT_SMALL, COLOR_WHITE,
                                   "%s %7ld", 
                                   x->short_name, step_count);
        }
    }
}

// -----------------  PAGE 10: LOCATION  ----------------------

static char *num2str(double num, char *fmt, char *s);

static void page_10_draw(void)
{
    double lat, lng, alt;
    bool   alt_is_wgs84;
    int    row=3;
    char   s[50];

    util_get_location(&lat, &lng, &alt, &alt_is_wgs84);

    sdlx_render_printf(0, ROW2Y(row++), "Lat  = %s", num2str(lat,"%.4f",s));
    sdlx_render_printf(0, ROW2Y(row++), "Long = %s", num2str(lng,"%.4f",s));
    sdlx_render_printf(0, ROW2Y(row++), "Alt  = %s ft %s",
                       num2str(alt,"%0.0f",s), alt_is_wgs84 ? "WGS84" : "MSL");
}

static char *num2str(double num, char *fmt, char *s)
{
    if (num == INVALID_NUMBER) {
        sprintf(s, "invld");
    } else {
        sprintf(s, fmt, num);
    }
    return s;
}

// -----------------  PAGE 11: TEXT ROTATION  -----------------

#define EVID_ROT_NONE    10
#define EVID_ROT_CTR_90  11
#define EVID_ROT_CTR_180 12
#define EVID_ROT_CTR_270 13

char *text = "hello";
char *wrap_text = "this is line 1\nthis is line 2\nthis is line 3";

int rot_flags = 0;

static void page_11_draw(void)
{
    sdlx_loc_t *loc;
    int y;

    loc = sdlx_render_printf_ex2(500, 500,     
                                 FONT_NORMAL, COLOR_WHITE,
                                 rot_flags | FLAG_XY_CTR, 
                                 "%s", wrap_text);
    sdlx_render_rect(loc->x, loc->y, loc->w, loc->h, 2, COLOR_GREEN);

    y = sdlx_win_height - 8 * (1.5 * sdlx_char_height_dflt);

    loc = sdlx_render_printf_ex1(0, y, FONT_NORMAL, COLOR_LIGHT_BLUE, "ROT_NONE");
    sdlx_register_event(loc, EVID_ROT_NONE);
    y += 1.5 * sdlx_char_height_dflt;

    loc = sdlx_render_printf_ex1(0, y, FONT_NORMAL, COLOR_LIGHT_BLUE, "ROT_CTR_90");
    sdlx_register_event(loc, EVID_ROT_CTR_90);
    y += 1.5 * sdlx_char_height_dflt;

    loc = sdlx_render_printf_ex1(0, y, FONT_NORMAL, COLOR_LIGHT_BLUE, "ROT_CTR_180");
    sdlx_register_event(loc, EVID_ROT_CTR_180);
    y += 1.5 * sdlx_char_height_dflt;

    loc = sdlx_render_printf_ex1(0, y, FONT_NORMAL, COLOR_LIGHT_BLUE, "ROT_CTR_270");
    sdlx_register_event(loc, EVID_ROT_CTR_270);
    y += 1.5 * sdlx_char_height_dflt;
}

static void page_11_process_event(sdlx_event_t *event)
{
    switch (event->event_id) {
    case EVID_ROT_NONE:
        rot_flags = 0;
        break;
    case EVID_ROT_CTR_90:
        rot_flags = FLAG_ROT_CTR_90;
        break;
    case EVID_ROT_CTR_180:
        rot_flags = FLAG_ROT_CTR_180;
        break;
    case EVID_ROT_CTR_270:
        rot_flags = FLAG_ROT_CTR_270;
        break;
    }
}

// -----------------  PAGE 12: LANDSCAPE ----------------------

int page_12_x, page_12_y;
double page_12_xrel, page_12_yrel;

static void page_12_draw(void)
{
    int w=500, h=250;

    // draw blue rectangle around display area
    sdlx_render_rect(0, 0, sdlx_win_width, sdlx_win_height, 3, COLOR_BLUE);

    // create texture1:
    // - wxh = 500 x 250
    // - white rectangle around perimeter
    // - yellow circle in center
    // - 'Hello' text in center
    texture1 = sdlx_create_texture(w, h);
    sdlx_set_render_target(texture1);
    sdlx_render_rect(0, 0, w, h, 5, COLOR_WHITE);
    sdlx_render_fill_circle(w/2, h/2, h/2, COLOR_YELLOW);
    sdlx_render_printf_ex2(w/2, h/2, FONT_NORMAL, COLOR_RED, FLAG_XY_CTR, "%s", "Hello");
    sdlx_set_render_target(NULL);

    // render texture1 to the display
    // - top left, no scaling
    sdlx_render_texture(texture1, 0, 0);
    // - bottom right, scale = 0.5
    sdlx_render_texture_ex1(texture1, sdlx_win_width-w/2, sdlx_win_height-h/2, w/2, h/2);
    // - bottom left, rotated 90 degrees
    sdlx_render_texture_ex2(texture1, -125, sdlx_win_height-h-125, w, h, 90);

    // print the display area dimensions
    sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(3),
                           FONT_NORMAL, COLOR_WHITE,
                           FLAG_X_CTR, 
                           "WxH = %d %d", sdlx_win_width, sdlx_win_height);

    // display the motion location
    sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(4), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
           "xrel = %0.3f", page_12_xrel);
    sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(5), FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
           "yrel = %0.3f", page_12_yrel);
    sdlx_render_point(page_12_x, page_12_y, COLOR_WHITE, 9);
    sdlx_register_event(NULL, EVID_MOTION);

    // destroy texture1
    sdlx_destroy_texture(texture1);
}

static void page_12_process_event(sdlx_event_t *ev)
{
    switch (ev->event_id) {
    case EVID_MOTION:
        page_12_x = nearbyint(ev->u.motion.x);
        page_12_y = nearbyint(ev->u.motion.y);
        page_12_xrel = ev->u.motion.xrel;
        page_12_yrel = ev->u.motion.yrel;
        break;
    }
}

// -----------------  PAGE 13: SVC MAKE REQ  ------------------

static void page_13_init(void)
{
}

static void page_13_exit(void)
{
}

static void page_13_draw(void)
{
    int rc, result;
    sdlx_color_t color;
    static int test;

    // This test repeatedly isses the SVC_TEMPLATE_REQ_ADD_ONE request to the Templae service.
    // The response is checked, and displayed in GREEN if okay.
    
    test++;
    svc_req_t *req = svc_req_init(SVC_TEMPLATE_REQ_ADD_ONE, (char*)&test, sizeof(test));

    rc = svc_make_req("Template", req, 5);
    if (rc != 0) {
        sdlx_render_printf_ex2(
            sdlx_win_width/2, sdlx_win_height/2, 
            FONT_NORMAL, COLOR_RED, FLAG_XY_CTR,
            "svc_make_req failed\nrc = %s", svc_make_req_status_to_str(rc));
        return;
    }

    result = *(int*)&req->data[0];

    color = (result == test + 1 ? COLOR_GREEN : COLOR_RED);
    sdlx_render_printf_ex2(sdlx_win_width/2, sdlx_win_height/2, FONT_NORMAL, color, FLAG_XY_CTR,
                           "ADD1 %d -> %d", test, result);
}

// -----------------  PAGE 14: CAMERA  ------------------------

#define EVID_TAKE_PICTURE 10
#define EVID_RESET_PICTURE 11

sdlx_texture_t *t;
double xc, yc, scale;
int jpeg_w, jpeg_h;

static void page_14_init(void)
{
#if 0
    //t = sdlx_create_texture(jpeg_w, jpeg_h);
    t = sdlx_create_texture(4000, 4000);
    if (t == NULL) {
        printf("E %s: failed to create texture\n", progname);
    }
#endif
}

static void page_14_exit(void)
{
    if (t) {
        printf("I %s: page_14_exit cleaning up\n", progname);
        sdlx_destroy_texture(t);
        t = NULL;
    }
}

static void page_14_draw(void)
{
    sdlx_loc_t src, dest;
    sdlx_loc_t *loc;

    if (t) {
        //src.x = xc - (scale * jpeg_w) / 2;
        //src.y = yc - (scale * jpeg_h) / 2;
        //src.w = jpeg_w;
        //src.h = jpeg_h;
        //printf("SRC %d %d %d %d - SCALE %f\n", src.x, src.y, src.w, src.h, scale);

        bool flag = false;

        src.x = xc - jpeg_w / 2 * scale;
        src.y = yc - jpeg_h / 2 * scale;
        src.w = jpeg_w * scale;
        src.h = jpeg_h * scale;

        if (src.x < 0) {
            xc = jpeg_w / 2 * scale;
            flag = true;
        } else if (src.x + src.w >= jpeg_w) {
            xc = jpeg_w - jpeg_w / 2 * scale;
            flag = true;
        }

        if (src.y < 0) {
            yc = jpeg_h / 2 * scale;
            flag = true;
        } else if (src.y + src.h >= jpeg_h) {
            yc = jpeg_h - jpeg_h / 2 * scale;
            flag = true;
        }

        if (flag) {
            int new_x = xc - jpeg_w / 2 * scale;
            int new_y = yc - jpeg_h / 2 * scale;

            // xxx flag is always set?
            //printf("XC,YC,SCALE = %0.0f %0.0f %0.3f  SRC %d %d %d %d => %d %d %d %d\n",
            //       xc, yc, scale,
            //       src.x, src.y, src.w, src.h,
            //       new_x, new_y, src.w, src.h);

            src.x = new_x;
            src.y = new_y;
        } else {
            //printf("XC,YC,SCALE = %0.0f %0.0f %0.3f  SRC %d %d %d %d\n", 
            //       xc, yc, scale,
            //       src.x, src.y, src.w, src.h);
        }

        dest.x = 0;
        dest.y = 0;
        if (orientation == PORTRAIT) {
            dest.w = 1000;
            dest.h = 1000 * ((double)jpeg_h / jpeg_w);  // xxx deviding here
        } else {
            dest.h = 1000;
            dest.w = 1000 * ((double)jpeg_w / jpeg_h);  // xxx deviding here
        }

        sdlx_render_texture_new(t, &src, &dest);
    }

    loc = sdlx_render_printf_ex1(sdlx_win_width-5*sdlx_char_width_dflt, sdlx_win_height-4*sdlx_char_height_dflt, 
                                 FONT_NORMAL, COLOR_LIGHT_BLUE, "RESET");
    sdlx_register_event(loc, EVID_RESET_PICTURE);

    loc = sdlx_render_printf_ex1(sdlx_win_width-5*sdlx_char_width_dflt, sdlx_win_height-2*sdlx_char_height_dflt, 
                                 FONT_NORMAL, COLOR_LIGHT_BLUE, "TAKE");
    sdlx_register_event(loc, EVID_TAKE_PICTURE);

    sdlx_register_event(NULL, EVID_MOTION);
    sdlx_register_event(NULL, EVID_PINCH);
}

static void page_14_process_event(sdlx_event_t *ev)
{
    int fd, rc;
    void *out_pixels;

    switch (ev->event_id) {
    case EVID_TAKE_PICTURE:

        printf("I %s: calling take_picture\n", progname);
        rc = util_take_picture();  // xxx return status
        if (rc != 0) {
            printf("E %s: util_take_picture failed\n", progname);
            break;
        }

        printf("I %s: back from take_picture, rc=%d\n", progname, rc);

        fd = open("tmp/photo.jpg", O_RDONLY, 0);
        if (fd == -1) {
            printf("E %s: failed to open jpeg file, %s\n", progname, strerror(errno));
            break;
        }

        rc = util_decode_jpeg_to_raw(fd, &jpeg_w, &jpeg_h, &out_pixels);
        if (rc != 0) {
            printf("E %s: failed to decode jpeg file, %s\n", progname, strerror(errno));
            close(fd);
            break;
        }
        printf("I %s: decode_jpeg okay, w/h=%d,%d\n", progname, jpeg_w, jpeg_h);

        close(fd);

        if (t != NULL) {
            int w, h;
            sdlx_query_texture(t, &w, &h);
            if (w != jpeg_w || h != jpeg_h) {
                sdlx_destroy_texture(t);
                t = NULL;
            }
        }

        if (t == NULL) {
            printf("I %s: CREATING TEXTURE %d %d\n", progname, jpeg_w, jpeg_h);
            t = sdlx_create_texture(jpeg_w, jpeg_h);
            if (t == NULL) {
                printf("E %s: failed to create texture\n", progname);
                free(out_pixels);
                break;
            }
        }

        sdlx_set_texture_pixels(t, out_pixels);
        printf("I %s: after call to sdlx_set_texture_pixels\n", progname);

        free(out_pixels);

        xc = jpeg_w / 2;
        yc = jpeg_h / 2;
        scale = 1;
        break;
    case EVID_MOTION:
        xc -= ev->u.motion.xrel * scale * 3; // xxx where is the 3 from
        yc -= ev->u.motion.yrel * scale * 3;
        //printf("I %s: motion x,y = %f %f\n", progname, xc, yc);
        break;
    case EVID_PINCH:
        if (ev->u.pinch.scale == 0) break;
        scale /= ev->u.pinch.scale;
        if (scale > 1) scale = 1;
        //printf("i %s: scale %f\n", progname, ev->u.pinch.scale);
        break;
    case EVID_RESET_PICTURE:
        xc = jpeg_w / 2;
        yc = jpeg_h / 2;
        scale = 1;
        break;
    }
}

// -----------------  PAGE 15: PINCH  -------------------------

#define RADIUS 500

double pg15_scale, pg15_focus_x, pg15_focus_y, pg15_span_x, pg15_span_y;
int pg15_orientation_save;

static void page_15_init(void)
{
    pg15_scale = 1;
    pg15_focus_x = sdlx_win_width/2;
    pg15_focus_y = sdlx_win_height/2;
    pg15_span_x = 0;
    pg15_span_y = 0;
    pg15_orientation_save = orientation;
}

static void page_15_exit(void)
{
    // nothing needed
}

static void page_15_draw(void)
{
    if (orientation != pg15_orientation_save) {
        page_15_init();
    }

    sdlx_render_fill_circle(pg15_focus_x, pg15_focus_y, RADIUS*pg15_scale, COLOR_YELLOW);
    sdlx_render_fill_rect(pg15_focus_x-pg15_span_x/2, pg15_focus_y-25, pg15_span_x, 50, COLOR_BLUE);
    sdlx_render_fill_rect(pg15_focus_x-25, pg15_focus_y-pg15_span_y/2, 50, pg15_span_y, COLOR_BLUE);

    sdlx_render_printf_ex2(sdlx_win_width/2, sdlx_win_height-sdlx_char_height(FONT_NORMAL)/2, 
                           FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, "Scale = %0.3f", pg15_scale);

    sdlx_register_event(NULL, EVID_PINCH);
    sdlx_register_event(NULL, EVID_MOTION);
}

static void page_15_process_event(sdlx_event_t *ev)
{
    switch(ev->event_id) {
    case EVID_PINCH:
        printf("I %s: PINCH scale=%0.3f  focus=%0.0f %0.0f  span=%0.0f %0.0f\n",
               progname,
               ev->u.pinch.scale,
               ev->u.pinch.focus_x, ev->u.pinch.focus_y,
               ev->u.pinch.span_x, ev->u.pinch.span_y);

        pg15_scale  *= ev->u.pinch.scale;
        pg15_focus_x = ev->u.pinch.focus_x;
        pg15_focus_y = ev->u.pinch.focus_y;
        pg15_span_x  = ev->u.pinch.span_x;
        pg15_span_y  = ev->u.pinch.span_y;
        break;
    case EVID_MOTION:
        printf("I %s: MOTION xy=%0.0f %0.0f  xyrel=%0.0f %0.0f\n", 
               progname, 
               ev->u.motion.x, ev->u.motion.y,
               ev->u.motion.xrel, ev->u.motion.yrel);
        pg15_focus_x += ev->u.motion.xrel;
        pg15_focus_y += ev->u.motion.yrel;
        break;
    }
}
