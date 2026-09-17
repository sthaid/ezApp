#include <apps/Checkers/common.h>

#define INFIN  100000000
#define VAL_MAN  100
#define VAL_KING 175

static int imin(int a, int b)
{
    if (a < b) {
        return a;
    }
    return b;
}

static int imax(int a, int b)
{
    if (a > b) {
        return a;
    }
    return b;
}

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

// Score from Black (CPU) perspective: positive favors Black.
static int evaluate(board_t *b)
{
    int r, c, p, score, mobility_b, mobility_r;
    move_list_t ml;

    score = 0;
    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            p = b->sq[r][c];
            if (p == B_MAN) {
                score += VAL_MAN;
                score += r;           // advance toward promotion
            } else if (p == B_KING) {
                score += VAL_KING;
                if (r >= 2 && r <= 5 && c >= 2 && c <= 5) {
                    score += 5;
                }
            } else if (p == R_MAN) {
                score -= VAL_MAN;
                score -= (7 - r);
            } else if (p == R_KING) {
                score -= VAL_KING;
                if (r >= 2 && r <= 5 && c >= 2 && c <= 5) {
                    score -= 5;
                }
            }
        }
    }

    generate_moves(b, SIDE_BLACK, &ml);
    mobility_b = ml.max;
    generate_moves(b, SIDE_RED, &ml);
    mobility_r = ml.max;
    score += 2 * (mobility_b - mobility_r);

    return score;
}

static int move_order_key(move_t *m)
{
    // Prefer longer captures, then any jump, then quieter
    if (m->len > 2) {
        return 1000 + m->len;
    }
    if ((m->r[1] - m->r[0] == 2) || (m->r[0] - m->r[1] == 2)) {
        return 500;
    }
    return 0;
}

static void copy_move(move_t *dst, move_t *src)
{
    int i;

    dst->len = src->len;
    for (i = 0; i < src->len; i++) {
        dst->r[i] = src->r[i];
        dst->c[i] = src->c[i];
    }
}

static void sort_moves(move_list_t *ml)
{
    int i, j, best, key_i, key_j;
    move_t tmp;

    for (i = 0; i < ml->max; i++) {
        best = i;
        key_i = move_order_key(&ml->move[i]);
        for (j = i + 1; j < ml->max; j++) {
            key_j = move_order_key(&ml->move[j]);
            if (key_j > key_i) {
                best = j;
                key_i = key_j;
            }
        }
        if (best != i) {
            copy_move(&tmp, &ml->move[i]);
            copy_move(&ml->move[i], &ml->move[best]);
            copy_move(&ml->move[best], &tmp);
        }
    }
}

static int alphabeta(board_t *b, int depth, int alpha, int beta, bool maximizing)
{
    int i, best, val, winner;
    move_list_t ml;
    board_t child;

    winner = game_winner(b);
    if (winner == SIDE_BLACK) {
        return INFIN - (10 - depth);
    }
    if (winner == SIDE_RED) {
        return -INFIN + (10 - depth);
    }
    if (depth == 0) {
        return evaluate(b);
    }

    generate_moves(b, b->side_to_move, &ml);
    if (ml.max == 0) {
        return evaluate(b);
    }
    sort_moves(&ml);

    if (maximizing) {
        best = -INFIN;
        for (i = 0; i < ml.max; i++) {
            board_copy(&child, b);
            apply_move(&child, &ml.move[i]);
            val = alphabeta(&child, depth - 1, alpha, beta, false);
            best = imax(best, val);
            alpha = imax(alpha, best);
            if (beta <= alpha) {
                break;
            }
        }
        return best;
    }

    best = INFIN;
    for (i = 0; i < ml.max; i++) {
        board_copy(&child, b);
        apply_move(&child, &ml.move[i]);
        val = alphabeta(&child, depth - 1, alpha, beta, true);
        best = imin(best, val);
        beta = imin(beta, best);
        if (beta <= alpha) {
            break;
        }
    }
    return best;
}

int cpu_choose_move(board_t *b, int depth, move_t *out_move)
{
    int i, best_val, val;
    move_list_t ml;
    board_t child;
    bool maximizing;

    generate_moves(b, b->side_to_move, &ml);
    if (ml.max == 0) {
        return -1;
    }
    sort_moves(&ml);

    maximizing = (b->side_to_move == SIDE_BLACK);
    best_val = maximizing ? -INFIN : INFIN;

    copy_move(out_move, &ml.move[0]);

    for (i = 0; i < ml.max; i++) {
        board_copy(&child, b);
        apply_move(&child, &ml.move[i]);
        val = alphabeta(&child, depth - 1, -INFIN, INFIN, !maximizing);
        if (maximizing) {
            if (val > best_val) {
                best_val = val;
                copy_move(out_move, &ml.move[i]);
            }
        } else {
            if (val < best_val) {
                best_val = val;
                copy_move(out_move, &ml.move[i]);
            }
        }
    }

    // Return 0 on success (do not return eval score — it can be negative).
    return 0;
}
