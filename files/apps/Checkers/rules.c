#include <apps/Checkers/common.h>

// -----------------  HELPERS  ---------------------------------

bool is_dark_square(int r, int c)
{
    return ((r + c) & 1) == 1;
}

bool is_red_piece(int p)
{
    return p == R_MAN || p == R_KING;
}

bool is_black_piece(int p)
{
    return p == B_MAN || p == B_KING;
}

bool is_king(int p)
{
    return p == R_KING || p == B_KING;
}

bool piece_belongs_to_side(int p, int side)
{
    if (side == SIDE_RED) {
        return is_red_piece(p);
    }
    return is_black_piece(p);
}

static bool on_board(int r, int c)
{
    return r >= 0 && r < 8 && c >= 0 && c < 8;
}

static bool is_enemy(int p, int side)
{
    if (side == SIDE_RED) {
        return is_black_piece(p);
    }
    return is_red_piece(p);
}

static int promote_if_needed(int piece, int r)
{
    if (piece == R_MAN && r == 0) {
        return R_KING;
    }
    if (piece == B_MAN && r == 7) {
        return B_KING;
    }
    return piece;
}

static void add_move(move_list_t *ml, move_t *m)
{
    int i;

    if (ml->max >= MAX_MOVES) {
        return;
    }
    for (i = 0; i < m->len; i++) {
        ml->move[ml->max].r[i] = m->r[i];
        ml->move[ml->max].c[i] = m->c[i];
    }
    ml->move[ml->max].len = m->len;
    ml->max++;
}

// -----------------  BOARD SETUP  -----------------------------

void board_init(board_t *b, int first_side)
{
    int r, c;

    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            b->sq[r][c] = EMPTY;
        }
    }

    // Black on top rows 0..2, Red on bottom rows 5..7 (dark squares)
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 8; c++) {
            if (is_dark_square(r, c)) {
                b->sq[r][c] = B_MAN;
            }
        }
    }
    for (r = 5; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            if (is_dark_square(r, c)) {
                b->sq[r][c] = R_MAN;
            }
        }
    }

    b->side_to_move = first_side;
}

void board_copy(board_t *dst, board_t *src)
{
    int r, c;

    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            dst->sq[r][c] = src->sq[r][c];
        }
    }
    dst->side_to_move = src->side_to_move;
}

// -----------------  MOVE GENERATION  -------------------------

static void gen_quiet_from(board_t *b, int r, int c, int side, move_list_t *ml)
{
    int piece, dr[4], dc[4], n_dir, i, nr, nc;
    move_t m;

    piece = b->sq[r][c];
    n_dir = 0;

    if (piece == B_MAN || piece == B_KING || piece == R_KING) {
        dr[n_dir] = 1;
        dc[n_dir] = -1;
        n_dir++;
        dr[n_dir] = 1;
        dc[n_dir] = 1;
        n_dir++;
    }
    if (piece == R_MAN || piece == R_KING || piece == B_KING) {
        dr[n_dir] = -1;
        dc[n_dir] = -1;
        n_dir++;
        dr[n_dir] = -1;
        dc[n_dir] = 1;
        n_dir++;
    }

    for (i = 0; i < n_dir; i++) {
        nr = r + dr[i];
        nc = c + dc[i];
        if (!on_board(nr, nc)) {
            continue;
        }
        if (b->sq[nr][nc] != EMPTY) {
            continue;
        }
        m.r[0] = r;
        m.c[0] = c;
        m.r[1] = nr;
        m.c[1] = nc;
        m.len = 2;
        add_move(ml, &m);
    }
}

