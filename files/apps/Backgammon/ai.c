#include <apps/Backgammon/common.h>

#define INFIN        100000000
#define WIN_SCORE    1000000
#define TOP_MEDIUM   4
#define TOP_HARD     8
#define BEAM_PLAY    3
#define BEAM_DBL_MED 2
#define BEAM_DBL_HARD 3

// Easy is 1-ply. Medium and Hard rank moves with the full
// heuristic, then run a 2-ply expectimax over the best few.
// Replies are searched directly. Doubles (four moves) use a
// beam so the interpreter stays responsive. Pure races skip
// the second ply. Hard keeps more candidate moves and a wider
// doubles beam than Medium.

static play_list_t cpu_pl;
static int pscore[MAX_PLAYS];
static int pord[MAX_PLAYS];
static board_t ranked_child[TOP_HARD];
static int kept_idx[TOP_HARD];

static int feat_ready;
static int feat[25][31];

static int pick_best_1ply_play(board_t *b, int side, play_list_t *pl, play_t *out);
static void init_feat(void);
static int eval_fast(board_t *b);
static int step_rank(board_t *b, int side, step_t *s);
static void select_beam(int *rank, int n, int side, int beam, int *keep, int *nkeep);
static void add_hits(step_t *s, int n, int *keep, int *nkeep, int cap);
static void consider_fast(board_t *b, int side, int nsteps, int die0,
                          int *maxn, int *has_hi, int hi, int *best);
static void finish_second(board_t *b, int side, int d1, int d2, int hi,
                          int *maxn, int *has_hi, int *best);
static void play_order(board_t *b, int side, int d1, int d2, int hi, int beam,
                       int *maxn, int *has_hi, int *best);
static void play_same(board_t *b, int side, int die, int left, int nsteps,
                      int beam, int *maxn, int *has_hi, int *best);
static int reply_score(board_t *b, int side, int beam);

int difficulty_to_depth(int difficulty)
{
    if (difficulty == DIFF_EASY) {
        return DEPTH_EASY;
    }
    if (difficulty == DIFF_HARD) {
        return DEPTH_HARD;
    }
    return DEPTH_MEDIUM;
}

// Score from CPU perspective: positive favors the CPU.
// Single pass over the 24 points: pips, contact, blots, anchors, primes.
int evaluate_board(board_t *b)
{
    int score, ph, pc, p, n;
    int pr_cpu, pr_hum, run_c, run_h;
    int hum_max, cpu_min, contact;

    if (b->off[SIDE_CPU] >= N_CHECKERS) {
        return WIN_SCORE;
    }
    if (b->off[SIDE_HUMAN] >= N_CHECKERS) {
        return -WIN_SCORE;
    }

    ph = b->bar[SIDE_HUMAN] * 25;
    pc = b->bar[SIDE_CPU] * 25;
    hum_max = 0;
    cpu_min = 25;
    pr_cpu = 0;
    pr_hum = 0;
    run_c = 0;
    run_h = 0;
    score = 0;

    for (p = 1; p <= 24; p++) {
        n = b->point[p];
        if (n > 0) {
            ph += n * p;
            if (p > hum_max) {
                hum_max = p;
            }
            if (n == 1) {
                if (p <= 6) {
                    score += 20;
                } else if (p >= 19) {
                    score += 12;
                } else {
                    score += 8;
                }
            }
            if (n >= 2) {
                run_h++;
                if (run_h > pr_hum) {
                    pr_hum = run_h;
                }
                if (p <= 6) {
                    score -= 14;
                }
                if (p >= 19) {
                    score -= 28;
                }
            } else {
                run_h = 0;
            }
            run_c = 0;
        } else if (n < 0) {
            pc += (-n) * (25 - p);
            if (p < cpu_min) {
                cpu_min = p;
            }
            if (n == -1) {
                if (p >= 19) {
                    score -= 20;
                } else if (p <= 6) {
                    score -= 12;
                } else {
                    score -= 8;
                }
            }
            if (n <= -2) {
                run_c++;
                if (run_c > pr_cpu) {
                    pr_cpu = run_c;
                }
                if (p <= 6) {
                    score += 28;
                }
                if (p >= 19) {
                    score += 14;
                }
            } else {
                run_c = 0;
            }
            run_h = 0;
        } else {
            run_h = 0;
            run_c = 0;
        }
    }

    contact = 0;
    if (b->bar[SIDE_HUMAN] > 0) {
        contact = 1;
    }
    if (b->bar[SIDE_CPU] > 0) {
        contact = 1;
    }
    if (contact == 0) {
        if (hum_max != 0) {
            if (cpu_min != 25) {
                if (hum_max > cpu_min) {
                    contact = 1;
                }
            }
        }
    }

    if (contact == 0) {
        return (ph - pc) * 5 + (b->off[SIDE_CPU] - b->off[SIDE_HUMAN]) * 40;
    }

    score += (ph - pc) * 2;
    score += (b->off[SIDE_CPU] - b->off[SIDE_HUMAN]) * 30;
    score += b->bar[SIDE_HUMAN] * 80;
    score -= b->bar[SIDE_CPU] * 90;
    score += pr_cpu * 10;
    score -= pr_hum * 10;
    if (pr_cpu >= 6) {
        score += 40;
    }
    if (pr_hum >= 6) {
        score -= 40;
    }

    return score;
}

