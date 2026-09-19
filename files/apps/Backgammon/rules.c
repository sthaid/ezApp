#include <apps/Backgammon/common.h>

static void add_step(step_list_t *sl, int from, int to, int die);
static void add_play(play_list_t *pl, step_t *cur, int nsteps);
static void gen_rec(board_t *b, int side, int *remain, int nremain,
                    step_t *cur, int nsteps, play_list_t *pl);
static void filter_plays(play_list_t *pl, int nremain, int r0, int r1);

// -----------------  COPY / SIDE  -----------------------------

void board_copy(board_t *dst, board_t *src)
{
    int i;

    for (i = 0; i < 25; i++) {
        dst->point[i] = src->point[i];
    }
    dst->bar[0] = src->bar[0];
    dst->bar[1] = src->bar[1];
    dst->off[0] = src->off[0];
    dst->off[1] = src->off[1];
    dst->side_to_move = src->side_to_move;
    dst->dice[0] = src->dice[0];
    dst->dice[1] = src->dice[1];
    dst->nremain = src->nremain;
    for (i = 0; i < MAX_STEPS; i++) {
        dst->remain[i] = src->remain[i];
    }
}

void copy_step(step_t *dst, step_t *src)
{
    dst->from = src->from;
    dst->to = src->to;
    dst->die = src->die;
}

void copy_play(play_t *dst, play_t *src)
{
    int i;

    dst->nsteps = src->nsteps;
    for (i = 0; i < MAX_STEPS; i++) {
        copy_step(&dst->step[i], &src->step[i]);
    }
}

int other_side(int side)
{
    if (side == SIDE_HUMAN) {
        return SIDE_CPU;
    }
    return SIDE_HUMAN;
}

// -----------------  SETUP  -----------------------------------

void board_init(board_t *b)
{
    int p;

    for (p = 0; p < 25; p++) {
        b->point[p] = 0;
    }
    b->point[24] = 2;
    b->point[13] = 5;
    b->point[8] = 3;
    b->point[6] = 5;
    b->point[1] = -2;
    b->point[12] = -5;
    b->point[17] = -3;
    b->point[19] = -5;
    b->bar[SIDE_HUMAN] = 0;
    b->bar[SIDE_CPU] = 0;
    b->off[SIDE_HUMAN] = 0;
    b->off[SIDE_CPU] = 0;
    b->side_to_move = SIDE_HUMAN;
    b->dice[0] = 0;
    b->dice[1] = 0;
    b->nremain = 0;
    for (p = 0; p < MAX_STEPS; p++) {
        b->remain[p] = 0;
    }
}

int roll_die(void)
{
    return (int)(random() % 6) + 1;
}

void set_dice(board_t *b, int d1, int d2)
{
    int i;

    b->dice[0] = d1;
    b->dice[1] = d2;
    if (d1 == d2) {
        b->nremain = 4;
        for (i = 0; i < 4; i++) {
            b->remain[i] = d1;
        }
    } else {
        b->nremain = 2;
        b->remain[0] = d1;
        b->remain[1] = d2;
        b->remain[2] = 0;
        b->remain[3] = 0;
    }
}

void opening_roll(board_t *b)
{
    int dh, dc;

    dh = roll_die();
    dc = roll_die();
    while (dh == dc) {
        dh = roll_die();
        dc = roll_die();
    }
    set_dice(b, dh, dc);
    if (dh > dc) {
        b->side_to_move = SIDE_HUMAN;
    } else {
        b->side_to_move = SIDE_CPU;
    }
}

void roll_turn_dice(board_t *b)
{
    set_dice(b, roll_die(), roll_die());
}

void consume_die(board_t *b, int die)
{
    int i, j, found;

    found = 0;
    for (i = 0; i < b->nremain; i++) {
        if (found) {
            continue;
        }
        if (b->remain[i] == die) {
            for (j = i; j < b->nremain - 1; j++) {
                b->remain[j] = b->remain[j + 1];
            }
            b->nremain--;
            if (b->nremain < 0) {
                b->nremain = 0;
            }
            found = 1;
        }
    }
}

// -----------------  QUERIES  ---------------------------------

bool is_blocked(board_t *b, int side, int pt)
{
    if (pt < 1) {
        return true;
    }
    if (pt > 24) {
        return true;
    }
    if (side == SIDE_HUMAN) {
        if (b->point[pt] <= -2) {
            return true;
        }
        return false;
    }
    if (b->point[pt] >= 2) {
        return true;
    }
    return false;
}

