#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <sdlx.h>
#include <utils.h>
#include <svcs.h>

#include "lib/lib.h"

// -----------------  ORIENTATION  --------------------------------

// returns PORTRAIT or LANDSCAPE
int get_device_orientation(void)
{
    double ax, ay, az;
    int rc;
    static int orient = PORTRAIT;
    static bool error_printed;

    rc = sdlx_sensor_read_gravity_accel(&ax, &ay, &az);
    if (rc != 0) {
        if (!error_printed) {
            printf("E lib: get_device_orientation failed to read accelerometer\n");
            error_printed = true;
        }
        return orient;
    }
    
    if (ay > 7 && orient != PORTRAIT) {
        printf("I lib: orientation is now PORTRAIT\n");
        orient = PORTRAIT;
    }

    if (ax > 7 && orient != LANDSCAPE) {
        printf("I lib: orientation is now LANDSCAPE\n");
        orient = LANDSCAPE;
    }

    return orient;
}

// -----------------  BAR GRAPH  ----------------------------------

int y_axis_3[10]     =  {1, 2, 3, INVALID_NUMBER};
int y_axis_5[10]     =  {1, 2, 3, 4, 5, INVALID_NUMBER};
int y_axis_10[10]    =  {2, 4, 6, 8, 10, INVALID_NUMBER};
int y_axis_30[10]    =  {10, 20, 30, INVALID_NUMBER};
int y_axis_50[10]    =  {10, 20, 30, 40, 50, INVALID_NUMBER};
int y_axis_100[10]   =  {20, 40, 60, 80, 100, INVALID_NUMBER};
int y_axis_300[10]   =  {100, 200, 300, INVALID_NUMBER};
int y_axis_500[10]   =  {100, 200, 300, 400, 500, INVALID_NUMBER};
int y_axis_1000[10]  =  {200, 400, 600, 800, 1000, INVALID_NUMBER};
int y_axis_3000[10]   =  {1000, 2000, 3000, INVALID_NUMBER};
int y_axis_5000[10]   =  {1000, 2000, 3000, 4000, 5000, INVALID_NUMBER};
int y_axis_10000[10]  =  {2000, 4000, 6000, 8000, 10000, INVALID_NUMBER};
int y_axis_30000[10]   =  {10000, 20000, 30000, INVALID_NUMBER};
int y_axis_50000[10]   =  {10000, 20000, 30000, 40000, 50000, INVALID_NUMBER};
int y_axis_100000[10]  =  {20000, 40000, 60000, 80000, 100000, INVALID_NUMBER};

int max_y_values[15] = {3, 5, 10, 
                        30, 50, 100, 
                        300, 500, 1000,
                        3000, 5000, 10000,
                        30000, 50000, 100000};