// Contact-phase features as a lookup, so a reply search does not
// re-branch on every point. Prime length stays in evaluate_board,
// which is only used to rank the moves we might play.
static void init_feat(void)
{
    int p, c, n, s;

    if (feat_ready) {
        return;
    }
    for (p = 1; p <= 24; p++) {
        for (c = -15; c <= 15; c++) {
            n = c;
            s = 0;
            if (n > 0) {
                s += n * p * 2;
                if (n == 1) {
                    if (p <= 6) {
                        s += 20;
                    } else if (p >= 19) {
                        s += 12;
                    } else {
                        s += 8;
                    }
                } else {
                    if (p <= 6) {
                        s -= 14;
                    }
                    if (p >= 19) {
                        s -= 28;
                    }
                }
            } else if (n < 0) {
                s -= (-n) * (25 - p) * 2;
                if (n == -1) {
                    if (p >= 19) {
                        s -= 20;
                    } else if (p <= 6) {
                        s -= 12;
                    } else {
                        s -= 8;
                    }
                } else {
                    if (p <= 6) {
                        s += 28;
                    }
                    if (p >= 19) {
                        s += 14;
                    }
                }
            }
            feat[p][n + 15] = s;
        }
    }
    feat_ready = 1;
}

static int eval_fast(board_t *b)
{
    int p, s;

    init_feat();
    if (b->off[SIDE_CPU] >= N_CHECKERS) {
        return WIN_SCORE;
    }
    if (b->off[SIDE_HUMAN] >= N_CHECKERS) {
        return -WIN_SCORE;
    }
    s = 0;
    for (p = 1; p <= 24; p++) {
        s += feat[p][b->point[p] + 15];
    }
    s += b->bar[SIDE_HUMAN] * (25 * 2 + 80);
    s -= b->bar[SIDE_CPU] * (25 * 2 + 90);
    s += (b->off[SIDE_CPU] - b->off[SIDE_HUMAN]) * 30;
    return s;
}

static int step_rank(board_t *b, int side, step_t *s)
{
    board_t snap;
    int sc;

    board_copy(&snap, b);
    apply_step(b, side, s);
    sc = eval_fast(b);
    board_copy(b, &snap);
    return sc;
}

