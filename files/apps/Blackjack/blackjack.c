#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"

//
// defines
//

#define STATE_BET  0
#define STATE_PLAY 1
#define STATE_DONE 2

#define EVID_HIT      1
#define EVID_STAND    2
#define EVID_DOUBLE   3
#define EVID_DEAL     4
#define EVID_BET_DOWN 5
#define EVID_BET_UP   6
#define EVID_RESET    7

#define MAX_HAND  12
#define BET_STEP  10
#define BANK_START 200

#define CARD_W 168
#define CARD_H 248

#define FONT_RANK 36
#define FONT_SUIT 18

#define SUIT_HEART   0
#define SUIT_DIAMOND 1
#define SUIT_CLUB    2
#define SUIT_SPADE   3

//
// variables
//

char *progname;
char *data_dir;

int deck[52];
int deck_n;

int player[MAX_HAND];
int player_n;
int dealer[MAX_HAND];
int dealer_n;

int state;
int bank;
int bet;
int wager;
int hole_hidden;
char status[80];

sdlx_color_t felt;
sdlx_color_t felt_dark;
sdlx_color_t card_red;
sdlx_color_t card_back;
sdlx_color_t card_back_lite;

char *rank_name[13] = {
    "A", "2", "3", "4", "5", "6", "7",
    "8", "9", "10", "J", "Q", "K"
};

//
// prototypes
//

void game_init(void);
void clamp_bet(void);
void shuffle_deck(void);
int draw_card(void);
int card_rank(int card);
int card_suit(int card);
int hand_value(int *cards, int n);
bool is_blackjack(int *cards, int n);
void start_hand(void);
void player_hit(void);
void player_stand(void);
void player_double(void);
void dealer_play(void);
void settle(void);
void update_display(void);
void draw_button(int x, int y, int w, int h, char *label, int evid,
                 sdlx_color_t bg, sdlx_color_t fg);
void draw_hand(int *cards, int n, int y, bool hide_hole);
void draw_card_face(int x, int y, int card);
void draw_card_back(int x, int y);
void card_row(int n, int *x0, int *step);
void draw_suit(int cx, int cy, int suit, int s, sdlx_color_t color);
void draw_heart(int cx, int cy, int s, sdlx_color_t color);
void draw_diamond(int cx, int cy, int s, sdlx_color_t color);
void draw_club(int cx, int cy, int s, sdlx_color_t color);
void draw_spade(int cx, int cy, int s, sdlx_color_t color);
sdlx_color_t suit_color(int suit);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    sdlx_event_t event;
    bool end_program;

    if (argc != 2) {
        printf("E %s: argc=%d is not 2\n", "Blackjack", argc);
        return 1;
    }
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    srandom((unsigned int)util_microsec_timer());

    felt = sdlx_create_color(10, 92, 48, 255);
    felt_dark = sdlx_create_color(6, 64, 34, 255);
    card_red = sdlx_create_color(190, 24, 36, 255);
    card_back = sdlx_create_color(18, 52, 130, 255);
    card_back_lite = sdlx_create_color(70, 120, 210, 255);

    game_init();
    end_program = false;

    while (!end_program) {
        sdlx_display_init(felt, PORTRAIT);
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
        case EVID_BET_DOWN:
            if (state != STATE_PLAY) {
                if (bet > BET_STEP) {
                    bet = bet - BET_STEP;
                }
            }
            break;
        case EVID_BET_UP:
            if (state != STATE_PLAY) {
                if (bet + BET_STEP <= bank) {
                    bet = bet + BET_STEP;
                }
            }
            break;
        case EVID_DEAL:
            if (state != STATE_PLAY) {
                start_hand();
            }
            break;
        case EVID_HIT:
            if (state == STATE_PLAY) {
                player_hit();
            }
            break;
        case EVID_STAND:
            if (state == STATE_PLAY) {
                player_stand();
            }
            break;
        case EVID_DOUBLE:
            if (state == STATE_PLAY) {
                player_double();
            }
            break;
        case EVID_RESET:
            game_init();
            printf("I %s: bank reset\n", progname);
            break;
        default:
            break;
        }
    }

    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  GAME  ------------------------------------------

void game_init(void)
{
    bank = BANK_START;
    bet = BET_STEP;
    wager = 0;
    player_n = 0;
    dealer_n = 0;
    hole_hidden = 0;
    state = STATE_BET;
    sprintf(status, "%s", "Place your bet");
}

