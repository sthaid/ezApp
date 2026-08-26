#ifndef __LIB_H__
#define __LIB_H__

#include <svcs.h>

// device orientation
int get_device_orientation(void);

// bar graph
void display_bar_graph(
        int graph_x, int graph_y, int graph_w, int graph_h,
        double *values, sdlx_color_t *colors, int max_values, int max_y,
        char *x_axis_str);
void bar_graph_increase_y_axis(int *max_y);
void bar_graph_decrease_y_axis(int *max_y);

// date utils, args:
// - y = year, for example 2026
// - m = month, 1-12
// - d = day, 1-31
char *ymd_to_str(int y, int m, int d);
char *get_month_str(int m);
char *get_weekday_str(int y, int m, int d);
bool is_weekend(int y, int m, int d);
bool is_today(int y, int m, int d);
int days_in_month(int y, int m);
void get_current_ymd(int *y, int *m, int *d);
void set_ymd_to_prior(int *y, int *m, int *d);
void set_ymd_to_next(int *y, int *m, int *d);

// string utils
void str_remove_trailing_newline(char *s);
void str_sanitize(char *s);

// show file
void show_file(char *dir, char *filename);

// event registration
#define EVID_SHOW_README_FILE 999999  // xxx make this 1000000+
void reg_event_str(int x, int y, sdlx_color_t color, char *event_name, int event_id);
void reg_event_fill_rect(int x, int y, int w, int h, sdlx_color_t color, int event_id);
void reg_event_show_readme_file(void);

// init service request
svc_req_t *svc_req_init(int req_id, char *data, int data_len);

#endif
