#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"

//
// defines
//

#define EMPTY 0
#define HUMAN 1
#define CPU   2

#define STATE_READY  0
#define STATE_ACTIVE 1
#define STATE_OVER   2

#define EVID_CELL_BASE  1
#define EVID_NEW_CPU    20
#define EVID_NEW_YOU    21

#define BOARD_X  80
#define BOARD_Y  420
#define BOARD_W  840
#define CELL_W   280

#define GRID_W   18
#define MARK_W   16
#define MARK_PAD 40

#define COLOR_GRID  COLOR_TEAL
#define COLOR_XMARK COLOR_ORANGE
#define COLOR_OMARK COLOR_LIGHT_BLUE
#define COLOR_WIN   COLOR_YELLOW
#define COLOR_PANEL COLOR_DARK_GRAY

//
// variables
//

char *progname;
char *data_dir;

int board[9];
int state;
int whose_turn;
int winner;
int win_a, win_b, win_c;

int win_line[8][3] = {
    {0, 1, 2},
    {3, 4, 5},
    {6, 7, 8},
    {0, 3, 6},
    {1, 4, 7},
    {2, 5, 8},
    {0, 4, 8},
    {2, 4, 6}
};

// Search center, then corners, then edges so alpha-beta prunes sooner.
int move_order[9] = {4, 0, 2, 6, 8, 1, 3, 5, 7};

//
// prototypes
//

void game_reset(int you_first);
void start_game(int you_first);
void play_cpu_if_needed(void);
int winner_of(void);
void capture_win_line(void);
bool board_full(void);
void finish_move(void);
void apply_human_move(int cell);
void apply_cpu_move(void);
int minimax(int player, int depth, int alpha, int beta);
int find_winning_cell(int player);
int cpu_choose_cell(void);
void update_display(void);
void draw_thick_line(int x1, int y1, int x2, int y2, int width, sdlx_color_t color);
void draw_x(int x, int y, int w, int h, sdlx_color_t color);
void draw_o(int x, int y, int w, int h, sdlx_color_t color);
int abs_int(int v);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    sdlx_event_t event;
    int cell;
    bool end_program;

    if (argc != 2) {
        printf("E %s: argc=%d is not 2\n", "TicTacToe", argc);
        return 1;
    }
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    srandom((unsigned int)util_microsec_timer());

    game_reset(1);
    end_program = false;

    while (!end_program) {
        sdlx_display_init(COLOR_BLACK, PORTRAIT);
        sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);
        update_display();
        sdlx_display_present();

        sdlx_get_event(-1, &event);

        switch (event.event_id) {
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_QUIT:
            end_program = true;
            break;
        case EVID_NEW_CPU:
            start_game(0);
            break;
        case EVID_NEW_YOU:
            start_game(1);
            break;
        default:
            if (state == STATE_ACTIVE) {
                if (whose_turn == HUMAN) {
                    if (event.event_id >= EVID_CELL_BASE) {
                        if (event.event_id < EVID_CELL_BASE + 9) {
                            cell = event.event_id - EVID_CELL_BASE;
                            apply_human_move(cell);
                            play_cpu_if_needed();
                        }
                    }
                }
            }
            break;
        }
    }

    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  GAME  ------------------------------------------

void game_reset(int you_first)
{
    int i;

    for (i = 0; i < 9; i++) {
        board[i] = EMPTY;
    }
    winner = EMPTY;
    win_a = -1;
    win_b = -1;
    win_c = -1;
    state = STATE_READY;
    if (you_first) {
        whose_turn = HUMAN;
    } else {
        whose_turn = CPU;
    }
}

void start_game(int you_first)
{
    game_reset(you_first);
    state = STATE_ACTIVE;
    printf("I %s: new game, you_first=%d\n", progname, you_first);
    play_cpu_if_needed();
}

void play_cpu_if_needed(void)
{
    if (state != STATE_ACTIVE) {
        return;
    }
    if (whose_turn != CPU) {
        return;
    }
    apply_cpu_move();
}

int winner_of(void)
{
    int i, a, b, c, p;

    for (i = 0; i < 8; i++) {
        a = win_line[i][0];
        b = win_line[i][1];
        c = win_line[i][2];
        p = board[a];
        if (p != EMPTY) {
            if (board[b] == p) {
                if (board[c] == p) {
                    return p;
                }
            }
        }
    }
    return EMPTY;
}

void capture_win_line(void)
{
    int i, a, b, c, p;

    win_a = -1;
    win_b = -1;
    win_c = -1;
    for (i = 0; i < 8; i++) {
        a = win_line[i][0];
        b = win_line[i][1];
        c = win_line[i][2];
        p = board[a];
        if (p != EMPTY) {
            if (board[b] == p) {
                if (board[c] == p) {
                    win_a = a;
                    win_b = b;
                    win_c = c;
                    return;
                }
            }
        }
    }
}

bool board_full(void)
{
    int i;

    for (i = 0; i < 9; i++) {
        if (board[i] == EMPTY) {
            return false;
        }
    }
    return true;
}