// Recursive capture search. path already contains start..current.
// crowned: man became king during this sequence (ends further jumps).
static void gen_jumps_from(board_t *b, int r, int c, int piece, int side,
                           move_t *path, bool crowned, move_list_t *ml)
{
    int dr[4], dc[4], n_dir, i, mr, mc, lr, lc;
    int saved_from, saved_mid;

    if (crowned) {
        return;
    }

    n_dir = 0;
    if (piece == B_MAN || piece == B_KING || piece == R_KING) {
        dr[n_dir] = 1;
        dc[n_dir] = -1;
        n_dir++;
        dr[n_dir] = 1;
        dc[n_dir] = 1;
        n_dir++;
    }
    if (piece == R_MAN || piece == R_KING || piece == B_KING) {
        dr[n_dir] = -1;
        dc[n_dir] = -1;
        n_dir++;
        dr[n_dir] = -1;
        dc[n_dir] = 1;
        n_dir++;
    }

    for (i = 0; i < n_dir; i++) {
        mr = r + dr[i];
        mc = c + dc[i];
        lr = r + 2 * dr[i];
        lc = c + 2 * dc[i];
        if (!on_board(lr, lc)) {
            continue;
        }
        if (b->sq[lr][lc] != EMPTY) {
            continue;
        }
        if (!is_enemy(b->sq[mr][mc], side)) {
            continue;
        }

        // make hop on a temporary board state (mutate/restore)
        saved_from = b->sq[r][c];
        saved_mid = b->sq[mr][mc];
        b->sq[r][c] = EMPTY;
        b->sq[mr][mc] = EMPTY;
        b->sq[lr][lc] = piece;

        path->r[path->len] = lr;
        path->c[path->len] = lc;
        path->len++;

        {
            int landed = promote_if_needed(piece, lr);
            bool did_crown = (landed != piece);

            if (did_crown) {
                b->sq[lr][lc] = landed;
                add_move(ml, path);
            } else {
                int prev_max = ml->max;
                gen_jumps_from(b, lr, lc, piece, side, path, false, ml);
                if (ml->max == prev_max) {
                    // no further jumps; this hop sequence is complete
                    add_move(ml, path);
                }
            }
        }

        path->len--;
        b->sq[lr][lc] = EMPTY;
        b->sq[mr][mc] = saved_mid;
        b->sq[r][c] = saved_from;
    }
}

void generate_moves(board_t *b, int side, move_list_t *ml)
{
    int r, c, piece;
    move_list_t captures;
    move_t path;

    ml->max = 0;
    captures.max = 0;

    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            piece = b->sq[r][c];
            if (!piece_belongs_to_side(piece, side)) {
                continue;
            }
            path.r[0] = r;
            path.c[0] = c;
            path.len = 1;
            gen_jumps_from(b, r, c, piece, side, &path, false, &captures);
        }
    }

    if (captures.max > 0) {
        int i, j;
        for (i = 0; i < captures.max; i++) {
            for (j = 0; j < captures.move[i].len; j++) {
                ml->move[ml->max].r[j] = captures.move[i].r[j];
                ml->move[ml->max].c[j] = captures.move[i].c[j];
            }
            ml->move[ml->max].len = captures.move[i].len;
            ml->max++;
        }
        return;
    }

    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            piece = b->sq[r][c];
            if (!piece_belongs_to_side(piece, side)) {
                continue;
            }
            gen_quiet_from(b, r, c, side, ml);
        }
    }
}

// -----------------  APPLY / STATUS  --------------------------

void apply_move(board_t *b, move_t *m)
{
    int i, r0, c0, r1, c1, mr, mc, piece;

    if (m->len < 2) {
        return;
    }

    r0 = m->r[0];
    c0 = m->c[0];
    piece = b->sq[r0][c0];
    b->sq[r0][c0] = EMPTY;

    for (i = 1; i < m->len; i++) {
        r1 = m->r[i];
        c1 = m->c[i];
        // if jump (distance 2), remove captured piece
        if ((r1 - r0 == 2) || (r0 - r1 == 2)) {
            mr = (r0 + r1) / 2;
            mc = (c0 + c1) / 2;
            b->sq[mr][mc] = EMPTY;
        }
        r0 = r1;
        c0 = c1;
    }

    piece = promote_if_needed(piece, m->r[m->len - 1]);
    b->sq[m->r[m->len - 1]][m->c[m->len - 1]] = piece;

    if (b->side_to_move == SIDE_RED) {
        b->side_to_move = SIDE_BLACK;
    } else {
        b->side_to_move = SIDE_RED;
    }
}

int count_pieces(board_t *b, int side)
{
    int r, c, n;

    n = 0;
    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            if (piece_belongs_to_side(b->sq[r][c], side)) {
                n++;
            }
        }
    }
    return n;
}

bool side_has_move(board_t *b, int side)
{
    move_list_t ml;

    generate_moves(b, side, &ml);
    return ml.max > 0;
}

int game_winner(board_t *b)
{
    int side, other;

    side = b->side_to_move;
    if (side == SIDE_RED) {
        other = SIDE_BLACK;
    } else {
        other = SIDE_RED;
    }

    if (count_pieces(b, side) == 0) {
        return other;
    }
    if (!side_has_move(b, side)) {
        return other;
    }
    return -1;
}
