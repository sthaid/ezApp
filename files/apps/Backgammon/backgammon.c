#include <apps/Backgammon/common.h>

//#define TEST_HARNESS

#ifdef TEST_HARNESS
#include <profile.h>
#endif


#define BOARD_X     50
#define BOARD_Y     0
#define POINT_W     120
#define POINT_H     460
#define BAR_W       80
#define MID_GAP     80
#define TRAY_X      1575
#define TRAY_W      120
#define DIE_SIZE    100
#define RAIL_GAP    80
#define CHK_RADIUS  26
#define CHK_STEP    48
#define MAX_VIS     5
#define RECT_LW     7
#define CPU_STEP_US 450000

#define EVID_NEW_GAME  100
#define EVID_DIFF      101
#define EVID_BAR       200
#define EVID_OFF       201
#define EVID_PT_BASE   1000

#define SEL_NONE  (-1)

static board_t board;
static play_list_t ui_pl;
static int difficulty;
static int sel;
static int dest_hi;
static bool game_started;
static bool end_program;
static bool cpu_thinking;
static char *diff_str[2] = { "Easy", "Medium" };

static void new_game(void);
static void draw_and_register(void);
static void play_or_skip(void);
static void do_cpu_turn(void);
static void handle_point_tap(int pt);
static void handle_bar_tap(void);
static void handle_off_tap(void);
static void try_play_step(int from, int to);
static bool dest_is_legal(int from, int to);
static bool from_is_legal(int from);
static bool find_step(int from, int to, step_t *out);
static int  point_x(int pt);
static int  point_y(int pt);
static bool point_is_top(int pt);
static int  bar_x(void);
static void draw_die(int x, int y, int size, int val);
static void draw_stack(int cx, int y0, int dir, int n, sdlx_color_t col);
static void die_pip(int cx, int cy, int ox, int oy, int col, int row, int pr);

// -----------------  MAIN  ------------------------------------