void finish_move(void)
{
    winner = winner_of();
    if (winner != EMPTY) {
        capture_win_line();
        state = STATE_OVER;
        if (winner == HUMAN) {
            printf("I %s: human wins\n", progname);
        } else {
            printf("I %s: cpu wins\n", progname);
        }
        return;
    }
    if (board_full()) {
        state = STATE_OVER;
        printf("I %s: draw\n", progname);
        return;
    }
    if (whose_turn == HUMAN) {
        whose_turn = CPU;
    } else {
        whose_turn = HUMAN;
    }
}

void apply_human_move(int cell)
{
    if (cell < 0) {
        return;
    }
    if (cell > 8) {
        return;
    }
    if (board[cell] != EMPTY) {
        return;
    }
    board[cell] = HUMAN;
    printf("I %s: human move cell=%d\n", progname, cell);
    finish_move();
}

void apply_cpu_move(void)
{
    int cell;

    cell = cpu_choose_cell();
    if (cell < 0) {
        return;
    }
    board[cell] = CPU;
    printf("I %s: cpu move cell=%d\n", progname, cell);
    finish_move();
}

int minimax(int player, int depth, int alpha, int beta)
{
    int i, cell, w, score, best;

    w = winner_of();
    if (w == CPU) {
        return 10 - depth;
    }
    if (w == HUMAN) {
        return depth - 10;
    }
    if (board_full()) {
        return 0;
    }

    if (player == CPU) {
        best = -999;
        for (i = 0; i < 9; i++) {
            cell = move_order[i];
            if (board[cell] == EMPTY) {
                board[cell] = CPU;
                score = minimax(HUMAN, depth + 1, alpha, beta);
                board[cell] = EMPTY;
                if (score > best) {
                    best = score;
                }
                if (score > alpha) {
                    alpha = score;
                }
                if (alpha >= beta) {
                    return best;
                }
            }
        }
        return best;
    }

    best = 999;
    for (i = 0; i < 9; i++) {
        cell = move_order[i];
        if (board[cell] == EMPTY) {
            board[cell] = HUMAN;
            score = minimax(CPU, depth + 1, alpha, beta);
            board[cell] = EMPTY;
            if (score < best) {
                best = score;
            }
            if (score < beta) {
                beta = score;
            }
            if (alpha >= beta) {
                return best;
            }
        }
    }
    return best;
}

int find_winning_cell(int player)
{
    int i, w;

    for (i = 0; i < 9; i++) {
        if (board[i] == EMPTY) {
            board[i] = player;
            w = winner_of();
            board[i] = EMPTY;
            if (w == player) {
                return i;
            }
        }
    }
    return -1;
}

int cpu_choose_cell(void)
{
    int i, cell, empty, score, best, nbest, pick;
    int best_moves[9];

    cell = find_winning_cell(CPU);
    if (cell >= 0) {
        return cell;
    }
    cell = find_winning_cell(HUMAN);
    if (cell >= 0) {
        return cell;
    }

    empty = 0;
    for (i = 0; i < 9; i++) {
        if (board[i] == EMPTY) {
            empty = empty + 1;
        }
    }
    if (empty == 9) {
        if ((random() % 2) == 0) {
            return 4;
        }
        pick = (int)(random() % 8);
        if (pick >= 4) {
            pick = pick + 1;
        }
        return pick;
    }

    best = -999;
    nbest = 0;
    for (i = 0; i < 9; i++) {
        cell = move_order[i];
        if (board[cell] == EMPTY) {
            board[cell] = CPU;
            score = minimax(HUMAN, 0, -999, 999);
            board[cell] = EMPTY;
            if (score > best) {
                best = score;
                nbest = 0;
                best_moves[nbest] = cell;
                nbest = nbest + 1;
            } else {
                if (score == best) {
                    best_moves[nbest] = cell;
                    nbest = nbest + 1;
                }
            }
        }
    }

    if (nbest <= 0) {
        return -1;
    }
    pick = (int)(random() % nbest);
    return best_moves[pick];
}

// -----------------  DISPLAY  ---------------------------------------

int abs_int(int v)
{
    if (v < 0) {
        return -v;
    }
    return v;
}

void draw_thick_line(int x1, int y1, int x2, int y2, int width, sdlx_color_t color)
{
    int n, i, x, y, half, dx, dy;

    half = width / 2;
    if (half < 1) {
        half = 1;
    }

    dx = abs_int(x2 - x1);
    dy = abs_int(y2 - y1);
    n = dx;
    if (dy > n) {
        n = dy;
    }
    if (n < 1) {
        n = 1;
    }

    for (i = 0; i <= n; i++) {
        x = x1 + (x2 - x1) * i / n;
        y = y1 + (y2 - y1) * i / n;
        sdlx_render_fill_circle(x, y, half, color);
    }
}

void draw_x(int x, int y, int w, int h, sdlx_color_t color)
{
    int x1, y1, x2, y2;

    x1 = x + MARK_PAD;
    y1 = y + MARK_PAD;
    x2 = x + w - MARK_PAD;
    y2 = y + h - MARK_PAD;
    draw_thick_line(x1, y1, x2, y2, MARK_W, color);
    draw_thick_line(x1, y2, x2, y1, MARK_W, color);
}

