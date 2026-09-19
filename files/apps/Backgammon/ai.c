#include <apps/Backgammon/common.h>

#define INFIN        100000000
#define WIN_SCORE    1000000
#define TOP_N_MEDIUM 8

// Hard later: 2-ply over all plays (no top-N prune) and/or truncated
// Monte Carlo rollouts. DIFF_HARD is reserved in common.h but is not
// offered in the UI. If selected, Medium behavior is used.

static play_list_t cpu_pl;
static play_list_t opp_pl;
static int pscore[MAX_PLAYS];
static int pord[MAX_PLAYS];

static int evaluate(board_t *b);
static int best_1ply_score(board_t *b, int side);
static int pick_best_1ply_play(board_t *b, int side, play_list_t *pl, play_t *out);
static int prime_len(board_t *b, int side);

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

static int prime_len(board_t *b, int side)
{
    int p, run, best, made;

    run = 0;
    best = 0;
    for (p = 1; p <= 24; p++) {
        made = 0;
        if (side == SIDE_HUMAN) {
            if (b->point[p] >= 2) {
                made = 1;
            }
        } else {
            if (b->point[p] <= -2) {
                made = 1;
            }
        }
        if (made) {
            run++;
            if (run > best) {
                best = run;
            }
        } else {
            run = 0;
        }
    }
    return best;
}

// Score from CPU perspective: positive favors the CPU.
static int evaluate(board_t *b)
{
    int score, ph, pc, p, n, winner;
    int pr_cpu, pr_hum;

    winner = game_winner(b);
    if (winner == SIDE_CPU) {
        return WIN_SCORE;
    }
    if (winner == SIDE_HUMAN) {
        return -WIN_SCORE;
    }

    ph = pip_count(b, SIDE_HUMAN);
    pc = pip_count(b, SIDE_CPU);

    if (!has_contact(b)) {
        score = (ph - pc) * 5;
        score += (b->off[SIDE_CPU] - b->off[SIDE_HUMAN]) * 40;
        return score;
    }

    score = (ph - pc) * 2;
    score += (b->off[SIDE_CPU] - b->off[SIDE_HUMAN]) * 30;
    score += b->bar[SIDE_HUMAN] * 80;
    score -= b->bar[SIDE_CPU] * 90;

    for (p = 1; p <= 24; p++) {
        n = b->point[p];
        if (n == 1) {
            if (p <= 6) {
                score += 20;
            } else if (p >= 19) {
                score += 12;
            } else {
                score += 8;
            }
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
        if (p >= 1) {
            if (p <= 6) {
                if (n <= -2) {
                    score += 28;
                }
                if (n >= 2) {
                    score -= 14;
                }
            }
        }
        if (p >= 19) {
            if (p <= 24) {
                if (n >= 2) {
                    score -= 28;
                }
                if (n <= -2) {
                    score += 14;
                }
            }
        }
    }

    pr_cpu = prime_len(b, SIDE_CPU);
    pr_hum = prime_len(b, SIDE_HUMAN);
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

// Opponent (or self) plays a 1-ply best complete play; returns evaluate()
// of the resulting position. Human minimizes CPU score; CPU maximizes it.
static int best_1ply_score(board_t *b, int side)
{
    int i, sc, best, n_ties;
    board_t child;

    generate_plays(b, side, &opp_pl);
    if (!has_legal_play(&opp_pl)) {
        return evaluate(b);
    }

    if (side == SIDE_CPU) {
        best = -INFIN;
    } else {
        best = INFIN;
    }
    n_ties = 0;

    for (i = 0; i < opp_pl.max; i++) {
        board_copy(&child, b);
        apply_play(&child, side, &opp_pl.play[i]);
        sc = evaluate(&child);
        if (side == SIDE_CPU) {
            if (sc > best) {
                best = sc;
                n_ties = 1;
            } else if (sc == best) {
                n_ties++;
            }
        } else {
            if (sc < best) {
                best = sc;
                n_ties = 1;
            } else if (sc == best) {
                n_ties++;
            }
        }
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
        sc = evaluate(&child);
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
    int i, j, n, nkeep, t, best_i, eq, d1, d2, w, best_eq, n_ties;
    board_t child, after_roll;

    generate_plays(b, SIDE_CPU, &cpu_pl);
    if (!has_legal_play(&cpu_pl)) {
        return -1;
    }

    if (difficulty == DIFF_EASY) {
        return pick_best_1ply_play(b, SIDE_CPU, &cpu_pl, out_play);
    }

    // Medium (and Hard placeholder): 1-ply rank, then 2-ply expectiminimax
    // on the top TOP_N_MEDIUM candidates.
    n = cpu_pl.max;
    for (i = 0; i < n; i++) {
        pord[i] = i;
        board_copy(&child, b);
        apply_play(&child, SIDE_CPU, &cpu_pl.play[i]);
        pscore[i] = evaluate(&child);
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

    nkeep = n;
    if (nkeep > TOP_N_MEDIUM) {
        nkeep = TOP_N_MEDIUM;
    }

    best_eq = -INFIN;
    n_ties = 0;
    copy_play(out_play, &cpu_pl.play[pord[0]]);

    for (i = 0; i < nkeep; i++) {
        board_copy(&child, b);
        apply_play(&child, SIDE_CPU, &cpu_pl.play[pord[i]]);
        if (game_winner(&child) == SIDE_CPU) {
            eq = WIN_SCORE * 36;
        } else {
            eq = 0;
            for (d1 = 1; d1 <= 6; d1++) {
                for (d2 = d1; d2 <= 6; d2++) {
                    if (d1 == d2) {
                        w = 1;
                    } else {
                        w = 2;
                    }
                    board_copy(&after_roll, &child);
                    after_roll.side_to_move = SIDE_HUMAN;
                    set_dice(&after_roll, d1, d2);
                    eq += w * best_1ply_score(&after_roll, SIDE_HUMAN);
                }
            }
        }
        if (eq > best_eq) {
            best_eq = eq;
            n_ties = 1;
            copy_play(out_play, &cpu_pl.play[pord[i]]);
        } else if (eq == best_eq) {
            n_ties++;
            if ((random() % n_ties) == 0) {
                copy_play(out_play, &cpu_pl.play[pord[i]]);
            }
        }
    }

    return 0;
}