static void select_beam(int *rank, int n, int side, int beam, int *keep, int *nkeep)
{
    int i, j, worst;

    *nkeep = 0;
    for (i = 0; i < n; i++) {
        if (*nkeep < beam) {
            keep[*nkeep] = i;
            *nkeep = *nkeep + 1;
            continue;
        }
        worst = 0;
        for (j = 1; j < *nkeep; j++) {
            if (side == SIDE_CPU) {
                if (rank[keep[j]] < rank[keep[worst]]) {
                    worst = j;
                }
            } else {
                if (rank[keep[j]] > rank[keep[worst]]) {
                    worst = j;
                }
            }
        }
        if (side == SIDE_CPU) {
            if (rank[i] > rank[keep[worst]]) {
                keep[worst] = i;
            }
        } else {
            if (rank[i] < rank[keep[worst]]) {
                keep[worst] = i;
            }
        }
    }
}

static void add_hits(step_t *s, int n, int *keep, int *nkeep, int cap)
{
    int i, j, seen;

    for (i = 0; i < n; i++) {
        if (s[i].hit == 0) {
            continue;
        }
        seen = 0;
        for (j = 0; j < *nkeep; j++) {
            if (keep[j] == i) {
                seen = 1;
            }
        }
        if (seen) {
            continue;
        }
        if (*nkeep >= cap) {
            return;
        }
        keep[*nkeep] = i;
        *nkeep = *nkeep + 1;
    }
}

static void consider_fast(board_t *b, int side, int nsteps, int die0,
                          int *maxn, int *has_hi, int hi, int *best)
{
    int sc, use;

    if (nsteps < *maxn) {
        return;
    }
    if (nsteps > *maxn) {
        *maxn = nsteps;
        *has_hi = 0;
        if (nsteps == 1) {
            if (hi != 0) {
                if (die0 == hi) {
                    *has_hi = 1;
                }
            }
        }
        *best = eval_fast(b);
        return;
    }

    use = 1;
    if (*maxn == 1) {
        if (hi != 0) {
            if (die0 == hi) {
                if (*has_hi == 0) {
                    *has_hi = 1;
                    *best = eval_fast(b);
                    return;
                }
            } else {
                if (*has_hi) {
                    use = 0;
                }
            }
        }
    }
    if (use == 0) {
        return;
    }
    sc = eval_fast(b);
    if (side == SIDE_CPU) {
        if (sc > *best) {
            *best = sc;
        }
    } else {
        if (sc < *best) {
            *best = sc;
        }
    }
}

static void finish_second(board_t *b, int side, int d1, int d2, int hi,
                          int *maxn, int *has_hi, int *best)
{
    step_t s2[MAX_STEPS_ONE_DIE];
    step_list_t sl;
    board_t snap;
    int n2, j;

    generate_steps(b, side, d2, &sl);
    n2 = sl.max;
    if (n2 <= 0) {
        consider_fast(b, side, 1, d1, maxn, has_hi, hi, best);
        return;
    }
    for (j = 0; j < n2; j++) {
        memcpy(&s2[j], &sl.step[j], sizeof(step_t));
    }
    for (j = 0; j < n2; j++) {
        board_copy(&snap, b);
        apply_step(b, side, &s2[j]);
        consider_fast(b, side, 2, d1, maxn, has_hi, hi, best);
        board_copy(b, &snap);
    }
}

static void play_order(board_t *b, int side, int d1, int d2, int hi, int beam,
                       int *maxn, int *has_hi, int *best)
{
    step_t s1[MAX_STEPS_ONE_DIE];
    int rank[MAX_STEPS_ONE_DIE];
    int keep[MAX_STEPS_ONE_DIE];
    step_list_t sl;
    board_t snap;
    int n1, nkeep, i;

    generate_steps(b, side, d1, &sl);
    n1 = sl.max;
    if (n1 <= 0) {
        return;
    }
    for (i = 0; i < n1; i++) {
        memcpy(&s1[i], &sl.step[i], sizeof(step_t));
    }
    if (n1 <= beam) {
        nkeep = n1;
        for (i = 0; i < n1; i++) {
            keep[i] = i;
        }
    } else {
        for (i = 0; i < n1; i++) {
            rank[i] = step_rank(b, side, &s1[i]);
        }
        select_beam(rank, n1, side, beam, keep, &nkeep);
        add_hits(s1, n1, keep, &nkeep, beam + 2);
    }
    for (i = 0; i < nkeep; i++) {
        board_copy(&snap, b);
        apply_step(b, side, &s1[keep[i]]);
        finish_second(b, side, d1, d2, hi, maxn, has_hi, best);
        board_copy(b, &snap);
    }
}

