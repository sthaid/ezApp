#ifndef __SCRNSAVER_COMMON_H__
#define __SCRNSAVER_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"

// Each saver implements:
//   void ss_<name>(int op, double t);
// op is one of the values below.
// t is seconds since this saver was selected (0 on INIT).
// Draw after the caller has called sdlx_display_init.
// The drawable area is sdlx_win_width by sdlx_win_height.
// Portrait size is 1000 x 2000. The control bar sits below that.
#define SS_OP_INIT     1
#define SS_OP_DRAW     2
#define SS_OP_CLEANUP  3

char *progname;
char *data_dir;

int  ss_clampi(int v, int lo, int hi);
sdlx_color_t ss_rgb(int r, int g, int b, int a);
sdlx_color_t ss_hsv(double hue, double sat, double val, int a);

// Soft dot: wide translucent disk plus a brighter core.
void ss_orb(int x, int y, int rad, int r, int g, int b, int a);

// Registry. See savers.c for how to plug in another saver.
int   saver_count(void);
char *saver_name(int index);
void  saver_dispatch(int index, int op, double t);

#endif