void display_bar_graph(
        int graph_x, int graph_y, int graph_w, int graph_h,     // graph location
        double *values, sdlx_color_t *colors, int max_values,   // bar values and colors
        int max_y,                                              // max_y axis
        char *x_axis_str)                                       // x axis label string
{
    int idx, h;
    int graph_y_bottom;
    int n = sizeof(max_y_values) / sizeof(int);

    // max_y should normally be passed in containing one of the max_y_values;
    // if not, then adjust max_y upward to the nearest max_y_value
    for (int i = 0; i < n; i++) {
        if (max_y <= max_y_values[i]) {
            max_y = max_y_values[i];
            break;
        }
    }
    if (max_y > max_y_values[n-1]) {
        max_y = max_y_values[n-1];
    }

    // draw rectangle around the graph area
    sdlx_render_rect(graph_x, graph_y, graph_w, graph_h, 5, COLOR_WHITE);

    // shrink graph location by 10 pixels to compensate for the 
    // rectangle that was drawn around the graph area
    graph_x += 5;
    graph_y += 5;
    graph_w -= 10;
    graph_h -= 10;
    graph_y_bottom = graph_y + graph_h - 1;

    // draw graph bars
    double w = (double)(graph_w) / max_values;
    for (idx = 0; idx < max_values; idx++) {
        h = values[idx] / max_y * graph_h;
        if (h > graph_h) h = graph_h;
        if (h < 0) h = 0;
        if (h > 0) {
            sdlx_render_fill_rect(idx*w+8, graph_y_bottom-h, w-6, h, colors[idx]);
        }
    }

    // display graph x-axis labels
    int len = strlen(x_axis_str);
    sdlx_render_printf_ex1(graph_x, graph_y_bottom+5, len, COLOR_WHITE, "%s", x_axis_str);

    // display graph y-axis labels
    int *y_axis = NULL;
    switch (max_y) {
    case 3:    y_axis = y_axis_3; break;
    case 5:    y_axis = y_axis_5; break;
    case 10:   y_axis = y_axis_10; break;
    case 30:   y_axis = y_axis_30; break;
    case 50:   y_axis = y_axis_50; break;
    case 100:  y_axis = y_axis_100; break;
    case 300:  y_axis = y_axis_300; break;
    case 500:  y_axis = y_axis_500;  break;
    case 1000: y_axis = y_axis_1000; break;
    case 3000:  y_axis = y_axis_3000; break;
    case 5000:  y_axis = y_axis_5000;  break;
    case 10000: y_axis = y_axis_10000; break;
    case 30000:  y_axis = y_axis_30000; break;
    case 50000:  y_axis = y_axis_50000;  break;
    case 100000: y_axis = y_axis_100000; break;
    }
    if (y_axis) {
        for (int i = 0; y_axis[i] != INVALID_NUMBER; i++) {
            int y = graph_y_bottom - 
                    ((double)y_axis[i] / max_y) * graph_h -
                    sdlx_char_height(FONT_SMALL) / 2;
            sdlx_render_printf_ex2(graph_x-10, y, 
                                   FONT_SMALL, COLOR_WHITE, FLAG_BG_BLACK,
                                   "%d", y_axis[i]);
        }
    }
}

// decrease max_y to the next lower may_y_values
void bar_graph_decrease_y_axis(int *max_y)
{
    int n = sizeof(max_y_values) / sizeof(int);

    printf("decreasing\n");

    if (*max_y <= max_y_values[0]) {
        *max_y = max_y_values[0];
        return;
    }

    for (int i = n-1; i >= 0; i--) {
        if (*max_y > max_y_values[i]) {
            *max_y = max_y_values[i];
            return;
        }
        if (*max_y == max_y_values[i]) {
            *max_y = max_y_values[i-1];
            return;
        }
    }
}

// increase max_y to the next higher may_y_values
void bar_graph_increase_y_axis(int *max_y)
{
    int n = sizeof(max_y_values) / sizeof(int);

    if (*max_y >= max_y_values[n-1]) {
        *max_y = max_y_values[n-1];
        return;
    }

    for (int i = 0; i < n; i++) {
        if (*max_y < max_y_values[i]) {
            *max_y = max_y_values[i];
            return;
        }
        if (*max_y == max_y_values[i]) {
            *max_y = max_y_values[i+1];
            return;
        }
    }
}

// -----------------  DATE UTILS  ---------------------------------

// in the following code:
// - y = year, for example 2026
// - m = month, 1-12
// - d = day, 1-31

// notes regarding struct tm field values
// - tm_mday  1-31
// - tm_mon   0-11
// - tm_year  year minus 1900

