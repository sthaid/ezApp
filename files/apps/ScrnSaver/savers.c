#include "apps/ScrnSaver/common.h"

// ---------------------------------------------------------------
// Plug in a screen saver
// ---------------------------------------------------------------
// PicoC does not support function pointers, so each saver is a
// normal function and this file is the registry.
//
// 1. Create ss_<name>.c that defines:
//        void ss_<name>(int op, double t)
//    Handle SS_OP_INIT, SS_OP_DRAW, and SS_OP_CLEANUP.
//    INIT may allocate; CLEANUP must free it.
//    DRAW paints one frame. t is seconds since selection.
//    Copy ss_stars.c as a starting point.
// 2. Declare the function below.
// 3. Add its display name in saver_name().
// 4. Add a branch in saver_dispatch().
// 5. Increase SS_COUNT.
//
// The new .c file is picked up automatically. eztest compiles
// every .c file in this directory.
// ---------------------------------------------------------------

#define SS_COUNT 3

void ss_stars(int op, double t);
void ss_aurora(int op, double t);
void ss_mandala(int op, double t);

int saver_count(void)
{
    return SS_COUNT;
}

char *saver_name(int index)
{
    if (index == 0) return "Stars";
    if (index == 1) return "Aurora";
    if (index == 2) return "Mandala";
    return "";
}

void saver_dispatch(int index, int op, double t)
{
    if (index == 0) {
        ss_stars(op, t);
    } else if (index == 1) {
        ss_aurora(op, t);
    } else if (index == 2) {
        ss_mandala(op, t);
    }
}
