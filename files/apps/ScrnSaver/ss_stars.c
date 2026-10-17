#include "apps/ScrnSaver/common.h"

// Flight through a star field. Stars are points in a unit box
// and projected from a vanishing point. Closer stars leave streaks.

#define STAR_N 110

typedef struct {
    double x;
    double y;
    double z;
    double speed;
    int tint;
} star_t;

static star_t stars[STAR_N];
static double stars_last_t;

static void stars_origin(int *cx, int *cy);
static void stars_project(double x, double y, double z, int *sx, int *sy);
static void stars_place(int i, double z, int random_z);
static void stars_color(int tint, int *r, int *g, int *b);
static void stars_init(double t);
static void stars_draw(double t);

// -----------------  ENTRY  ------------------------------------------

void ss_stars(int op, double t)
{
    if (op == SS_OP_INIT) {
        stars_init(t);
    } else if (op == SS_OP_DRAW) {
        stars_draw(t);
    }
}

// -----------------  SETUP  ------------------------------------------

static void stars_origin(int *cx, int *cy)
{
    *cx = sdlx_win_width / 2;
    *cy = sdlx_win_height * 42 / 100;
}

static void stars_project(double x, double y, double z, int *sx, int *sy)
{
    double k;
    int cx;
    int cy;

    if (z < 0.05) z = 0.05;
    k = 460.0 / z;
    stars_origin(&cx, &cy);
    *sx = cx + (int)(x * k);
    *sy = cy + (int)(y * k * 1.35);
}

static void stars_place(int i, double z, int random_z)
{
    int sx;
    int sy;
    int guard;
    int w;
    int h;

    w = sdlx_win_width;
    h = sdlx_win_height;
    sx = w / 2;
    sy = h / 2;

    for (guard = 0; guard < 8; guard++) {
        stars[i].x = (rand() % 2000 - 1000) / 1000.0;
        stars[i].y = (rand() % 2000 - 1000) / 1000.0;
        if (random_z) {
            z = 0.25 + (rand() % 750) / 1000.0;
        }
        stars[i].z = z;
        stars_project(stars[i].x, stars[i].y, z, &sx, &sy);
        if (sx > 20 && sx < w - 20 && sy > 20 && sy < h - 20) {
            break;
        }
    }

    stars[i].speed = 0.18 + (rand() % 100) / 320.0;
    stars[i].tint = rand() % 10;
}

static void stars_color(int tint, int *r, int *g, int *b)
{
    if (tint >= 9) {
        *r = 255;
        *g = 196;
        *b = 90;
    } else if (tint >= 7) {
        *r = 150;
        *g = 190;
        *b = 255;
    } else {
        *r = 235;
        *g = 240;
        *b = 255;
    }
}

static void stars_init(double t)
{
    int i;

    for (i = 0; i < STAR_N; i++) {
        stars_place(i, 1.0, 1);
    }
    stars_last_t = t;
}

// -----------------  FRAME  ------------------------------------------

static void stars_draw(double t)
{
    int i;
    int w;
    int h;
    int r;
    int g;
    int b;
    int bri;
    int sx;
    int sy;
    int bx;
    int by;
    double dt;
    double boost;
    double trail;
    double zback;
    sdlx_color_t col;

    w = sdlx_win_width;
    h = sdlx_win_height;

    dt = t - stars_last_t;
    stars_last_t = t;
    if (dt < 0.0) dt = 0.0;
    if (dt > 0.05) dt = 0.05;
    boost = 0.85 + 0.30 * sin(t * 0.35);

    for (i = 0; i < STAR_N; i++) {
        stars[i].z -= dt * stars[i].speed * boost;
        if (stars[i].z < 0.07) {
            stars_place(i, 1.0, 0);
            continue;
        }

        stars_project(stars[i].x, stars[i].y, stars[i].z, &sx, &sy);
        if (sx < -80 || sx > w + 80 || sy < -80 || sy > h + 80) {
            stars_place(i, 1.0, 0);
            continue;
        }

        bri = (int)((1.08 - stars[i].z) * 280.0);
        bri = ss_clampi(bri, 30, 255);
        stars_color(stars[i].tint, &r, &g, &b);
        col = ss_rgb(r, g, b, bri);

        // streak length is in depth, so it stays visible at any frame rate
        trail = 0.04 + (1.0 - stars[i].z) * 0.07;
        zback = stars[i].z + trail;
        stars_project(stars[i].x, stars[i].y, zback, &bx, &by);
        sdlx_render_line(bx, by, sx, sy, col);
        sdlx_render_point(sx, sy, ss_rgb(r, g, b, 255), stars[i].z < 0.45 ? 2 : 0);

        if (stars[i].z < 0.2) {
            int rad = (int)((0.28 - stars[i].z) * 40.0);
            if (rad < 2) rad = 2;
            ss_orb(sx, sy, rad, r, g, b, bri);
        }
    }
}