char *month_str_tbl[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
char *day_str_tbl[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

char *ymd_to_str(int y, int m, int d)
{
    static char ymd_str[20];
    
    sprintf(ymd_str, "%s %s %d, %d",
            get_weekday_str(y, m, d), get_month_str(m), d, y);
    return ymd_str;
}

char *get_month_str(int m)
{
    return month_str_tbl[m-1];
}

char *get_weekday_str(int y, int m, int d)
{
    struct tm tm;
    time_t t;

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = y - 1900;
    tm.tm_mon  = m - 1;
    tm.tm_mday = d;
    tm.tm_isdst = -1;  // system will determine dst

    t = mktime(&tm);
    localtime_r(&t, &tm);
    return day_str_tbl[tm.tm_wday];
}

bool is_weekend(int y, int m, int d)
{
    struct tm tm;
    time_t t;

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = y - 1900;
    tm.tm_mon  = m - 1;
    tm.tm_mday = d;
    tm.tm_isdst = -1;  // system will determine dst

    t = mktime(&tm);
    localtime_r(&t, &tm);
    return tm.tm_wday == 0 || tm.tm_wday == 6;
}

bool is_today(int y, int m, int d)
{
    struct tm tm;
    time_t t;

    t = time(NULL);
    localtime_r(&t, &tm);

    return (tm.tm_year+1900 == y) &&
           (tm.tm_mon+1 == m) &&
           (tm.tm_mday == d);
}

int days_in_month(int y, int m)
{
    if (m == 9 || m == 4 || m == 6 || m == 11) {
        return 30;
    } else if (m == 2) {
        bool leap_year = (((y % 4) == 0) && !((y % 100) == 0)) || ((y % 400) == 0);
        return leap_year ? 29 : 28;
    } else {
        return 31;
    }
}

void get_current_ymd(int *y, int *m, int *d)
{
    time_t t;
    struct tm tm;

    t = time(NULL);
    localtime_r(&t, &tm);
    *y = tm.tm_year + 1900;
    *m = tm.tm_mon + 1;
    *d = tm.tm_mday;
}

void set_ymd_to_prior(int *y_arg, int *m_arg, int *d_arg)
{
    int y = *y_arg;
    int m = *m_arg;
    int d = *d_arg;

    if (--d < 1) {
        if (--m < 1) {
            m = 12;
            y--;
        }
        d = days_in_month(y, m);
    }

    *y_arg = y;
    *m_arg = m;
    *d_arg = d;
}

void set_ymd_to_next(int *y_arg, int *m_arg, int *d_arg)
{
    int y = *y_arg;
    int m = *m_arg;
    int d = *d_arg;

    if (++d > days_in_month(y, m)) {
        if (++m > 12) {
            m = 1;
            y++;
        }
        d = 1;
    }

    *y_arg = y;
    *m_arg = m;
    *d_arg = d;
}

// -----------------  STRING UTILS  -------------------------------

void str_remove_trailing_newline(char *s)
{
    int len = strlen(s);

    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }
}

void str_sanitize(char *s)
{
    int i;
    char *p;

    // remove trailing newline
    str_remove_trailing_newline(s);

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

// -----------------  SHOW FILE  ----------------------------------

#define README_FONT_SMALLEST  40
#define README_FONT_LARGEST   30

#define EVID_README_FONT_SELECT 1

void show_file(char *data_dir, char *filename)
{
    double       x, y;
    int          y_top, y_bottom;
    int          len;
    sdlx_event_t event;
    bool         done = false;
    char        *lines[1];
    sdlx_color_t colors[1];
    int          orient, new_orient;
    bool         orient_changed;
    int          fontid;

    // get fontid from params file
    fontid = util_get_numeric_param(data_dir, "readme_fontid", README_FONT_SMALLEST);

    // read the file
    lines[0] = util_read_file(data_dir, filename, &len);
    if (lines[0] == NULL) {
        printf("E lib: failed to read %s\n", filename);
        return;
    }

    // init vars
    x         = 0;
    y_top     = 25;
    y_bottom  = sdlx_win_height;
    y         = y_top;
    orient    = -1;
    colors[0] = COLOR_BLACK;

    // display filename lines
    while (true) {
        // handle change of display orientation
        new_orient = get_device_orientation();
        orient_changed = (new_orient != orient);
        orient = new_orient;

        // display file lines and register for motion (scrolling) & exit events
        sdlx_display_init(COLOR_WHITE, orient);
        if (orient_changed) {
            x = 0;
            y = y_top;
            y_bottom  = sdlx_win_height;
        }
        sdlx_render_multiline_text(x, y, y_top, y_bottom, fontid, lines, colors, 1);
        sdlx_register_control_events(EVID_README_FONT_SELECT, "FONT", 0, NULL, EVID_QUIT, "X");
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_display_present();

        // wait for event with 100ms timeout;
        // if timedout then continue
        sdlx_get_event(100000, &event);
        if (event.event_id == -1) {
            continue;
        }
    
        // process the event
        switch (event.event_id) {
        case EVID_README_FONT_SELECT:
            fontid = (fontid > README_FONT_LARGEST ? fontid-5 : README_FONT_SMALLEST);
            util_set_numeric_param(data_dir, "readme_fontid", fontid);
            printf("I lib: fontid = %d\n", fontid);
            x = 0;
            y = y_top;
            break;
        case EVID_MOTION: {
            double xrel = event.u.motion.xrel;
            double yrel = event.u.motion.yrel;

            if (fabs(xrel) > fabs(yrel)*1.5) x += xrel;
            if (fabs(yrel) > fabs(xrel)*1.5) y += yrel;

            if (y >= y_top) y = y_top;
            if (x > 0) x = 0;
            break; }
        case EVID_QUIT:
            done = true;
            break;
        }

        if (done) {
            break;
        }
    }

    // free allocation
    free(lines[0]);
}

// -----------------  EVENT REGISTRATION  -------------------------

void reg_event_str(int x, int y, sdlx_color_t color, char *event_name, int event_id)
{
    sdlx_loc_t *loc;

    loc = sdlx_render_printf_ex1(x, y, FONT_NORMAL, color, "%s", event_name);
    sdlx_register_event(loc, event_id);
}

void reg_event_fill_rect(int x, int y, int w, int h, sdlx_color_t color, int event_id)
{
    sdlx_loc_t loc;

    sdlx_render_fill_rect(x, y, w, h, color);
    loc.x = x;
    loc.y = y;
    loc.w = w;
    loc.h = h;
    sdlx_register_event(&loc, event_id);
}

void reg_event_show_readme_file(void)
{
    reg_event_str(sdlx_win_width-2*sdlx_char_width(FONT_NORMAL), 0, 
                  COLOR_LIGHT_BLUE, "?", EVID_SHOW_README_FILE);
}

// -----------------  SERVICE REQUEST INITIALIZER  ----------------

svc_req_t *svc_req_init(int req_id, char *data, int data_len)
{
    static svc_req_t req;

    // check data_len
    if (data_len > MAX_SVC_REQ_DATA) {
        printf("E lib: svc_req_init, data_len=%d, too large\n", data_len);
        return NULL;
    }
            
    // zero, and init the req
    memset(&req, 0, sizeof(req)); 
    req.id = req_id;
    memcpy(req.data, data, data_len);

    // return ptr to static defined req
    return &req;
}   

// -----------------  DOUBLE LINKED LIST  --------------------------

// xxx add is_list_empty

void add_to_list(node_t *loc, node_t *new_elem)
{
    node_t *elem1 = loc;
    node_t *elem2 = elem1->next;

    elem1->next = new_elem;
    new_elem->prev = elem1;

    elem2->prev = new_elem;
    new_elem->next = elem2;
}

void init_list_head(node_t *head)
{
    head->next = head;
    head->prev = head;
}

void add_to_list_head(node_t *head, node_t *new_elem)
{
    add_to_list(head, new_elem);
}

void add_to_list_tail(node_t *head, node_t *new_elem)
{
    add_to_list(head->prev, new_elem);
}

void remove_from_list(node_t *remove_elem)
{
    node_t *elem1 = remove_elem->prev;
    node_t *elem2 = remove_elem->next;

    elem1->next = elem2;
    elem2->prev = elem1;

    remove_elem->next = NULL;
    remove_elem->prev = NULL;
}

