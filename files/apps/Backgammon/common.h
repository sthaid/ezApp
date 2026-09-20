#ifndef __BACKGAMMON_COMMON_H__
#define __BACKGAMMON_COMMON_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"

// sides: human is White (moves 24 -> 1), CPU is Black (moves 1 -> 24)
#define SIDE_HUMAN  0
#define SIDE_CPU    1

#define N_CHECKERS  15

#define DIFF_EASY    0
#define DIFF_MEDIUM  1
#define DIFF_HARD    2   // reserved; not offered in the UI yet

#define DEPTH_EASY    1
#define DEPTH_MEDIUM  2
#define DEPTH_HARD    2  // later: 2-ply over all plays and/or rollouts

#define MAX_STEPS  4
#define MAX_PLAYS  512
#define MAX_STEPS_ONE_DIE  16

#define FROM_BAR  0
#define TO_OFF    0

typedef struct {
    int from;  // 1-24, or FROM_BAR
    int to;    // 1-24, or TO_OFF
    int die;
    int hit;   // 1 if this step sent a blot to the bar
} step_t;

typedef struct {
    step_t step[MAX_STEPS];
    int nsteps;
} play_t;

typedef struct {
    play_t play[MAX_PLAYS];
    int max;
} play_list_t;

typedef struct {
    step_t step[MAX_STEPS_ONE_DIE];
    int max;
} step_list_t;

// point[1..24]: +count human, -count CPU; point[0] unused
typedef struct {
    int point[25];
    int bar[2];
    int off[2];
    int side_to_move;
    int dice[2];
    int nremain;
    int remain[MAX_STEPS];
} board_t;

char *progname;
char *data_dir;

// rules.c
void board_init(board_t *b);
void board_copy(board_t *dst, board_t *src);
void copy_step(step_t *dst, step_t *src);
void copy_play(play_t *dst, play_t *src);
int  other_side(int side);
int  roll_die(void);
void set_dice(board_t *b, int d1, int d2);
void opening_roll(board_t *b);
void roll_turn_dice(board_t *b);
void consume_die(board_t *b, int die);
bool is_blocked(board_t *b, int side, int pt);
bool all_in_home(board_t *b, int side);
void generate_steps(board_t *b, int side, int die, step_list_t *sl);
void generate_plays(board_t *b, int side, play_list_t *pl);
void apply_step(board_t *b, int side, step_t *s);
void apply_play(board_t *b, int side, play_t *p);
int  game_winner(board_t *b);  // SIDE_HUMAN, SIDE_CPU, or -1
int  pip_count(board_t *b, int side);
bool has_contact(board_t *b);
bool has_legal_play(play_list_t *pl);
int  score_best_play(board_t *b, int side);

// ai.c
int  evaluate_board(board_t *b);  // CPU-positive heuristic
int  difficulty_to_depth(int difficulty);
// Returns 0 on success (fills out_play), or -1 if no legal plays.
int  cpu_choose_play(board_t *b, int difficulty, play_t *out_play);

#endif