int main(int argc, char **argv)
{
    sdlx_event_t event;
    int evid, pt;

#ifdef TEST_HARNESS
    bool first_call = true;
#endif

    if (argc != 2) {
        printf("E %s: argc=%d is not 2\n", "Backgammon", argc);
        return 1;
    }
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

#ifndef TEST_HARNESS
    srandom(time(NULL));
#endif

    difficulty = (int)util_get_numeric_param(data_dir, "difficulty", DIFF_MEDIUM);
    if (difficulty < DIFF_EASY || difficulty > DIFF_MEDIUM) {
        difficulty = DIFF_MEDIUM;
    }

#ifdef TEST_HARNESS
    difficulty = DIFF_MEDIUM;
#endif

    sel = SEL_NONE;
    dest_hi = SEL_NONE;
    game_started = false;
    end_program = false;
    cpu_thinking = false;
    board_init(&board);
    ui_pl.max = 0;

    while (!end_program) {
        draw_and_register();
#ifdef TEST_HARNESS
        if (first_call) {
            event.event_id = EVID_NEW_GAME;
            first_call = false;
        } else {
            sdlx_get_event(-1, &event);
        }
#else
        sdlx_get_event(-1, &event);
#endif
        evid = event.event_id;

        if (evid == EVID_QUIT) {
            end_program = true;
        } else if (evid == EVID_SHOW_README_FILE) {
            show_file(data_dir, "README");
        } else if (evid == EVID_NEW_GAME) {
            new_game();
        } else if (evid == EVID_DIFF) {
            difficulty++;
            if (difficulty > DIFF_MEDIUM) {
                difficulty = DIFF_EASY;
            }
            util_set_numeric_param(data_dir, "difficulty", difficulty);
        } else if (evid == EVID_BAR) {
            if (game_started) {
                handle_bar_tap();
            }
        } else if (evid == EVID_OFF) {
            if (game_started) {
                handle_off_tap();
            }
        } else if (evid >= EVID_PT_BASE + 1 && evid <= EVID_PT_BASE + 24) {
            pt = evid - EVID_PT_BASE;
            if (game_started) {
                handle_point_tap(pt);
            }
        }
    }

    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  GAME CONTROL  ----------------------------

static void new_game(void)
{
    board_init(&board);
    opening_roll(&board);
    sel = SEL_NONE;
    dest_hi = SEL_NONE;
    game_started = true;
    printf("I %s: new game, dice=%d,%d first=%s\n",
           progname, board.dice[0], board.dice[1],
           board.side_to_move == SIDE_HUMAN ? "human" : "cpu");
    play_or_skip();
}

static void play_or_skip(void)
{
    int winner, skips;

    skips = 0;
    while (game_started && !end_program) {
        winner = game_winner(&board);
        if (winner >= 0) {
            return;
        }
        generate_next_steps(&board, board.side_to_move, &ui_pl);
        if (!has_legal_play(&ui_pl)) {
            if (board.side_to_move == SIDE_HUMAN) {
                sdlx_show_toast("No legal play");
            } else {
                sdlx_show_toast("CPU has no play");
            }
            board.side_to_move = other_side(board.side_to_move);
            roll_turn_dice(&board);
            skips++;
            if (skips > 8) {
                return;
            }
            continue;
        }
        if (board.side_to_move == SIDE_HUMAN) {
            if (board.bar[SIDE_HUMAN] > 0) {
                sel = FROM_BAR;
            }
            return;
        }
        do_cpu_turn();
        winner = game_winner(&board);
        if (winner >= 0) {
            return;
        }
        if (end_program) {
            return;
        }
        board.side_to_move = SIDE_HUMAN;
        roll_turn_dice(&board);
        skips = 0;
    }
}

static void do_cpu_turn(void)
{
    play_t play;
    int i;
    sdlx_event_t event;

    cpu_thinking = true;
    draw_and_register();

#ifdef TEST_HARNESS
    if (profile_start() != 0) {
        printf("E %s: profile_start failed\n", progname);
    }

    long start = util_microsec_timer();
    cpu_choose_play(&board, difficulty, &play);
    long duration = util_microsec_timer() - start;

    profile_stop(100);

    printf("I %s: test harness end program, cpu move duration = %0.3f seconds\n",
           progname, duration/1000000.0);
    exit(1);
#else
    long start = util_microsec_timer();
    int rc = cpu_choose_play(&board, difficulty, &play);
    printf("I %s: cpu move duration = %0.3f\n", 
           progname, (util_microsec_timer() - start) / 1000000.0);
    if (rc < 0) {
        cpu_thinking = false;
        return;
    }
    cpu_thinking = false;
#endif

    for (i = 0; i < play.nsteps; i++) {
        if (end_program) {
            return;
        }
        sel = play.step[i].from;
        dest_hi = play.step[i].to;
        if (dest_hi == TO_OFF) {
            dest_hi = 25;
        }
        draw_and_register();
        sdlx_get_event(CPU_STEP_US, &event);
        if (event.event_id == EVID_QUIT) {
            end_program = true;
            return;
        }
        apply_step(&board, SIDE_CPU, &play.step[i]);
        consume_die(&board, play.step[i].die);
    }
    sel = SEL_NONE;
    dest_hi = SEL_NONE;
}

static void try_play_step(int from, int to)
{
    step_t st;
    int winner;

    if (!find_step(from, to, &st)) {
        return;
    }
    apply_step(&board, SIDE_HUMAN, &st);
    consume_die(&board, st.die);
    sel = SEL_NONE;
    dest_hi = SEL_NONE;

    winner = game_winner(&board);
    if (winner >= 0) {
        ui_pl.max = 0;
        return;
    }

    if (board.nremain <= 0) {
        board.side_to_move = SIDE_CPU;
        roll_turn_dice(&board);
        play_or_skip();
        return;
    }

    generate_next_steps(&board, SIDE_HUMAN, &ui_pl);
    if (!has_legal_play(&ui_pl)) {
        board.side_to_move = SIDE_CPU;
        roll_turn_dice(&board);
        play_or_skip();
        return;
    }
    if (board.bar[SIDE_HUMAN] > 0) {
        sel = FROM_BAR;
    }
}

static bool dest_is_legal(int from, int to)
{
    int i;

    for (i = 0; i < ui_pl.max; i++) {
        if (ui_pl.play[i].nsteps <= 0) {
            continue;
        }
        if (ui_pl.play[i].step[0].from == from) {
            if (ui_pl.play[i].step[0].to == to) {
                return true;
            }
        }
    }
    return false;
}

static bool from_is_legal(int from)
{
    int i;

    for (i = 0; i < ui_pl.max; i++) {
        if (ui_pl.play[i].nsteps <= 0) {
            continue;
        }
        if (ui_pl.play[i].step[0].from == from) {
            return true;
        }
    }
    return false;
}

static bool find_step(int from, int to, step_t *out)
{
    int i;

    for (i = 0; i < ui_pl.max; i++) {
        if (ui_pl.play[i].nsteps <= 0) {
            continue;
        }
        if (ui_pl.play[i].step[0].from == from) {
            if (ui_pl.play[i].step[0].to == to) {
                copy_step(out, &ui_pl.play[i].step[0]);
                return true;
            }
        }
    }
    return false;
}

static void handle_bar_tap(void)
{
    if (board.side_to_move != SIDE_HUMAN) {
        return;
    }
    if (game_winner(&board) >= 0) {
        return;
    }
    if (board.bar[SIDE_HUMAN] > 0) {
        if (from_is_legal(FROM_BAR)) {
            sel = FROM_BAR;
        }
    }
}

static void handle_off_tap(void)
{
    if (board.side_to_move != SIDE_HUMAN) {
        return;
    }
    if (game_winner(&board) >= 0) {
        return;
    }
    if (sel == SEL_NONE) {
        return;
    }
    try_play_step(sel, TO_OFF);
}

static void handle_point_tap(int pt)
{
    int n;

    if (board.side_to_move != SIDE_HUMAN) {
        return;
    }
    if (game_winner(&board) >= 0) {
        return;
    }

    if (sel != SEL_NONE && dest_is_legal(sel, pt)) {
        try_play_step(sel, pt);
        return;
    }

    if (board.bar[SIDE_HUMAN] > 0) {
        sel = FROM_BAR;
        return;
    }

    n = board.point[pt];
    if (n > 0 && from_is_legal(pt)) {
        sel = pt;
        return;
    }

    sel = SEL_NONE;
}

// -----------------  GEOMETRY  --------------------------------

static bool point_is_top(int pt)
{
    if (pt >= 13) {
        return true;
    }
    return false;
}

static int point_x(int pt)
{
    int col, left;

    if (pt >= 13 && pt <= 18) {
        col = pt - 13;
        left = BOARD_X;
    } else if (pt >= 19 && pt <= 24) {
        col = pt - 19;
        left = BOARD_X + 6 * POINT_W + BAR_W;
    } else if (pt >= 7 && pt <= 12) {
        col = 12 - pt;
        left = BOARD_X;
    } else {
        col = 6 - pt;
        left = BOARD_X + 6 * POINT_W + BAR_W;
    }
    return left + col * POINT_W;
}

static int point_y(int pt)
{
    if (point_is_top(pt)) {
        return BOARD_Y;
    }
    return BOARD_Y + POINT_H + MID_GAP;
}

static int bar_x(void)
{
    return BOARD_X + 6 * POINT_W;
}

static void die_pip(int cx, int cy, int ox, int oy, int col, int row, int pr)
{
    sdlx_render_fill_circle(cx + col * ox, cy + row * oy, pr, COLOR_BLACK);
}

static void draw_die(int x, int y, int size, int val)
{
    int cx, cy, ox, oy, pr;

    sdlx_render_fill_rect(x, y, size, size, COLOR_WHITE);
    sdlx_render_rect(x, y, size, size, 3, COLOR_BLACK);
    if (val < 1 || val > 6) {
        return;
    }
    cx = x + size / 2;
    cy = y + size / 2;
    ox = size / 4;
    oy = size / 4;
    pr = size / 11;
    if (pr < 3) {
        pr = 3;
    }

    if (val == 1) {
        die_pip(cx, cy, ox, oy, 0, 0, pr);
    } else if (val == 2) {
        die_pip(cx, cy, ox, oy, -1, -1, pr);
        die_pip(cx, cy, ox, oy, 1, 1, pr);
    } else if (val == 3) {
        die_pip(cx, cy, ox, oy, -1, -1, pr);
        die_pip(cx, cy, ox, oy, 0, 0, pr);
        die_pip(cx, cy, ox, oy, 1, 1, pr);
    } else if (val == 4) {
        die_pip(cx, cy, ox, oy, -1, -1, pr);
        die_pip(cx, cy, ox, oy, 1, -1, pr);
        die_pip(cx, cy, ox, oy, -1, 1, pr);
        die_pip(cx, cy, ox, oy, 1, 1, pr);
    } else if (val == 5) {
        die_pip(cx, cy, ox, oy, -1, -1, pr);
        die_pip(cx, cy, ox, oy, 1, -1, pr);
        die_pip(cx, cy, ox, oy, 0, 0, pr);
        die_pip(cx, cy, ox, oy, -1, 1, pr);
        die_pip(cx, cy, ox, oy, 1, 1, pr);
    } else {
        die_pip(cx, cy, ox, oy, -1, -1, pr);
        die_pip(cx, cy, ox, oy, -1, 0, pr);
        die_pip(cx, cy, ox, oy, -1, 1, pr);
        die_pip(cx, cy, ox, oy, 1, -1, pr);
        die_pip(cx, cy, ox, oy, 1, 0, pr);
        die_pip(cx, cy, ox, oy, 1, 1, pr);
    }
}

static void draw_stack(int cx, int y0, int dir, int n, sdlx_color_t col)
{
    int i, show, cy;
    sdlx_color_t ring;
    char buf[16];

    if (n <= 0) {
        return;
    }
    show = n;
    if (show > MAX_VIS) {
        show = MAX_VIS;
    }
    if (col == COLOR_WHITE) {
        ring = COLOR_GRAY;
    } else {
        ring = COLOR_LIGHT_GRAY;
    }
    for (i = 0; i < show; i++) {
        cy = y0 + dir * (CHK_RADIUS + 4 + i * CHK_STEP);
        sdlx_render_fill_circle(cx, cy, CHK_RADIUS, col);
        sdlx_render_circle(cx, cy, CHK_RADIUS, 2, ring);
        if (i == show - 1) {
            if (n > MAX_VIS) {
                sprintf(buf, "%d", n);
                sdlx_render_printf_ex(cx, cy, FONT_TINY, COLOR_YELLOW, FLAG_XY_CTR,
                                      "%s", buf);
            }
        }
    }
}

// -----------------  DISPLAY  ---------------------------------

static void draw_and_register(void)
{
    int pt, x, y, n, cx, winner, bx, dx, dy, top, cw, ch, new_x;
    sdlx_loc_t loc;
    sdlx_loc_t *ploc;
    sdlx_point_t tri[4];
    sdlx_color_t light, dark, wood, barc, pcol, hi, legal;
    char status[80];
    char nbuf[8];

    light = sdlx_create_color(210, 180, 140, 255);
    dark  = sdlx_create_color(139, 90, 43, 255);
    wood  = sdlx_create_color(70, 45, 25, 255);
    barc  = sdlx_create_color(50, 32, 18, 255);
    hi    = COLOR_YELLOW;
    legal = COLOR_LIGHT_GREEN;

    sdlx_display_init(COLOR_BLACK, LANDSCAPE);

    winner = -1;
    if (game_started) {
        winner = game_winner(&board);
    }
    if (!game_started) {
        status[0] = '\0';
    } else if (winner == SIDE_HUMAN) {
        sprintf(status, "%s", "You Win");
    } else if (winner == SIDE_CPU) {
        sprintf(status, "%s", "CPU Wins");
    } else if (cpu_thinking) {
        sprintf(status, "%s", "CPU thinking...");
    } else if (board.side_to_move == SIDE_HUMAN) {
        sprintf(status, "%s", "Your Move");
    } else {
        sprintf(status, "%s", "CPU Move");
    }

    sdlx_render_fill_rect(BOARD_X - 8, BOARD_Y,
                          12 * POINT_W + BAR_W + 16,
                          2 * POINT_H + MID_GAP,
                          wood);

    bx = bar_x();
    sdlx_render_fill_rect(bx, BOARD_Y, BAR_W, 2 * POINT_H + MID_GAP, barc);

    for (pt = 1; pt <= 24; pt++) {
        x = point_x(pt);
        y = point_y(pt);
        top = point_is_top(pt);
        if (pt & 1) {
            pcol = dark;
        } else {
            pcol = light;
        }
        sdlx_render_fill_rect(x, y, POINT_W, POINT_H, pcol);

        if (top) {
            tri[0].x = x;
            tri[0].y = y;
            tri[1].x = x + POINT_W;
            tri[1].y = y;
            tri[2].x = x + POINT_W / 2;
            tri[2].y = y + POINT_H - 4;
            tri[3].x = x;
            tri[3].y = y;
        } else {
            tri[0].x = x;
            tri[0].y = y + POINT_H;
            tri[1].x = x + POINT_W;
            tri[1].y = y + POINT_H;
            tri[2].x = x + POINT_W / 2;
            tri[2].y = y + 4;
            tri[3].x = x;
            tri[3].y = y + POINT_H;
        }
        sdlx_render_lines(tri, 4, COLOR_BLACK);

        sprintf(nbuf, "%d", pt);
        if (top) {
            sdlx_render_printf_ex(x + POINT_W / 2, y + POINT_H - 18,
                                  FONT_TINY, COLOR_BLACK, FLAG_XY_CTR, "%s", nbuf);
        } else {
            sdlx_render_printf_ex(x + POINT_W / 2, y + 18,
                                  FONT_TINY, COLOR_BLACK, FLAG_XY_CTR, "%s", nbuf);
        }

        if (sel == pt) {
            sdlx_render_rect(x + 2, y + 2, POINT_W - 4, POINT_H - 4, RECT_LW, hi);
        } else if (dest_hi == pt) {
            sdlx_render_rect(x + 2, y + 2, POINT_W - 4, POINT_H - 4, RECT_LW, hi);
        } else if (game_started && board.side_to_move == SIDE_HUMAN && sel != SEL_NONE) {
            if (dest_is_legal(sel, pt)) {
                sdlx_render_rect(x + 4, y + 4, POINT_W - 8, POINT_H - 8, RECT_LW, legal);
            }
        }

        n = board.point[pt];
        cx = x + POINT_W / 2;
        if (n > 0) {
            if (top) {
                draw_stack(cx, y, 1, n, COLOR_WHITE);
            } else {
                draw_stack(cx, y + POINT_H, -1, n, COLOR_WHITE);
            }
        } else if (n < 0) {
            if (top) {
                draw_stack(cx, y, 1, -n, COLOR_BLACK);
            } else {
                draw_stack(cx, y + POINT_H, -1, -n, COLOR_BLACK);
            }
        }

        loc.x = x;
        loc.y = y;
        loc.w = POINT_W;
        loc.h = POINT_H;
        sdlx_register_event(&loc, EVID_PT_BASE + pt);
    }

    if (sel == FROM_BAR) {
        sdlx_render_rect(bx + 2, BOARD_Y + 2, BAR_W - 4, 2 * POINT_H + MID_GAP - 4,
                         RECT_LW, hi);
    }

    draw_stack(bx + BAR_W / 2, BOARD_Y, 1, board.bar[SIDE_CPU], COLOR_BLACK);
    draw_stack(bx + BAR_W / 2, BOARD_Y + 2 * POINT_H + MID_GAP, -1,
               board.bar[SIDE_HUMAN], COLOR_WHITE);

    loc.x = bx;
    loc.y = BOARD_Y;
    loc.w = BAR_W;
    loc.h = 2 * POINT_H + MID_GAP;
    sdlx_register_event(&loc, EVID_BAR);

    sdlx_render_fill_rect(TRAY_X, BOARD_Y, TRAY_W, POINT_H, wood);
    sdlx_render_rect(TRAY_X, BOARD_Y, TRAY_W, POINT_H, 3, COLOR_GRAY);
    sdlx_render_printf_ex(TRAY_X + TRAY_W / 2, BOARD_Y + 16,
                          FONT_TINY, COLOR_LIGHT_GRAY, FLAG_X_CTR, "%s", "CPU");
    draw_stack(TRAY_X + TRAY_W / 2, BOARD_Y + 40, 1, board.off[SIDE_CPU], COLOR_BLACK);

    sdlx_render_fill_rect(TRAY_X, BOARD_Y + POINT_H + MID_GAP, TRAY_W, POINT_H, wood);
    sdlx_render_rect(TRAY_X, BOARD_Y + POINT_H + MID_GAP, TRAY_W, POINT_H, 3, COLOR_GRAY);
    if (game_started && board.side_to_move == SIDE_HUMAN && sel != SEL_NONE) {
        if (dest_is_legal(sel, TO_OFF)) {
            sdlx_render_rect(TRAY_X + 4, BOARD_Y + POINT_H + MID_GAP + 4,
                             TRAY_W - 8, POINT_H - 8, RECT_LW, legal);
        }
    }
    if (dest_hi == 25) {
        sdlx_render_rect(TRAY_X + 2, BOARD_Y + POINT_H + MID_GAP + 2,
                         TRAY_W - 4, POINT_H - 4, RECT_LW, hi);
    }
    sdlx_render_printf_ex(TRAY_X + TRAY_W / 2, BOARD_Y + POINT_H + MID_GAP + 16,
                          FONT_TINY, COLOR_LIGHT_GRAY, FLAG_X_CTR, "%s", "You");
    draw_stack(TRAY_X + TRAY_W / 2, BOARD_Y + 2 * POINT_H + MID_GAP - 8, -1,
               board.off[SIDE_HUMAN], COLOR_WHITE);

    loc.x = TRAY_X;
    loc.y = BOARD_Y + POINT_H + MID_GAP;
    loc.w = TRAY_W;
    loc.h = POINT_H;
    sdlx_register_event(&loc, EVID_OFF);

    dx = BOARD_X + 6 * POINT_W + BAR_W / 2 - DIE_SIZE - 8;
    dy = BOARD_Y + POINT_H + (MID_GAP - DIE_SIZE) / 2;
    if (game_started) {
        if (board.dice[0] >= 1) {
            draw_die(dx, dy, DIE_SIZE, board.dice[0]);
            draw_die(dx + DIE_SIZE + 16, dy, DIE_SIZE, board.dice[1]);
        }
    }

    cw = sdlx_char_width(FONT_NORMAL);
    ch = sdlx_char_height(FONT_NORMAL);
    new_x = sdlx_win_width - 6 * cw;
    ploc = sdlx_render_printf_ex(new_x, 0, FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                 "%s", "New");
    sdlx_register_event(ploc, EVID_NEW_GAME);
    y = ch + RAIL_GAP;
    ploc = sdlx_render_printf_ex(new_x, y, FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                                 "%s", diff_str[difficulty]);
    sdlx_register_event(ploc, EVID_DIFF);
    if (status[0] != '\0') {
        n = sdlx_win_width - (TRAY_X + TRAY_W);
        sdlx_render_printf_ex(new_x, y + ch + RAIL_GAP, FONT_NORMAL, COLOR_WHITE, n,
                              "%s", status);
    }

    reg_event_show_readme_file();
    sdlx_register_control_events(0, NULL,
                                 0, NULL,
                                 EVID_QUIT, "X");

    sdlx_display_present();
}
