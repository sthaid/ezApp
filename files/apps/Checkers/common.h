#ifndef __CHECKERS_COMMON_H__
#define __CHECKERS_COMMON_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"

// pieces
#define EMPTY   0
#define R_MAN   1
#define R_KING  2
#define B_MAN   3
#define B_KING  4

// sides
#define SIDE_RED    0
#define SIDE_BLACK  1

#define MAX_PATH    13
#define MAX_MOVES   64

#define DIFF_EASY   0
#define DIFF_MEDIUM 1
#define DIFF_HARD   2

#define DEPTH_EASY   2
#define DEPTH_MEDIUM 3
#define DEPTH_HARD   4

typedef struct {
    int sq[8][8];
    int side_to_move;
} board_t;

// path[0] = start square; path[len-1] = final landing; len >= 2
typedef struct {
    int r[MAX_PATH];
    int c[MAX_PATH];
    int len;
} move_t;

typedef struct {
    move_t move[MAX_MOVES];
    int max;
} move_list_t;

char *progname;
char *data_dir;

// rules.c
void board_init(board_t *b, int first_side);
void board_copy(board_t *dst, board_t *src);
bool is_dark_square(int r, int c);
bool is_red_piece(int p);
bool is_black_piece(int p);
bool is_king(int p);
bool piece_belongs_to_side(int p, int side);
void generate_moves(board_t *b, int side, move_list_t *ml);
void apply_move(board_t *b, move_t *m);
int  count_pieces(board_t *b, int side);
bool side_has_move(board_t *b, int side);
int  game_winner(board_t *b);  // SIDE_RED, SIDE_BLACK, or -1 if ongoing

// ai.c
int  difficulty_to_depth(int difficulty);
// Returns 0 on success (fills out_move), or -1 if no legal moves.
int  cpu_choose_move(board_t *b, int depth, move_t *out_move);

#endif
