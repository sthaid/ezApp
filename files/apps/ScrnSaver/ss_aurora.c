#include "apps/ScrnSaver/common.h"

// Northern lights over a night lake. Ribbons are chains of
// translucent circles. The lake repeats the lower ribbons.

#define AURORA_STARS 42

static int aurora_sx[AURORA_STARS];
static int aurora_sy[AURORA_STARS];
static int aurora_ph[AURORA_STARS];
static int aurora_ready;

static int aurora_horizon(void);
static void aurora_init_sky(void);
static void aurora_sky(int horizon);
static void aurora_ribbon(double t, int ybase, int amp, double phase,
                          int rad, int r, int g, int b, int alpha,
                          int samples, int horizon, int reflect);
static void aurora_moon(double t, int horizon);
static void aurora_stars(double t, int horizon, int mx, int my);

// -----------------  ENTRY  ------------------------------------------

void ss_aurora(int op, double t)
{
    int horizon;

    if (op == SS_OP_INIT) {
        aurora_init_sky();
        return;
    }
    if (op != SS_OP_DRAW) {
        return;
    }
    if (!aurora_ready) {
        aurora_init_sky();
    }

    horizon = aurora_horizon();
    aurora_sky(horizon);
    aurora_stars(t, horizon, 760, 300);
    aurora_ribbon(t, 500, 64, t * 0.70 + 1.2, 86, 168, 88, 230, 26, 22, horizon, 0);
    aurora_ribbon(t, 720, 96, t * 0.48, 110, 36, 214, 128, 30, 26, horizon, 1);
    aurora_ribbon(t, 980, 58, t * 0.36 + 2.4, 100, 32, 186, 226, 28, 26, horizon, 1);
    aurora_moon(t, horizon);

    sdlx_render_fill_rect(0, horizon - 2, sdlx_win_width, 5, ss_rgb(110, 200, 185, 80));
}

// -----------------  SKY  --------------------------------------------

static int aurora_horizon(void)
{
    return sdlx_win_height * 64 / 100;
}

static void aurora_init_sky(void)
{
    int i;
    int horizon;
    int w;

    horizon = aurora_horizon();
    w = sdlx_win_width;
    if (horizon < 2) horizon = 2;
    if (w < 2) w = 2;

    for (i = 0; i < AURORA_STARS; i++) {
        aurora_sx[i] = rand() % w;
        aurora_sy[i] = rand() % horizon;
        aurora_ph[i] = rand() % 628;
    }
    aurora_ready = 1;
}

static void aurora_sky(int horizon)
{
    int i;
    int bands;
    int h;
    int w;
    int y0;
    int y1;
    int hh;
    int r;
    int g;
    int b;

    w = sdlx_win_width;
    h = sdlx_win_height;
    bands = 80;
    if (horizon < 1) horizon = 1;
    if (h <= horizon) horizon = h - 1;

    for (i = 0; i < bands; i++) {
        double u;
        y0 = i * h / bands;
        y1 = (i + 1) * h / bands;
        hh = y1 - y0 + 1;
        if (y0 < horizon) {
            u = (double)y0 / (double)horizon;
            r = (int)(2 + 8 * u);
            g = (int)(3 + 18 * u);
            b = (int)(12 + 40 * u);
        } else {
            u = (double)(y0 - horizon) / (double)(h - horizon);
            r = (int)(8 * (1.0 - u));
            g = (int)(16 * (1.0 - u));
            b = (int)(28 * (1.0 - u));
        }
        sdlx_render_fill_rect(0, y0, w, hh, ss_rgb(r, g, b, 255));
    }
}