void clamp_bet(void)
{
    if (bet > bank) {
        bet = bank;
    }
    bet = (bet / BET_STEP) * BET_STEP;
    if (bet < BET_STEP) {
        if (bank >= BET_STEP) {
            bet = BET_STEP;
        } else {
            bet = 0;
        }
    }
}

void shuffle_deck(void)
{
    int i, j, tmp;

    for (i = 0; i < 52; i++) {
        deck[i] = i;
    }
    for (i = 51; i > 0; i--) {
        j = (int)(random() % (i + 1));
        tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }
    deck_n = 52;
}

int draw_card(void)
{
    if (deck_n <= 0) {
        shuffle_deck();
    }
    deck_n = deck_n - 1;
    return deck[deck_n];
}

int card_rank(int card)
{
    return card % 13;
}

int card_suit(int card)
{
    return card / 13;
}

int hand_value(int *cards, int n)
{
    int i, r, total, aces;

    total = 0;
    aces = 0;
    for (i = 0; i < n; i++) {
        r = card_rank(cards[i]);
        if (r == 0) {
            total = total + 11;
            aces = aces + 1;
        } else if (r >= 9) {
            total = total + 10;
        } else {
            total = total + r + 1;
        }
    }
    while (total > 21) {
        if (aces <= 0) {
            break;
        }
        total = total - 10;
        aces = aces - 1;
    }
    return total;
}

bool is_blackjack(int *cards, int n)
{
    if (n != 2) {
        return false;
    }
    if (hand_value(cards, n) != 21) {
        return false;
    }
    return true;
}

void start_hand(void)
{
    int up;

    clamp_bet();
    if (bet < BET_STEP) {
        sprintf(status, "%s", "Out of chips");
        return;
    }
    if (bank < bet) {
        sprintf(status, "%s", "Bet is more than you have");
        return;
    }

    shuffle_deck();
    player_n = 0;
    dealer_n = 0;

    player[player_n] = draw_card();
    player_n = player_n + 1;
    dealer[dealer_n] = draw_card();
    dealer_n = dealer_n + 1;
    player[player_n] = draw_card();
    player_n = player_n + 1;
    dealer[dealer_n] = draw_card();
    dealer_n = dealer_n + 1;

    bank = bank - bet;
    wager = bet;
    hole_hidden = 1;
    state = STATE_PLAY;
    sprintf(status, "%s", "Hit or Stand");
    printf("I %s: deal bet=%d bank=%d\n", progname, wager, bank);

    up = card_rank(dealer[0]);
    if (up == 0) {
        if (is_blackjack(dealer, dealer_n)) {
            hole_hidden = 0;
            if (is_blackjack(player, player_n)) {
                bank = bank + wager;
                state = STATE_DONE;
                sprintf(status, "%s", "Push - both blackjack");
            } else {
                state = STATE_DONE;
                sprintf(status, "%s", "Dealer blackjack");
            }
            printf("I %s: %s bank=%d\n", progname, status, bank);
            return;
        }
    } else if (up >= 9) {
        if (is_blackjack(dealer, dealer_n)) {
            hole_hidden = 0;
            if (is_blackjack(player, player_n)) {
                bank = bank + wager;
                state = STATE_DONE;
                sprintf(status, "%s", "Push - both blackjack");
            } else {
                state = STATE_DONE;
                sprintf(status, "%s", "Dealer blackjack");
            }
            printf("I %s: %s bank=%d\n", progname, status, bank);
            return;
        }
    }

    if (is_blackjack(player, player_n)) {
        hole_hidden = 0;
        bank = bank + wager + (wager * 3) / 2;
        state = STATE_DONE;
        sprintf(status, "%s", "Blackjack! Pays 3 to 2");
        printf("I %s: %s bank=%d\n", progname, status, bank);
    }
}

void player_hit(void)
{
    int v;

    if (player_n >= MAX_HAND) {
        return;
    }
    player[player_n] = draw_card();
    player_n = player_n + 1;
    v = hand_value(player, player_n);
    printf("I %s: hit value=%d\n", progname, v);
    if (v > 21) {
        hole_hidden = 0;
        state = STATE_DONE;
        sprintf(status, "%s", "You bust");
        printf("I %s: bust bank=%d\n", progname, bank);
        return;
    }
    if (v == 21) {
        player_stand();
    }
}

void player_stand(void)
{
    printf("I %s: stand value=%d\n", progname, hand_value(player, player_n));
    dealer_play();
    settle();
}