bool all_in_home(board_t *b, int side)
{
    int p;

    if (b->bar[side] > 0) {
        return false;
    }
    if (side == SIDE_HUMAN) {
        for (p = 7; p <= 24; p++) {
            if (b->point[p] > 0) {
                return false;
            }
        }
        return true;
    }
    for (p = 1; p <= 18; p++) {
        if (b->point[p] < 0) {
            return false;
        }
    }
    return true;
}

int pip_count(board_t *b, int side)
{
    int p, pips;

    pips = b->bar[side] * 25;
    if (side == SIDE_HUMAN) {
        for (p = 1; p <= 24; p++) {
            if (b->point[p] > 0) {
                pips += b->point[p] * p;
            }
        }
    } else {
        for (p = 1; p <= 24; p++) {
            if (b->point[p] < 0) {
                pips += (-b->point[p]) * (25 - p);
            }
        }
    }
    return pips;
}

bool has_contact(board_t *b)
{
    int p, hum_max, cpu_min;

    if (b->bar[SIDE_HUMAN] > 0) {
        return true;
    }
    if (b->bar[SIDE_CPU] > 0) {
        return true;
    }

    hum_max = 0;
    cpu_min = 25;
    for (p = 1; p <= 24; p++) {
        if (b->point[p] > 0) {
            if (p > hum_max) {
                hum_max = p;
            }
        }
        if (b->point[p] < 0) {
            if (p < cpu_min) {
                cpu_min = p;
            }
        }
    }
    if (hum_max == 0) {
        return false;
    }
    if (cpu_min == 25) {
        return false;
    }
    if (hum_max > cpu_min) {
        return true;
    }
    return false;
}

int game_winner(board_t *b)
{
    if (b->off[SIDE_HUMAN] >= N_CHECKERS) {
        return SIDE_HUMAN;
    }
    if (b->off[SIDE_CPU] >= N_CHECKERS) {
        return SIDE_CPU;
    }
    return -1;
}

bool has_legal_play(play_list_t *pl)
{
    if (pl->max <= 0) {
        return false;
    }
    if (pl->play[0].nsteps <= 0) {
        return false;
    }
    return true;
}

// -----------------  APPLY  -----------------------------------

void apply_step(board_t *b, int side, step_t *s)
{
    if (s->from == FROM_BAR) {
        b->bar[side]--;
    } else if (side == SIDE_HUMAN) {
        b->point[s->from]--;
    } else {
        b->point[s->from]++;
    }

    if (s->to == TO_OFF) {
        b->off[side]++;
        return;
    }

    if (side == SIDE_HUMAN) {
        if (b->point[s->to] == -1) {
            b->point[s->to] = 0;
            b->bar[SIDE_CPU]++;
        }
        b->point[s->to]++;
    } else {
        if (b->point[s->to] == 1) {
            b->point[s->to] = 0;
            b->bar[SIDE_HUMAN]++;
        }
        b->point[s->to]--;
    }
}

void apply_play(board_t *b, int side, play_t *p)
{
    int i;

    for (i = 0; i < p->nsteps; i++) {
        apply_step(b, side, &p->step[i]);
    }
}

// -----------------  SINGLE-DIE STEPS  ------------------------

static void add_step(step_list_t *sl, int from, int to, int die)
{
    if (sl->max >= MAX_STEPS_ONE_DIE) {
        return;
    }
    sl->step[sl->max].from = from;
    sl->step[sl->max].to = to;
    sl->step[sl->max].die = die;
    sl->max++;
}

void generate_steps(board_t *b, int side, int die, step_list_t *sl)
{
    int p, dest, hi, lo, q;

    sl->max = 0;
    if (die < 1) {
        return;
    }
    if (die > 6) {
        return;
    }

    if (b->bar[side] > 0) {
        if (side == SIDE_HUMAN) {
            dest = 25 - die;
        } else {
            dest = die;
        }
        if (!is_blocked(b, side, dest)) {
            add_step(sl, FROM_BAR, dest, die);
        }
        return;
    }

    if (side == SIDE_HUMAN) {
        for (p = 1; p <= 24; p++) {
            if (b->point[p] <= 0) {
                continue;
            }
            dest = p - die;
            if (dest >= 1) {
                if (!is_blocked(b, side, dest)) {
                    add_step(sl, p, dest, die);
                }
            } else if (all_in_home(b, side)) {
                if (dest == 0) {
                    add_step(sl, p, TO_OFF, die);
                } else {
                    hi = 0;
                    for (q = 6; q >= 1; q--) {
                        if (b->point[q] > 0) {
                            hi = q;
                            break;
                        }
                    }
                    if (p == hi) {
                        add_step(sl, p, TO_OFF, die);
                    }
                }
            }
        }
        return;
    }

    for (p = 1; p <= 24; p++) {
        if (b->point[p] >= 0) {
            continue;
        }
        dest = p + die;
        if (dest <= 24) {
            if (!is_blocked(b, side, dest)) {
                add_step(sl, p, dest, die);
            }
        } else if (all_in_home(b, side)) {
            if (dest == 25) {
                add_step(sl, p, TO_OFF, die);
            } else {
                lo = 25;
                for (q = 19; q <= 24; q++) {
                    if (b->point[q] < 0) {
                        if (q < lo) {
                            lo = q;
                        }
                    }
                }
                if (p == lo) {
                    add_step(sl, p, TO_OFF, die);
                }
            }
        }
    }
}