static void aurora_stars(double t, int horizon, int mx, int my)
{
    int i;
    int bri;
    int size;

    for (i = 0; i < AURORA_STARS; i++) {
        int dx;
        int dy;
        if (aurora_sy[i] >= horizon) continue;
        dx = aurora_sx[i] - mx;
        dy = aurora_sy[i] - my;
        if (dx * dx + dy * dy < 70 * 70) continue;

        bri = 150 + (int)(90.0 * sin(t * 2.2 + aurora_ph[i] / 100.0));
        bri = ss_clampi(bri, 40, 255);
        size = 0;
        if (i % 9 == 0) size = 2;
        sdlx_render_point(aurora_sx[i], aurora_sy[i], ss_rgb(bri, bri, 220, 255), size);
        if (size == 2) {
            sdlx_render_fill_circle(aurora_sx[i], aurora_sy[i], 3,
                                    ss_rgb(200, 210, 255, 40));
        }
    }
}

// -----------------  LIGHTS  -----------------------------------------

static void aurora_ribbon(double t, int ybase, int amp, double phase,
                          int rad, int r, int g, int b, int alpha,
                          int samples, int horizon, int reflect)
{
    int i;
    int w;
    int h;

    if (samples < 2) return;
    w = sdlx_win_width;
    h = sdlx_win_height;

    for (i = 0; i < samples; i++) {
        double u;
        double wave;
        int x;
        int y;
        int rr;
        int a;
        int gg;
        int ry;

        u = (double)i / (double)(samples - 1);
        wave = sin(u * 2.0 * M_PI * 1.35 + phase) * amp;
        wave = wave + sin(u * 2.0 * M_PI * 2.6 - phase * 0.62) * (amp * 0.36);
        x = (int)(u * (w + 120) - 60);
        y = ybase + (int)wave + (int)(22.0 * sin(t * 0.22 + phase));
        rr = rad + (int)(8.0 * sin(u * 6.0 + phase));
        if (rr < 24) rr = 24;
        a = alpha + (int)(8.0 * sin(t * 0.8 + u * 6.0));
        gg = g + (int)(18.0 * sin(u * 5.0 + phase));

        // wide overlap turns the circles into one curtain
        sdlx_render_fill_circle(x, y, rr, ss_rgb(r, gg, b, a));
        sdlx_render_fill_circle(x, y + rr / 5, rr * 3 / 4, ss_rgb(r, gg, b, a / 2));

        if (reflect && y < horizon) {
            ry = horizon + (horizon - y);
            if (ry > horizon && ry < h) {
                sdlx_render_fill_circle(x, ry, rr * 2 / 3, ss_rgb(r, gg, b, a / 3));
            }
        }
    }
}

static void aurora_moon(double t, int horizon)
{
    int mx;
    int my;
    int i;
    int h;

    mx = 760;
    my = 300;
    h = sdlx_win_height;

    sdlx_render_fill_circle(mx, my, 130, ss_rgb(255, 236, 190, 10));
    sdlx_render_fill_circle(mx, my, 96, ss_rgb(255, 242, 205, 16));
    sdlx_render_fill_circle(mx, my, 70, ss_rgb(255, 246, 214, 22));
    sdlx_render_fill_circle(mx, my, 48, ss_rgb(255, 250, 236, 255));
    sdlx_render_fill_circle(mx - 14, my - 12, 8, ss_rgb(226, 218, 194, 255));
    sdlx_render_fill_circle(mx + 16, my + 4, 5, ss_rgb(232, 224, 200, 255));
    sdlx_render_fill_circle(mx - 2, my + 14, 3, ss_rgb(236, 228, 206, 255));

    for (i = 0; i < 7; i++) {
        int y = horizon + 28 + i * 42;
        int a = 42 - i * 5;
        int rad = 9 - i / 2;
        int x = mx + (int)(10.0 * sin(t * 1.6 + i * 0.7));
        if (y >= h) break;
        if (rad < 2) rad = 2;
        if (a < 8) a = 8;
        sdlx_render_fill_circle(x, y, rad, ss_rgb(220, 220, 190, a));
    }

    for (i = 0; i < 6; i++) {
        int y = horizon + 70 + i * 100;
        int x = 140 + (int)(46.0 * sin(t * 1.15 + i * 0.85));
        int ww = 500 + (int)(36.0 * sin(t * 0.6 + i));
        if (y >= h) break;
        sdlx_render_fill_rect(x, y, ww, 2, ss_rgb(130, 210, 190, 32));
    }
}