void player_double(void)
{
    int v;

    if (player_n != 2) {
        return;
    }
    if (bank < wager) {
        sprintf(status, "%s", "Not enough to double");
        return;
    }
    bank = bank - wager;
    wager = wager + wager;
    player[player_n] = draw_card();
    player_n = player_n + 1;
    v = hand_value(player, player_n);
    printf("I %s: double wager=%d value=%d\n", progname, wager, v);
    if (v > 21) {
        hole_hidden = 0;
        state = STATE_DONE;
        sprintf(status, "%s", "You bust");
        printf("I %s: bust bank=%d\n", progname, bank);
        return;
    }
    dealer_play();
    settle();
}

void dealer_play(void)
{
    int v;

    hole_hidden = 0;
    v = hand_value(dealer, dealer_n);
    while (v < 17) {
        if (dealer_n >= MAX_HAND) {
            break;
        }
        dealer[dealer_n] = draw_card();
        dealer_n = dealer_n + 1;
        v = hand_value(dealer, dealer_n);
    }
    printf("I %s: dealer value=%d cards=%d\n", progname, v, dealer_n);
}

void settle(void)
{
    int pv, dv;

    pv = hand_value(player, player_n);
    dv = hand_value(dealer, dealer_n);
    state = STATE_DONE;

    if (dv > 21) {
        bank = bank + wager + wager;
        sprintf(status, "%s", "Dealer busts - you win");
    } else if (pv > dv) {
        bank = bank + wager + wager;
        sprintf(status, "%s", "You win");
    } else if (pv == dv) {
        bank = bank + wager;
        sprintf(status, "%s", "Push");
    } else {
        sprintf(status, "%s", "Dealer wins");
    }
    printf("I %s: %s bank=%d\n", progname, status, bank);
    clamp_bet();
}

// -----------------  DISPLAY  ---------------------------------------

void draw_button(int x, int y, int w, int h, char *label, int evid,
                 sdlx_color_t bg, sdlx_color_t fg)
{
    sdlx_loc_t loc;

    sdlx_render_fill_rect(x, y, w, h, bg);
    sdlx_render_rect(x, y, w, h, 4, COLOR_WHITE);
    sdlx_render_printf_ex(x + w / 2, y + h / 2,
                          FONT_SMALL, fg, FLAG_XY_CTR,
                          "%s", label);
    if (evid > 0) {
        loc.x = x;
        loc.y = y;
        loc.w = w;
        loc.h = h;
        sdlx_register_event(&loc, evid);
    }
}

void card_row(int n, int *x0, int *step)
{
    int span, avail, gap;

    avail = sdlx_win_width - 48;
    if (n <= 1) {
        *step = 0;
        *x0 = (sdlx_win_width - CARD_W) / 2;
        return;
    }
    gap = (avail - CARD_W) / (n - 1);
    if (gap > CARD_W + 16) {
        gap = CARD_W + 16;
    }
    *step = gap;
    span = CARD_W + gap * (n - 1);
    *x0 = (sdlx_win_width - span) / 2;
}

sdlx_color_t suit_color(int suit)
{
    if (suit == SUIT_HEART) {
        return card_red;
    }
    if (suit == SUIT_DIAMOND) {
        return card_red;
    }
    return COLOR_BLACK;
}

void draw_diamond(int cx, int cy, int s, sdlx_color_t color)
{
    int y, half;

    for (y = -s; y <= s; y++) {
        if (y < 0) {
            half = s + y;
        } else {
            half = s - y;
        }
        if (half < 1) {
            half = 1;
        }
        sdlx_render_fill_rect(cx - half, cy + y, half * 2, 2, color);
    }
}

void draw_heart(int cx, int cy, int s, sdlx_color_t color)
{
    int y, half, rad;

    rad = (s * 5) / 8;
    if (rad < 2) {
        rad = 2;
    }
    sdlx_render_fill_circle(cx - s / 2, cy - s / 4, rad, color);
    sdlx_render_fill_circle(cx + s / 2, cy - s / 4, rad, color);
    for (y = 0; y <= s; y++) {
        half = s - y;
        if (half < 1) {
            half = 1;
        }
        sdlx_render_fill_rect(cx - half, cy + y, half * 2, 2, color);
    }
}

