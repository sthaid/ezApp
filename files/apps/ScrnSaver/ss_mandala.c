#include "apps/ScrnSaver/common.h"

// Three geometric figures on a vertical axis. Hue drifts so the
// palette stays related and slowly walks the color wheel.

#define MAN_MAX 12

static void mandala_rosette(int cx, int cy, int radius, double spin,
                            double hue, int petals, int chords);
static void mandala_star(int cx, int cy, int radius, double spin,
                         double hue, int points);
static void mandala_jewel(int cx, int cy, int radius, double hue);
static void mandala_sparks(int cx, int cy, int radius, double spin,
                           double hue, int count, int size);

// -----------------  ENTRY  ------------------------------------------

void ss_mandala(int op, double t)
{
    int w;
    int h;
    int cx;
    int cy;
    int top;
    int bot;
    double hue;
    double pulse;
    int main_r;

    if (op != SS_OP_DRAW) {
        return;
    }

    w = sdlx_win_width;
    h = sdlx_win_height;
    cx = w / 2;
    cy = h / 2;
    top = 360;
    bot = h - 360;
    hue = t * 9.0;
    pulse = 1.0 + 0.035 * sin(t * 0.7);
    main_r = (int)(390 * pulse);

    sdlx_render_line(cx, 160, cx, h - 160, ss_hsv(hue, 0.30, 0.40, 55));

    sdlx_render_fill_circle(cx, cy, 540, ss_rgb(26, 8, 52, 32));
    sdlx_render_fill_circle(cx, cy, 280, ss_rgb(42, 12, 78, 26));
    sdlx_render_fill_circle(cx, top, 190, ss_rgb(20, 10, 48, 36));
    sdlx_render_fill_circle(cx, bot, 190, ss_rgb(20, 10, 48, 36));

    mandala_sparks(cx, cy, main_r + 70, t * 7.0, hue, 28, 1);
    mandala_rosette(cx, cy, main_r, t * 14.0, hue, 12, 1);
    mandala_sparks(cx, cy, main_r * 72 / 100, -t * 12.0, hue + 30.0, 16, 0);
    mandala_star(cx, cy, (int)(240 * pulse), -t * 20.0, hue + 38.0, 6);
    mandala_star(cx, cy, (int)(142 * pulse), t * 26.0, hue + 188.0, 5);
    mandala_jewel(cx, cy, 64, hue);

    for (int i = 0; i < 3; i++) {
        double a = t * 32.0 + i * 120.0;
        int x = cx + (int)(cosd(a) * (main_r + 28));
        int y = cy + (int)(sind(a) * (main_r + 28));
        sdlx_render_fill_circle(x, y, 16, ss_hsv(hue + i * 36.0, 0.35, 1.0, 45));
        sdlx_render_fill_circle(x, y, 6, ss_hsv(hue + i * 36.0, 0.80, 1.0, 255));
    }

    mandala_rosette(cx, top, (int)(142 * pulse), -t * 18.0, hue + 170.0, 8, 0);
    mandala_jewel(cx, top, 26, hue + 170.0);
    mandala_rosette(cx, bot, (int)(142 * pulse), t * 18.0, hue + 205.0, 8, 0);
    mandala_jewel(cx, bot, 26, hue + 205.0);
}

// -----------------  FIGURES  ----------------------------------------

static void mandala_rosette(int cx, int cy, int radius, double spin,
                            double hue, int petals, int chords)
{
    int px[MAN_MAX];
    int py[MAN_MAX];
    int i;
    int dot;
    int step;
    sdlx_color_t spoke;
    sdlx_color_t chord;

    if (petals < 3) petals = 3;
    if (petals > MAN_MAX) petals = MAN_MAX;
    if (radius < 8) return;

    dot = radius / 24;
    if (dot < 3) dot = 3;
    if (dot > 14) dot = 14;

    sdlx_render_circle(cx, cy, radius, 2, ss_hsv(hue, 0.50, 0.72, 160));
    sdlx_render_circle(cx, cy, radius * 58 / 100, 2, ss_hsv(hue + 24.0, 0.60, 0.78, 130));

    for (i = 0; i < petals; i++) {
        double a = spin + i * (360.0 / petals);
        px[i] = cx + (int)(cosd(a) * radius);
        py[i] = cy + (int)(sind(a) * radius);
    }

    spoke = ss_hsv(hue + 8.0, 0.40, 0.85, 100);
    for (i = 0; i < petals; i++) {
        sdlx_render_line(cx, cy, px[i], py[i], spoke);
    }

    if (chords) {
        step = petals / 2 - 1;
        if (step < 2) step = 2;
        chord = ss_hsv(hue + 42.0, 0.75, 0.95, 170);
        for (i = 0; i < petals; i++) {
            int j = (i + step) % petals;
            sdlx_render_line(px[i], py[i], px[j], py[j], chord);
        }
    }

    for (i = 0; i < petals; i++) {
        sdlx_render_fill_circle(px[i], py[i], dot + 5, ss_hsv(hue + 18.0, 0.70, 1.0, 36));
        sdlx_render_fill_circle(px[i], py[i], dot, ss_hsv(hue + 18.0, 0.85, 1.0, 235));
    }
}

static void mandala_star(int cx, int cy, int radius, double spin,
                         double hue, int points)
{
    int px[6];
    int py[6];
    int i;
    sdlx_color_t col;

    if (points < 5) points = 5;
    if (points > 6) points = 6;
    if (radius < 8) return;

    for (i = 0; i < points; i++) {
        double a = spin + i * (360.0 / points) - 90.0;
        px[i] = cx + (int)(cosd(a) * radius);
        py[i] = cy + (int)(sind(a) * radius);
    }

    col = ss_hsv(hue, 0.82, 1.0, 210);
    for (i = 0; i < points; i++) {
        int j = (i + 2) % points;
        sdlx_render_line(px[i], py[i], px[j], py[j], col);
        sdlx_render_line(px[i] + 1, py[i], px[j] + 1, py[j], col);
    }

    for (i = 0; i < points; i++) {
        sdlx_render_fill_circle(px[i], py[i], 5, ss_hsv(hue, 0.30, 1.0, 255));
    }
}

static void mandala_jewel(int cx, int cy, int radius, double hue)
{
    if (radius < 4) return;
    sdlx_render_fill_circle(cx, cy, radius, ss_hsv(hue, 0.55, 0.35, 70));
    sdlx_render_circle(cx, cy, radius * 70 / 100, 2, ss_hsv(hue + 15.0, 0.25, 1.0, 180));
    sdlx_render_fill_circle(cx, cy, radius * 36 / 100, ss_hsv(hue + 8.0, 0.15, 1.0, 255));
}

static void mandala_sparks(int cx, int cy, int radius, double spin,
                           double hue, int count, int size)
{
    int i;

    if (count < 1) return;
    for (i = 0; i < count; i++) {
        double a = spin + i * (360.0 / count);
        int x = cx + (int)(cosd(a) * radius);
        int y = cy + (int)(sind(a) * radius);
        sdlx_render_point(x, y, ss_hsv(hue + i * (360.0 / count), 0.75, 1.0, 220), size);
    }
}
