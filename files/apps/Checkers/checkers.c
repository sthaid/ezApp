#include <apps/Checkers/common.h>

//
// defines
//

#define Y_TOP           180
#define BOARD_SIZE      1000
#define SQ_SIZE         125

#define EVID_NEW_YOU    100
#define EVID_NEW_CPU    101
#define EVID_DIFF       102
#define EVID_SQ_BASE    1000

#define SEL_NONE        (-1)

//
// variables
//

static board_t board;
static int difficulty;
static int sel_r;
static int sel_c;
static bool game_started;
static char *diff_str[3] = { "Easy", "Medium", "Hard" };

//
// prototypes
//

static void new_game(int first_side);
static void draw_and_register(void);
static void handle_square_tap(int r, int c);
static void maybe_cpu_move(void);
static bool move_from_sel_to(int r, int c, move_t *out);
static bool square_is_legal_dest(int r, int c);

// -----------------  MAIN  ------------------------------------

int main(int argc, char **argv)
{
    sdlx_event_t event;
    bool end_program;
    int evid, r, c;

    if (argc != 2) {
        printf("E %s: argc=%d is not 2\n", "Checkers", argc);
        return 1;
    }
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    difficulty = (int)util_get_numeric_param(data_dir, "difficulty", DIFF_MEDIUM);
    if (difficulty < DIFF_EASY || difficulty > DIFF_HARD) {
        difficulty = DIFF_MEDIUM;
    }

    sel_r = SEL_NONE;
    sel_c = SEL_NONE;
    game_started = false;
    board_init(&board, SIDE_RED);
    end_program = false;

    while (!end_program) {
        draw_and_register();
        sdlx_get_event(-1, &event);
        evid = event.event_id;

        if (evid == EVID_QUIT) {
            end_program = true;
        } else if (evid == EVID_SHOW_README_FILE) {
            show_file(data_dir, "README");
        } else if (evid == EVID_NEW_YOU) {
            new_game(SIDE_RED);
        } else if (evid == EVID_NEW_CPU) {
            new_game(SIDE_BLACK);
            draw_and_register();
            maybe_cpu_move();
        } else if (evid == EVID_DIFF) {
            difficulty++;
            if (difficulty > DIFF_HARD) {
                difficulty = DIFF_EASY;
            }
            util_set_numeric_param(data_dir, "difficulty", difficulty);
        } else if (evid >= EVID_SQ_BASE && evid < EVID_SQ_BASE + 64) {
            evid -= EVID_SQ_BASE;
            r = evid / 8;
            c = evid % 8;
            if (game_started) {
                handle_square_tap(r, c);
            }
        }
    }

    return 0;
}

// -----------------  GAME CONTROL  ----------------------------

static void new_game(int first_side)
{
    board_init(&board, first_side);
    sel_r = SEL_NONE;
    sel_c = SEL_NONE;
    game_started = true;
}

static void maybe_cpu_move(void)
{
    move_t mv;
    int depth, winner;

    if (!game_started) {
        return;
    }
    winner = game_winner(&board);
    if (winner >= 0) {
        return;
    }
    if (board.side_to_move != SIDE_BLACK) {
        return;
    }

    depth = difficulty_to_depth(difficulty);
    if (cpu_choose_move(&board, depth, &mv) < 0) {
        return;
    }
    apply_move(&board, &mv);
    sel_r = SEL_NONE;
    sel_c = SEL_NONE;
}

static bool move_from_sel_to(int r, int c, move_t *out)
{
    move_list_t ml;
    int i, j;

    generate_moves(&board, SIDE_RED, &ml);
    for (i = 0; i < ml.max; i++) {
        if (ml.move[i].r[0] != sel_r || ml.move[i].c[0] != sel_c) {
            continue;
        }
        if (ml.move[i].r[ml.move[i].len - 1] != r ||
            ml.move[i].c[ml.move[i].len - 1] != c) {
            continue;
        }
        out->len = ml.move[i].len;
        for (j = 0; j < ml.move[i].len; j++) {
            out->r[j] = ml.move[i].r[j];
            out->c[j] = ml.move[i].c[j];
        }
        return true;
    }
    return false;
}

static bool square_is_legal_dest(int r, int c)
{
    move_t tmp;

    if (sel_r == SEL_NONE) {
        return false;
    }
    return move_from_sel_to(r, c, &tmp);
}