void draw_club(int cx, int cy, int s, sdlx_color_t color)
{
    int rad, stem_w;

    rad = s / 2;
    if (rad < 2) {
        rad = 2;
    }
    sdlx_render_fill_circle(cx, cy - s / 2, rad, color);
    sdlx_render_fill_circle(cx - s / 2, cy + s / 6, rad, color);
    sdlx_render_fill_circle(cx + s / 2, cy + s / 6, rad, color);
    stem_w = s / 3;
    if (stem_w < 2) {
        stem_w = 2;
    }
    sdlx_render_fill_rect(cx - stem_w / 2, cy, stem_w, s, color);
}

void draw_spade(int cx, int cy, int s, sdlx_color_t color)
{
    int y, half, rad, stem_w;

    rad = (s * 5) / 8;
    if (rad < 2) {
        rad = 2;
    }
    sdlx_render_fill_circle(cx - s / 2, cy + s / 5, rad, color);
    sdlx_render_fill_circle(cx + s / 2, cy + s / 5, rad, color);
    for (y = 0; y <= s; y++) {
        half = s - y;
        if (half < 1) {
            half = 1;
        }
        sdlx_render_fill_rect(cx - half, cy - y, half * 2, 2, color);
    }
    stem_w = s / 4;
    if (stem_w < 2) {
        stem_w = 2;
    }
    sdlx_render_fill_rect(cx - stem_w / 2, cy + s / 5, stem_w, s / 2, color);
}

void draw_suit(int cx, int cy, int suit, int s, sdlx_color_t color)
{
    if (suit == SUIT_HEART) {
        draw_heart(cx, cy, s, color);
    } else if (suit == SUIT_DIAMOND) {
        draw_diamond(cx, cy, s, color);
    } else if (suit == SUIT_CLUB) {
        draw_club(cx, cy, s, color);
    } else {
        draw_spade(cx, cy, s, color);
    }
}

void draw_card_back(int x, int y)
{
    int m;

    m = 14;
    sdlx_render_fill_rect(x, y, CARD_W, CARD_H, COLOR_WHITE);
    sdlx_render_fill_rect(x + 8, y + 8, CARD_W - 16, CARD_H - 16, card_back);
    sdlx_render_rect(x + m, y + m, CARD_W - 2 * m, CARD_H - 2 * m, 4, card_back_lite);
    draw_diamond(x + CARD_W / 2, y + CARD_H / 2, 28, card_back_lite);
}

void draw_card_face(int x, int y, int card)
{
    int suit, rank;
    sdlx_color_t color;
    char *name;

    suit = card_suit(card);
    rank = card_rank(card);
    color = suit_color(suit);
    name = rank_name[rank];

    sdlx_render_fill_rect(x, y, CARD_W, CARD_H, COLOR_WHITE);
    sdlx_render_rect(x, y, CARD_W, CARD_H, 3, COLOR_DARK_GRAY);

    sdlx_render_printf_ex(x + 14, y + 8,
                          FONT_RANK, color, FLAG_NONE,
                          "%s", name);
    draw_suit(x + 36, y + 78, suit, 10, color);

    draw_suit(x + CARD_W / 2, y + CARD_H / 2 + 8, suit, 36, color);

    sdlx_render_printf_ex(x + CARD_W - 28, y + CARD_H - 36,
                          FONT_RANK, color, FLAG_XY_CTR | FLAG_ROT_CTR_180,
                          "%s", name);
}

void draw_hand(int *cards, int n, int y, bool hide_hole)
{
    int i, x, step, x0;

    if (n <= 0) {
        return;
    }
    card_row(n, &x0, &step);
    for (i = 0; i < n; i++) {
        x = x0 + i * step;
        if (hide_hole) {
            if (i == 1) {
                draw_card_back(x, y);
            } else {
                draw_card_face(x, y, cards[i]);
            }
        } else {
            draw_card_face(x, y, cards[i]);
        }
    }
}