static void play_same(board_t *b, int side, int die, int left, int nsteps,
                      int beam, int *maxn, int *has_hi, int *best)
{
    step_t s1[MAX_STEPS_ONE_DIE];
    int rank[MAX_STEPS_ONE_DIE];
    int keep[MAX_STEPS_ONE_DIE];
    step_list_t sl;
    board_t snap;
    int n1, nkeep, i;

    if (left <= 0) {
        consider_fast(b, side, nsteps, die, maxn, has_hi, 0, best);
        return;
    }
    generate_steps(b, side, die, &sl);
    n1 = sl.max;
    if (n1 <= 0) {
        if (nsteps > 0) {
            consider_fast(b, side, nsteps, die, maxn, has_hi, 0, best);
        }
        return;
    }
    for (i = 0; i < n1; i++) {
        memcpy(&s1[i], &sl.step[i], sizeof(step_t));
    }
    if (n1 <= beam) {
        nkeep = n1;
        for (i = 0; i < n1; i++) {
            keep[i] = i;
        }
    } else {
        for (i = 0; i < n1; i++) {
            rank[i] = step_rank(b, side, &s1[i]);
        }
        select_beam(rank, n1, side, beam, keep, &nkeep);
        add_hits(s1, n1, keep, &nkeep, beam + 2);
    }
    for (i = 0; i < nkeep; i++) {
        board_copy(&snap, b);
        apply_step(b, side, &s1[keep[i]]);
        play_same(b, side, die, left - 1, nsteps + 1, beam, maxn, has_hi, best);
        board_copy(b, &snap);
    }
}

static int reply_score(board_t *b, int side, int beam)
{
    int maxn, has_hi, best, d1, d2, hi;

    if (b->nremain <= 0) {
        return eval_fast(b);
    }
    d1 = b->remain[0];
    d2 = d1;
    if (b->nremain >= 2) {
        d2 = b->remain[1];
    }
    hi = 0;
    if (d1 != d2) {
        hi = d1;
        if (d2 > hi) {
            hi = d2;
        }
    }
    maxn = -1;
    has_hi = 0;
    best = 0;
    if (d1 == d2) {
        play_same(b, side, d1, b->nremain, 0, beam, &maxn, &has_hi, &best);
    } else {
        play_order(b, side, d1, d2, hi, beam, &maxn, &has_hi, &best);
        play_order(b, side, d2, d1, hi, beam, &maxn, &has_hi, &best);
    }
    if (maxn < 0) {
        return eval_fast(b);
    }
    return best;
}

static int pick_best_1ply_play(board_t *b, int side, play_list_t *pl, play_t *out)
{
    int i, sc, best, n_ties;
    board_t child;

    if (!has_legal_play(pl)) {
        return -1;
    }

    best = -INFIN;
    n_ties = 0;
    copy_play(out, &pl->play[0]);

    for (i = 0; i < pl->max; i++) {
        board_copy(&child, b);
        apply_play(&child, side, &pl->play[i]);
        sc = evaluate_board(&child);
        if (sc > best) {
            best = sc;
            n_ties = 1;
            copy_play(out, &pl->play[i]);
        } else if (sc == best) {
            n_ties++;
            if ((random() % n_ties) == 0) {
                copy_play(out, &pl->play[i]);
            }
        }
    }

    return 0;
}