static void handle_square_tap(int r, int c)
{
    move_t mv;
    int winner, piece;

    if (board.side_to_move != SIDE_RED) {
        return;
    }
    winner = game_winner(&board);
    if (winner >= 0) {
        return;
    }

    piece = board.sq[r][c];

    // If a piece is selected and this is a legal destination, move
    if (sel_r != SEL_NONE && move_from_sel_to(r, c, &mv)) {
        apply_move(&board, &mv);
        sel_r = SEL_NONE;
        sel_c = SEL_NONE;
        // Show the human move before the CPU thinks
        draw_and_register();
        maybe_cpu_move();
        return;
    }

    // Select own piece (must have at least one legal move from it)
    if (piece_belongs_to_side(piece, SIDE_RED)) {
        move_list_t ml;
        int i;
        bool any;

        generate_moves(&board, SIDE_RED, &ml);
        any = false;
        for (i = 0; i < ml.max; i++) {
            if (ml.move[i].r[0] == r && ml.move[i].c[0] == c) {
                any = true;
                break;
            }
        }
        if (any) {
            sel_r = r;
            sel_c = c;
        } else {
            sel_r = SEL_NONE;
            sel_c = SEL_NONE;
        }
        return;
    }

    // Tap elsewhere clears selection
    sel_r = SEL_NONE;
    sel_c = SEL_NONE;
}

// -----------------  DISPLAY  ---------------------------------

static void draw_and_register(void)
{
    int r, c, x, y, piece, winner;
    int cx, cy, radius;
    sdlx_loc_t loc;
    sdlx_loc_t *ploc;
    sdlx_color_t light, dark, hi;
    char status[80];

    light = sdlx_create_color(222, 184, 135, 255);  // tan
    dark  = sdlx_create_color(139, 90, 43, 255);    // brown
    hi    = COLOR_YELLOW;

    sdlx_display_init(COLOR_BLACK, PORTRAIT);

    // title / status
    winner = -1;
    if (game_started) {
        winner = game_winner(&board);
    }
    if (!game_started) {
        sprintf(status, "%s", "American Checkers");
    } else if (winner == SIDE_RED) {
        sprintf(status, "%s", "You win!");
    } else if (winner == SIDE_BLACK) {
        sprintf(status, "%s", "CPU wins!");
    } else if (board.side_to_move == SIDE_RED) {
        sprintf(status, "%s", "Your turn");
    } else {
        sprintf(status, "%s", "CPU turn");
    }
    sdlx_render_printf_ex(sdlx_win_width / 2, 140,
                          FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, "%s", status);

    // board squares and pieces
    for (r = 0; r < 8; r++) {
        for (c = 0; c < 8; c++) {
            x = c * SQ_SIZE;
            y = Y_TOP + r * SQ_SIZE;
            if (is_dark_square(r, c)) {
                sdlx_render_fill_rect(x, y, SQ_SIZE, SQ_SIZE, dark);
            } else {
                sdlx_render_fill_rect(x, y, SQ_SIZE, SQ_SIZE, light);
            }

            if (sel_r == r && sel_c == c) {
                sdlx_render_rect(x + 2, y + 2, SQ_SIZE - 4, SQ_SIZE - 4, 4, hi);
            } else if (square_is_legal_dest(r, c)) {
                sdlx_render_rect(x + 4, y + 4, SQ_SIZE - 8, SQ_SIZE - 8, 3, COLOR_LIGHT_GREEN);
            }

            piece = board.sq[r][c];
            if (piece != EMPTY) {
                cx = x + SQ_SIZE / 2;
                cy = y + SQ_SIZE / 2;
                radius = 45;
                if (is_red_piece(piece)) {
                    sdlx_render_fill_circle(cx, cy, radius, COLOR_RED);
                } else {
                    sdlx_render_fill_circle(cx, cy, radius, COLOR_BLACK);
                    sdlx_render_circle(cx, cy, radius, 2, COLOR_LIGHT_GRAY);
                }
                if (is_king(piece)) {
                    sdlx_render_fill_circle(cx, cy, 18, COLOR_YELLOW);
                }
            }

            if (is_dark_square(r, c)) {
                loc.x = x;
                loc.y = y;
                loc.w = SQ_SIZE;
                loc.h = SQ_SIZE;
                sdlx_register_event(&loc, EVID_SQ_BASE + r * 8 + c);
            }
        }
    }

    // on-screen controls below board
    ploc = sdlx_render_printf_ex(20, Y_TOP + BOARD_SIZE + 80,
                                 FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                 "%s", "You First");
    sdlx_register_event(ploc, EVID_NEW_YOU);

    ploc = sdlx_render_printf_ex(20, Y_TOP + BOARD_SIZE + 200,
                                 FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                 "%s", "CPU First");
    sdlx_register_event(ploc, EVID_NEW_CPU);

    ploc = sdlx_render_printf_ex(20, Y_TOP + BOARD_SIZE + 320,
                                 FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                 "Diff: %s", diff_str[difficulty]);
    sdlx_register_event(ploc, EVID_DIFF);

    reg_event_show_readme_file();
    sdlx_register_control_events(0, NULL,
                                 0, NULL,
                                 EVID_QUIT, "X");

    sdlx_display_present();
}