void update_display(void)
{
    int x, bw, bh, gap, yb;
    int pv, dv, shown;
    bool can_double;
    sdlx_color_t msg_color;
    char line[64];

    sdlx_render_printf_ex(sdlx_win_width / 2, 56,
                          16, COLOR_LIGHT_GREEN, FLAG_XY_CTR,
                          "%s", "Blackjack");

    sprintf(line, "Bank  %d", bank);
    sdlx_render_printf_ex(sdlx_win_width / 2, 140,
                          FONT_SMALL, COLOR_WHITE, FLAG_XY_CTR,
                          "%s", line);

    if (state == STATE_PLAY) {
        sprintf(line, "Wager  %d", wager);
    } else {
        sprintf(line, "Bet  %d", bet);
    }
    sdlx_render_printf_ex(sdlx_win_width / 2, 210,
                          FONT_SMALL, COLOR_YELLOW, FLAG_XY_CTR,
                          "%s", line);

    msg_color = COLOR_WHITE;
    if (state == STATE_DONE) {
        if (strstr(status, "you win") != NULL) {
            msg_color = COLOR_LIGHT_GREEN;
        } else if (strstr(status, "You win") != NULL) {
            msg_color = COLOR_LIGHT_GREEN;
        } else if (strstr(status, "Blackjack!") != NULL) {
            msg_color = COLOR_LIGHT_GREEN;
        } else if (strstr(status, "Dealer") != NULL) {
            msg_color = COLOR_ORANGE;
        } else if (strstr(status, "bust") != NULL) {
            msg_color = COLOR_ORANGE;
        }
    }
    sdlx_render_printf_ex(sdlx_win_width / 2, 290,
                          FONT_SMALL, msg_color, FLAG_XY_CTR,
                          "%s", status);

    sdlx_render_fill_rect(0, 360, sdlx_win_width, 4, felt_dark);

    if (dealer_n > 0) {
        if (hole_hidden) {
            shown = 1;
        } else {
            shown = dealer_n;
        }
        dv = hand_value(dealer, shown);
        if (hole_hidden) {
            sprintf(line, "Dealer  %d + ?", dv);
        } else if (dv > 21) {
            sprintf(line, "Dealer  Bust");
        } else {
            sprintf(line, "Dealer  %d", dv);
        }
    } else {
        sprintf(line, "%s", "Dealer");
    }
    sdlx_render_printf_ex(40, 390, FONT_SMALL, COLOR_LIGHT_BLUE, FLAG_NONE,
                          "%s", line);
    draw_hand(dealer, dealer_n, 460, hole_hidden);

    if (player_n > 0) {
        pv = hand_value(player, player_n);
        if (pv > 21) {
            sprintf(line, "You  Bust");
        } else if (is_blackjack(player, player_n)) {
            sprintf(line, "%s", "You  Blackjack");
        } else {
            sprintf(line, "You  %d", pv);
        }
    } else {
        sprintf(line, "%s", "You");
    }
    sdlx_render_printf_ex(40, 760, FONT_SMALL, COLOR_ORANGE, FLAG_NONE,
                          "%s", line);
    draw_hand(player, player_n, 830, false);

    bw = 280;
    bh = 130;
    gap = 30;
    x = (sdlx_win_width - (3 * bw + 2 * gap)) / 2;
    yb = 1680;

    if (bank < BET_STEP) {
        if (state != STATE_PLAY) {
            draw_button((sdlx_win_width - 420) / 2, yb, 420, bh,
                        "Reset Bank", EVID_RESET, COLOR_ORANGE, COLOR_BLACK);
        }
    } else if (state == STATE_PLAY) {
        can_double = false;
        if (player_n == 2) {
            if (bank >= wager) {
                can_double = true;
            }
        }
        draw_button(x, yb, bw, bh, "Hit", EVID_HIT, COLOR_LIGHT_GREEN, COLOR_BLACK);
        draw_button(x + bw + gap, yb, bw, bh, "Stand", EVID_STAND, COLOR_YELLOW, COLOR_BLACK);
        if (can_double) {
            draw_button(x + 2 * (bw + gap), yb, bw, bh,
                        "Double", EVID_DOUBLE, COLOR_LIGHT_BLUE, COLOR_BLACK);
        } else {
            draw_button(x + 2 * (bw + gap), yb, bw, bh,
                        "Double", 0, COLOR_DARK_GRAY, COLOR_GRAY);
        }
    } else {
        draw_button(x, yb, bw, bh, "Bet -", EVID_BET_DOWN, COLOR_WHITE, COLOR_BLACK);
        draw_button(x + bw + gap, yb, bw, bh, "Deal", EVID_DEAL, COLOR_LIGHT_GREEN, COLOR_BLACK);
        draw_button(x + 2 * (bw + gap), yb, bw, bh, "Bet +", EVID_BET_UP, COLOR_WHITE, COLOR_BLACK);
    }

    reg_event_show_readme_file();
    sdlx_register_control_events(0, NULL,
                                 0, NULL,
                                 EVID_QUIT, "X");
}