int cpu_choose_play(board_t *b, int difficulty, play_t *out_play)
{
    int i, j, n, nkeep, t, best_i, limit, beam_d, beam_o, beam;
    int eq, d1, d2, w, best_eq, n_ties;
    board_t child, after_roll;

    generate_plays(b, SIDE_CPU, &cpu_pl);
    if (!has_legal_play(&cpu_pl)) {
        return -1;
    }

    if (difficulty == DIFF_EASY) {
        return pick_best_1ply_play(b, SIDE_CPU, &cpu_pl, out_play);
    }
    if (has_contact(b) == false) {
        return pick_best_1ply_play(b, SIDE_CPU, &cpu_pl, out_play);
    }

    if (difficulty == DIFF_HARD) {
        limit = TOP_HARD;
        beam_d = BEAM_DBL_HARD;
        beam_o = BEAM_PLAY;
    } else {
        limit = TOP_MEDIUM;
        beam_d = BEAM_DBL_MED;
        beam_o = BEAM_PLAY;
    }

    n = cpu_pl.max;
    for (i = 0; i < n; i++) {
        pord[i] = i;
        board_copy(&child, b);
        apply_play(&child, SIDE_CPU, &cpu_pl.play[i]);
        pscore[i] = evaluate_board(&child);
    }

    for (i = 0; i < n; i++) {
        best_i = i;
        for (j = i + 1; j < n; j++) {
            if (pscore[pord[j]] > pscore[pord[best_i]]) {
                best_i = j;
            }
        }
        t = pord[i];
        pord[i] = pord[best_i];
        pord[best_i] = t;
    }

    nkeep = 0;
    for (i = 0; i < n; i++) {
        if (nkeep >= limit) {
            break;
        }
        board_copy(&child, b);
        apply_play(&child, SIDE_CPU, &cpu_pl.play[pord[i]]);
        t = 0;
        for (j = 0; j < nkeep; j++) {
            if (memcmp(&ranked_child[j], &child, sizeof(board_t)) == 0) {
                t = 1;
            }
        }
        if (t) {
            continue;
        }
        board_copy(&ranked_child[nkeep], &child);
        kept_idx[nkeep] = pord[i];
        nkeep++;
    }
    if (nkeep <= 0) {
        return pick_best_1ply_play(b, SIDE_CPU, &cpu_pl, out_play);
    }
    if (game_winner(&ranked_child[0]) == SIDE_CPU) {
        copy_play(out_play, &cpu_pl.play[kept_idx[0]]);
        return 0;
    }
    if (nkeep == 1) {
        copy_play(out_play, &cpu_pl.play[kept_idx[0]]);
        return 0;
    }

    best_eq = -INFIN;
    n_ties = 0;
    copy_play(out_play, &cpu_pl.play[kept_idx[0]]);

    for (i = 0; i < nkeep; i++) {
        if (game_winner(&ranked_child[i]) == SIDE_CPU) {
            eq = WIN_SCORE * 36;
        } else {
            eq = 0;
            for (d1 = 1; d1 <= 6; d1++) {
                for (d2 = d1; d2 <= 6; d2++) {
                    if (d1 == d2) {
                        w = 1;
                        beam = beam_d;
                    } else {
                        w = 2;
                        beam = beam_o;
                    }
                    board_copy(&after_roll, &ranked_child[i]);
                    after_roll.side_to_move = SIDE_HUMAN;
                    set_dice(&after_roll, d1, d2);
                    eq += w * reply_score(&after_roll, SIDE_HUMAN, beam);
                }
            }
        }
        if (eq > best_eq) {
            best_eq = eq;
            n_ties = 1;
            copy_play(out_play, &cpu_pl.play[kept_idx[i]]);
        } else if (eq == best_eq) {
            n_ties++;
            if ((random() % n_ties) == 0) {
                copy_play(out_play, &cpu_pl.play[kept_idx[i]]);
            }
        }
    }

    return 0;
}
