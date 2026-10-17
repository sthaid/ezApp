#include "apps/ScrnSaver/common.h"

#define EVID_PREV 1
#define EVID_NEXT 2

#define FRAME_US       33333L
#define NAME_HOLD_US   3000000L

static void draw_banner(int index);
static void select_saver(int *index, int dir, double *origin, long *name_until);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    sdlx_event_t event;
    bool done;
    int index;
    double origin;
    long name_until;
    long t0;
    long elapsed;
    long wait;

    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", "ScrnSaver");
        return 1;
    }
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    srand((int)time(NULL));

    index = (int)util_get_numeric_param(data_dir, "saver", 0);
    if (index < 0 || index >= saver_count()) {
        index = 0;
    }

    saver_dispatch(index, SS_OP_INIT, 0.0);
    origin = util_microsec_timer() / 1000000.0;
    name_until = util_microsec_timer() + NAME_HOLD_US;
    done = false;

    while (!done) {
        double now;
        double t;

        t0 = util_microsec_timer();
        now = t0 / 1000000.0;
        t = now - origin;

        sdlx_display_init(COLOR_BLACK, PORTRAIT);
        saver_dispatch(index, SS_OP_DRAW, t);

        if (t0 < name_until) {
            draw_banner(index);
        }

        reg_event_str(sdlx_win_width - 2 * sdlx_char_width(FONT_NORMAL), 0,
                      ss_rgb(150, 190, 210, 210), "?", EVID_SHOW_README_FILE);

        sdlx_register_control_events(EVID_PREV, "<",
                                     EVID_NEXT, ">",
                                     EVID_QUIT, "X");
        sdlx_display_present();

        elapsed = util_microsec_timer() - t0;
        if (elapsed < 0) elapsed = 0;
        if (elapsed >= FRAME_US) {
            wait = 1000;
        } else {
            wait = FRAME_US - elapsed;
        }
        sdlx_get_event(wait, &event);

        if (event.event_id == EVID_NEXT) {
            select_saver(&index, 1, &origin, &name_until);
        } else if (event.event_id == EVID_PREV) {
            select_saver(&index, -1, &origin, &name_until);
        } else if (event.event_id == EVID_SHOW_README_FILE) {
            show_file(data_dir, "README");
        } else if (event.event_id == EVID_QUIT) {
            done = true;
        }
    }

    saver_dispatch(index, SS_OP_CLEANUP, 0.0);
    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  BANNER AND SELECTION  ---------------------------

static void draw_banner(int index)
{
    char *name;
    int cw;
    int ch;
    int tw;
    int x;
    int y;
    int pad;

    name = saver_name(index);
    cw = sdlx_char_width(FONT_SMALL);
    ch = sdlx_char_height(FONT_SMALL);
    tw = (int)strlen(name) * cw;
    pad = 18;
    y = 36;
    x = sdlx_win_width / 2 - tw / 2 - pad;

    sdlx_render_fill_rect(x, y - 10, tw + pad * 2, ch + 20, ss_rgb(0, 0, 0, 120));
    sdlx_render_printf_ex(sdlx_win_width / 2, y,
                          FONT_SMALL, COLOR_WHITE, FLAG_X_CTR,
                          "%s", name);
}

static void select_saver(int *index, int dir, double *origin, long *name_until)
{
    saver_dispatch(*index, SS_OP_CLEANUP, 0.0);
    *index = *index + dir;
    if (*index >= saver_count()) {
        *index = 0;
    }
    if (*index < 0) {
        *index = saver_count() - 1;
    }
    util_set_numeric_param(data_dir, "saver", *index);
    saver_dispatch(*index, SS_OP_INIT, 0.0);
    *origin = util_microsec_timer() / 1000000.0;
    *name_until = util_microsec_timer() + NAME_HOLD_US;
}