// -----------------  COMPLETE PLAYS  --------------------------

static void add_play(play_list_t *pl, step_t *cur, int nsteps)
{
    int i;

    if (pl->max >= MAX_PLAYS) {
        return;
    }
    pl->play[pl->max].nsteps = nsteps;
    for (i = 0; i < MAX_STEPS; i++) {
        if (i < nsteps) {
            copy_step(&pl->play[pl->max].step[i], &cur[i]);
        } else {
            pl->play[pl->max].step[i].from = 0;
            pl->play[pl->max].step[i].to = 0;
            pl->play[pl->max].step[i].die = 0;
        }
    }
    pl->max++;
}

static void gen_rec(board_t *b, int side, int *remain, int nremain,
                    step_t *cur, int nsteps, play_list_t *pl)
{
    int i, j, k, die, already, any, nnew;
    int new_remain[MAX_STEPS];
    board_t child;
    step_list_t sl;

    if (nremain <= 0) {
        add_play(pl, cur, nsteps);
        return;
    }

    any = 0;
    for (i = 0; i < nremain; i++) {
        die = remain[i];
        already = 0;
        for (j = 0; j < i; j++) {
            if (remain[j] == die) {
                already = 1;
            }
        }
        if (already) {
            continue;
        }

        generate_steps(b, side, die, &sl);
        for (k = 0; k < sl.max; k++) {
            any = 1;
            board_copy(&child, b);
            apply_step(&child, side, &sl.step[k]);
            nnew = 0;
            for (j = 0; j < nremain; j++) {
                if (j != i) {
                    new_remain[nnew] = remain[j];
                    nnew++;
                }
            }
            copy_step(&cur[nsteps], &sl.step[k]);
            gen_rec(&child, side, new_remain, nnew, cur, nsteps + 1, pl);
        }
    }

    if (!any) {
        add_play(pl, cur, nsteps);
    }
}

static void filter_plays(play_list_t *pl, int nremain, int r0, int r1)
{
    int i, j, maxn, hi, has_hi;

    maxn = 0;
    for (i = 0; i < pl->max; i++) {
        if (pl->play[i].nsteps > maxn) {
            maxn = pl->play[i].nsteps;
        }
    }

    j = 0;
    for (i = 0; i < pl->max; i++) {
        if (pl->play[i].nsteps == maxn) {
            if (j != i) {
                copy_play(&pl->play[j], &pl->play[i]);
            }
            j++;
        }
    }
    pl->max = j;

    if (maxn != 1) {
        return;
    }
    if (nremain != 2) {
        return;
    }
    if (r0 == r1) {
        return;
    }

    hi = r0;
    if (r1 > hi) {
        hi = r1;
    }
    has_hi = 0;
    for (i = 0; i < pl->max; i++) {
        if (pl->play[i].step[0].die == hi) {
            has_hi = 1;
        }
    }
    if (!has_hi) {
        return;
    }

    j = 0;
    for (i = 0; i < pl->max; i++) {
        if (pl->play[i].step[0].die == hi) {
            if (j != i) {
                copy_play(&pl->play[j], &pl->play[i]);
            }
            j++;
        }
    }
    pl->max = j;
}

void generate_plays(board_t *b, int side, play_list_t *pl)
{
    step_t cur[MAX_STEPS];
    int r0, r1, i;

    pl->max = 0;
    for (i = 0; i < MAX_STEPS; i++) {
        cur[i].from = 0;
        cur[i].to = 0;
        cur[i].die = 0;
    }

    if (b->nremain <= 0) {
        return;
    }

    r0 = b->remain[0];
    r1 = 0;
    if (b->nremain >= 2) {
        r1 = b->remain[1];
    }

    gen_rec(b, side, b->remain, b->nremain, cur, 0, pl);
    filter_plays(pl, b->nremain, r0, r1);
}