void draw_o(int x, int y, int w, int h, sdlx_color_t color)
{
    int cx, cy, radius;

    cx = x + w / 2;
    cy = y + h / 2;
    radius = w / 2 - MARK_PAD;
    sdlx_render_circle(cx, cy, radius, MARK_W, color);
}

void update_display(void)
{
    int r, c, i, x, y;
    int ax, ay, cx, cy;
    int gx;
    sdlx_loc_t loc;
    sdlx_loc_t *ploc;
    sdlx_color_t mark_color;
    char *status;

    sdlx_render_printf_ex(sdlx_win_width / 2, 80,
                          15, COLOR_LIGHT_GREEN, FLAG_XY_CTR,
                          "%s", "Tic Tac Toe");

    if (state == STATE_READY) {
        status = "Start a new game";
    } else if (state == STATE_OVER) {
        if (winner == HUMAN) {
            status = "You win";
        } else if (winner == CPU) {
            status = "CPU wins";
        } else {
            status = "Draw";
        }
    } else {
        status = "Your turn";
    }
    sdlx_render_printf_ex(sdlx_win_width / 2, 200,
                          FONT_NORMAL, COLOR_YELLOW, FLAG_XY_CTR,
                          "%s", status);

    sdlx_render_fill_rect(BOARD_X, BOARD_Y, BOARD_W, BOARD_W, COLOR_PANEL);

    gx = BOARD_X + CELL_W - GRID_W / 2;
    sdlx_render_fill_rect(gx, BOARD_Y, GRID_W, BOARD_W, COLOR_GRID);
    gx = BOARD_X + 2 * CELL_W - GRID_W / 2;
    sdlx_render_fill_rect(gx, BOARD_Y, GRID_W, BOARD_W, COLOR_GRID);
    gx = BOARD_Y + CELL_W - GRID_W / 2;
    sdlx_render_fill_rect(BOARD_X, gx, BOARD_W, GRID_W, COLOR_GRID);
    gx = BOARD_Y + 2 * CELL_W - GRID_W / 2;
    sdlx_render_fill_rect(BOARD_X, gx, BOARD_W, GRID_W, COLOR_GRID);

    for (i = 0; i < 9; i++) {
        r = i / 3;
        c = i - r * 3;
        x = BOARD_X + c * CELL_W;
        y = BOARD_Y + r * CELL_W;

        loc.x = x;
        loc.y = y;
        loc.w = CELL_W;
        loc.h = CELL_W;
        sdlx_register_event(&loc, EVID_CELL_BASE + i);

        if (board[i] == HUMAN) {
            mark_color = COLOR_XMARK;
            if (state == STATE_OVER) {
                if (i == win_a) {
                    mark_color = COLOR_WIN;
                } else if (i == win_b) {
                    mark_color = COLOR_WIN;
                } else if (i == win_c) {
                    mark_color = COLOR_WIN;
                }
            }
            draw_x(x, y, CELL_W, CELL_W, mark_color);
        } else if (board[i] == CPU) {
            mark_color = COLOR_OMARK;
            if (state == STATE_OVER) {
                if (i == win_a) {
                    mark_color = COLOR_WIN;
                } else if (i == win_b) {
                    mark_color = COLOR_WIN;
                } else if (i == win_c) {
                    mark_color = COLOR_WIN;
                }
            }
            draw_o(x, y, CELL_W, CELL_W, mark_color);
        }
    }

    if (state == STATE_OVER) {
        if (win_a >= 0) {
            ax = BOARD_X + (win_a - (win_a / 3) * 3) * CELL_W + CELL_W / 2;
            ay = BOARD_Y + (win_a / 3) * CELL_W + CELL_W / 2;
            cx = BOARD_X + (win_c - (win_c / 3) * 3) * CELL_W + CELL_W / 2;
            cy = BOARD_Y + (win_c / 3) * CELL_W + CELL_W / 2;
            draw_thick_line(ax, ay, cx, cy, MARK_W, COLOR_WIN);
        }
    }

    sdlx_render_printf_ex(sdlx_win_width / 2, 1340,
                          FONT_SMALL, COLOR_ORANGE, FLAG_XY_CTR,
                          "%s", "You are X");
    sdlx_render_printf_ex(sdlx_win_width / 2, 1420,
                          FONT_SMALL, COLOR_TEAL, FLAG_XY_CTR,
                          "%s", "CPU is O");

    ploc = sdlx_render_printf_ex(sdlx_win_width / 2, 1600,
                                 FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_XY_CTR,
                                 "%s", "New Game CPU First");
    sdlx_register_event(ploc, EVID_NEW_CPU);

    ploc = sdlx_render_printf_ex(sdlx_win_width / 2, 1760,
                                 FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_XY_CTR,
                                 "%s", "New Game You First");
    sdlx_register_event(ploc, EVID_NEW_YOU);

    reg_event_show_readme_file();

    sdlx_register_control_events(0, NULL,
                                 0, NULL,
                                 EVID_QUIT, "X");
}
