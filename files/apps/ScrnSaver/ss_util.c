#include "apps/ScrnSaver/common.h"

// -----------------  COLOR  -------------------------------------------

int ss_clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

sdlx_color_t ss_rgb(int r, int g, int b, int a)
{
    return sdlx_create_color(ss_clampi(r, 0, 255),
                             ss_clampi(g, 0, 255),
                             ss_clampi(b, 0, 255),
                             ss_clampi(a, 0, 255));
}

// hue is degrees and may be any value; it wraps.
// sat and val are 0..1.
sdlx_color_t ss_hsv(double hue, double sat, double val, int a)
{
    double c;
    double x;
    double m;
    double r;
    double g;
    double b;
    double hp;
    int sector;

    if (sat < 0.0) sat = 0.0;
    if (sat > 1.0) sat = 1.0;
    if (val < 0.0) val = 0.0;
    if (val > 1.0) val = 1.0;

    hue = hue - 360.0 * floor(hue / 360.0);
    if (hue < 0.0) {
        hue = hue + 360.0;
    }

    c = val * sat;
    hp = hue / 60.0;
    x = c * (1.0 - fabs(fmod(hp, 2.0) - 1.0));
    m = val - c;

    r = 0.0;
    g = 0.0;
    b = 0.0;
    sector = (int)hp;
    if (sector < 0) sector = 0;
    if (sector > 5) sector = 5;

    if (sector == 0) {
        r = c;
        g = x;
        b = 0.0;
    } else if (sector == 1) {
        r = x;
        g = c;
        b = 0.0;
    } else if (sector == 2) {
        r = 0.0;
        g = c;
        b = x;
    } else if (sector == 3) {
        r = 0.0;
        g = x;
        b = c;
    } else if (sector == 4) {
        r = x;
        g = 0.0;
        b = c;
    } else {
        r = c;
        g = 0.0;
        b = x;
    }

    return ss_rgb((int)((r + m) * 255.0),
                   (int)((g + m) * 255.0),
                   (int)((b + m) * 255.0),
                   a);
}

// -----------------  SOFT DOT  ----------------------------------------

void ss_orb(int x, int y, int rad, int r, int g, int b, int a)
{
    int halo;
    int core;

    if (rad < 1) rad = 1;
    if (a < 1) return;

    halo = a / 5;
    if (halo < 10) halo = 10;
    if (halo > a) halo = a;

    core = rad / 2;
    if (core < 1) core = 1;

    sdlx_render_fill_circle(x, y, rad, ss_rgb(r, g, b, halo));
    sdlx_render_fill_circle(x, y, core, ss_rgb(r, g, b, a));
}
