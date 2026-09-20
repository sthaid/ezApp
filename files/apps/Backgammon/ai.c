#include <apps/Backgammon/common.h>

#define INFIN        100000000
#define WIN_SCORE    1000000
#define TOP_N_MEDIUM 8

// Hard later: 2-ply over all plays (no top-N prune) and/or truncated
// Monte Carlo rollouts. DIFF_HARD is reserved in common.h but is not
// offered in the UI. If selected, Medium behavior is used.

static play_list_t cpu_pl;
static int pscore[MAX_PLAYS];
static int pord[MAX_PLAYS];
static board_t ranked_child[TOP_N_MEDIUM];
static int kept_idx[TOP_N_MEDIUM];

static int pick_best_1ply_play(board_t *b, int side, play_list_t *pl, play_t *out);

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
        if (nkeep >= TOP_N_MEDIUM) {
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
                    } else {
                        w = 2;
                    }
                    board_copy(&after_roll, &ranked_child[i]);
                    after_roll.side_to_move = SIDE_HUMAN;
                    set_dice(&after_roll, d1, d2);
                    eq += w * score_best_play(&after_roll, SIDE_HUMAN);
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
