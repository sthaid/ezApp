#include <apps/Reversi/common.h>

//
// defines
//

#define INFIN  0x7fffffff

//
// variables
//

//
// prototypes
//

static int min(int a, int b);
static int max(int a, int b);
static void setbit(unsigned char *bm, int idx);
static bool getbit(unsigned char *bm, int idx);

static int alphabeta(board_t *b, int depth, int alpha, int beta, bool maximizing_player, int *move);
static void init_edge_gateway_to_corner(void);
static int heuristic(board_t *b, bool maximizing_player, bool game_over, possible_moves_t *pm);

// -----------------  CPU PLAYER - GET_MOVE ---------------------------------

int cpu_get_move(int level, board_t *b, char *eval_str)
{
    int value, best_move, depth;

    char *my_color = (b->whose_turn == BLACK ? "BLACK" : "WHITE");
    char *other_color = (b->whose_turn == BLACK ? "WHITE" : "BLACK");

    // sanity check level arg
    if (level < 1 || level > 8) {
        printf("E %s: invlaid level %d\n", progname, level);
        return MOVE_PASS;
    }

    // initialization
    init_edge_gateway_to_corner();

    // get lookahead depth
    depth = ((b->black_cnt + b->white_cnt) >= (56 - level)) ? 20 : level;

    // call alphabeta to get the best move, and associated heuristic value
    value = alphabeta(b, depth, -INFIN, INFIN, true, &best_move);

    // create eval str, to be returned below
    eval_str[0] = '\0';
    if (value < -10000000) {
        sprintf(eval_str, "%s to win by %d", other_color, value / -10000000);
    } else if (value > 10000000) {
        sprintf(eval_str, "%s to win by %d", my_color, value / 10000000);
    }

    // return the move, that was obtained by call to alphabeta
    return best_move;
}

static int min(int a, int b) {
    return a < b ? a : b;
}

static int max(int a, int b) {
    return a > b ? a : b;
}

static void setbit(unsigned char *bm, int idx) {
    bm[idx/8] |= (1 << (idx&7));
}

static bool getbit(unsigned char *bm, int idx) {
    return bm[idx/8] & (1 << (idx&7));
}

// -----------------  CHOOSE BEST MOVE - ALPHA / BETA  ----------------------

// 
// The following is the alpha-beta pruning algorithm, ported from
//  https://en.wikipedia.org/wiki/Alpha%E2%80%93beta_pruning
// The algorithm has been updated to return the best_move along with the value.
// 

static int alphabeta(board_t *b, int depth, int alpha, int beta, bool maximizing_player, int *move)
{
    int              value, old_value;
    int              i, best_move = MOVE_PASS;
    board_t          b_child;
    bool             game_over;
    possible_moves_t pm;

    game_over = false;
    get_possible_moves(b, &pm);
    if (pm.max == 0) {
        b->whose_turn = OTHER_COLOR(b->whose_turn);
        if (!any_possible_moves(b)) {
            game_over = true;
        } else {
            pm.max = 1;
            pm.move[0] = MOVE_PASS;
        }
        b->whose_turn = OTHER_COLOR(b->whose_turn);
    }

    if (depth == 0 || game_over) {
        if (move) *move = MOVE_PASS;
        return heuristic(b, maximizing_player, game_over, &pm);
    }

    if (maximizing_player) {
        value = -INFIN;
        for (i = 0; i < pm.max; i++) {
            b_child = *b;
            apply_move(&b_child, pm.move[i]);
            old_value = value;
            value = max(value, alphabeta(&b_child, depth-1, alpha, beta, false, NULL));
            if (value > old_value) {
                best_move = pm.move[i];
            }
            if (value >= beta) {
                break;
            }
            alpha = max(alpha, value);
        }
        if (move) *move = best_move;
        return value;
    } else {
        value = INFIN;
        for (i = 0; i < pm.max; i++) {
            b_child = *b;
            apply_move(&b_child, pm.move[i]);
            value = min(value, alphabeta(&b_child, depth-1, alpha, beta, true, NULL));
            if (value <= alpha) {
                break;
            }
            beta = min(beta, value);
        }
        return value;
    }
}

// -----------------  HEURISTIC  ---------------------------------------------------

// - - - - - - corner_count - - - - - - -

static int corner_count(board_t *b)
{
    int cnt = 0;
    int my_color = b->whose_turn;
    int other_color = OTHER_COLOR(my_color);

    if (b->pos[1][1] == my_color) cnt++;
    if (b->pos[1][1] == other_color) cnt--;
    if (b->pos[1][8] == my_color) cnt++;
    if (b->pos[1][8] == other_color) cnt--;
    if (b->pos[8][1] == my_color) cnt++;
    if (b->pos[8][1] == other_color) cnt--;
    if (b->pos[8][8] == my_color) cnt++;
    if (b->pos[8][8] == other_color) cnt--;
    return cnt;
}

// - - - - - - corner_moves - - - - - - -

struct corner_move_tbl_s {
    int r;
    int c;
    int r_incr[3];
    int c_incr[3];
};

int corner_move_tbl_data[] = {
        1,1,  0, 1, 1,   1, 0, 1,    // 0: top left
        1,8,  0, 1, 1,  -1, 0,-1,    // 1: top right
        8,8,  0,-1,-1,  -1, 0,-1,    // 2: bottom right
        8,1,  0,-1,-1,   1, 0, 1,    // 3: bottom left
                    };

static bool is_corner_move_possible(board_t *b, int which_corner)
{
    int r, c, i, my_color, other_color;
    struct corner_move_tbl_s *corner_move_tbl = (void*)corner_move_tbl_data;

    r = corner_move_tbl[which_corner].r;
    c = corner_move_tbl[which_corner].c;
    if (b->pos[r][c] != NONE) {
        return false;
    }

    my_color    = b->whose_turn;
    other_color = OTHER_COLOR(my_color);
    for (i = 0; i < 3; i++) {
        int r_incr = corner_move_tbl[which_corner].r_incr[i];
        int c_incr = corner_move_tbl[which_corner].c_incr[i];
        int r_next = r + r_incr;
        int c_next = c + c_incr;
        int cnt    = 0;

        while (b->pos[r_next][c_next] == other_color) {
            r_next += r_incr;
            c_next += c_incr;
            cnt++;
        }

        if (cnt > 0 && b->pos[r_next][c_next] == my_color) {
            return true;
        }
    }

    return false;
}

static int corner_moves(board_t *b)
{
    int cnt = 0;
    int my_color = b->whose_turn;
    int other_color = OTHER_COLOR(my_color);
    int which_corner;

    for (which_corner = 0; which_corner < 4; which_corner++) {
        if (is_corner_move_possible(b, which_corner)) cnt++;
    }

    b->whose_turn = other_color;
    for (which_corner = 0; which_corner < 4; which_corner++) {
        if (is_corner_move_possible(b, which_corner)) cnt--;
    }
    b->whose_turn = my_color;

    return cnt;
}

// - - - - - - diagonal_gateway_to_corner  - - - - - - -

static int diagonal_gateways_to_corner(board_t *b)
{
    int cnt = 0;
    int my_color = b->whose_turn;
    int other_color = OTHER_COLOR(my_color);

    if (b->pos[1][1] == NONE) {
        if (b->pos[2][2] == other_color) cnt++;
        if (b->pos[2][2] == my_color) cnt--;
    }
    if (b->pos[1][8] == NONE) {
        if (b->pos[2][7] == other_color) cnt++;
        if (b->pos[2][7] == my_color) cnt--;
    }
    if (b->pos[8][1] == NONE) {
        if (b->pos[7][2] == other_color) cnt++;
        if (b->pos[7][2] == my_color) cnt--;
    }
    if (b->pos[8][8] == NONE) {
        if (b->pos[7][7] == other_color) cnt++;
        if (b->pos[7][7] == my_color) cnt--;
    }

    return cnt;
}

// - - - - - - edge_gateway_to_corner  - - - - - - -

unsigned char black_gateway_to_corner_bitmap[8192];
unsigned char white_gateway_to_corner_bitmap[8192];

static int HORIZONTAL_EDGE(board_t *b, int r)
{
    return ((b->pos[r][1] << 14) |
            (b->pos[r][2] << 12) | 
            (b->pos[r][3] << 10) | 
            (b->pos[r][4] <<  8) | 
            (b->pos[r][5] <<  6) | 
            (b->pos[r][6] <<  4) | 
            (b->pos[r][7] <<  2) | 
            (b->pos[r][8] <<  0));
}

static int VERTICAL_EDGE(board_t *b, int c)
{
    return ((b->pos[1][c] << 14) | 
            (b->pos[2][c] << 12) | 
            (b->pos[3][c] << 10) | 
            (b->pos[4][c] <<  8) | 
            (b->pos[5][c] <<  6) | 
            (b->pos[6][c] <<  4) | 
            (b->pos[7][c] <<  2) | 
            (b->pos[8][c] <<  0));
}

static int REVERSE(int x)
{
    return ((((x) & 0x0003) << 14) | 
            (((x) & 0x000c) << 10) | 
            (((x) & 0x0030) <<  6) | 
            (((x) & 0x00c0) <<  2) | 
            (((x) & 0x0300) >>  2) | 
            (((x) & 0x0c00) >>  6) | 
            (((x) & 0x3000) >> 10) | 
            (((x) & 0xc000) >> 14));
}

static int edge_gateway_to_corner(board_t *b)
{
    int       my_color    = b->whose_turn;
    int       other_color = OTHER_COLOR(my_color);
    int       cnt         = 0;
    unsigned short  edge;
    unsigned char  *my_color_gateway_to_corner_bitmap;
    unsigned char  *other_color_gateway_to_corner_bitmap;

    if (my_color == BLACK) {
        my_color_gateway_to_corner_bitmap = black_gateway_to_corner_bitmap;
    } else {
        my_color_gateway_to_corner_bitmap = white_gateway_to_corner_bitmap;
    }

    if (other_color == BLACK) {
        other_color_gateway_to_corner_bitmap = black_gateway_to_corner_bitmap;
    } else {
        other_color_gateway_to_corner_bitmap = white_gateway_to_corner_bitmap;
    }
    
    edge = HORIZONTAL_EDGE(b,1);
    if (getbit(my_color_gateway_to_corner_bitmap,edge)) cnt++;
    if (getbit(other_color_gateway_to_corner_bitmap,edge)) cnt--;

    edge = HORIZONTAL_EDGE(b,8);
    if (getbit(my_color_gateway_to_corner_bitmap,edge)) cnt++;
    if (getbit(other_color_gateway_to_corner_bitmap,edge)) cnt--;

    edge = VERTICAL_EDGE(b,1);
    if (getbit(my_color_gateway_to_corner_bitmap,edge)) cnt++;
    if (getbit(other_color_gateway_to_corner_bitmap,edge)) cnt--;

    edge = VERTICAL_EDGE(b,8);
    if (getbit(my_color_gateway_to_corner_bitmap,edge)) cnt++;
    if (getbit(other_color_gateway_to_corner_bitmap,edge)) cnt--;

    return cnt;
}

char *black_gateway_to_corner_patterns[] = {
                ".W......",
                ".W.W....",
                ".W.WW...",
                ".W.WWW..",
                ".W.WWWW.",
                ".WW.W...",
                ".WW.WW..",
                ".WW.WWW.",
                ".WWW.W..",
                ".WWW.WW.",
                ".W...W..",
                ".W...WW.",
                ".W..B...",
                ".WW..B..",
                ".W.B.B..",
                ".W.BBBW.",
                ".W.BBW..",
                ".W.BBWW.",
                ".W.BWW..",
                ".W.BWWW.",
                ".W.BW...",
                ".W.BW.W." 
                            };

#define MAX_BLACK_GATEWAY_TO_CORNER_PATTERNS (sizeof(black_gateway_to_corner_patterns)/sizeof(char*))

static void init_edge_gateway_to_corner(void)
{
    static bool initialized;

    if (initialized == true) {
        return;
    }
    initialized = true;
        
    // check for picoc issue with static init array
    if (MAX_BLACK_GATEWAY_TO_CORNER_PATTERNS == 0) {
        printf("E %s: FIXME MAX_BLACK_GATEWAY_TO_CORNER_PATTERNS = %d\n",
               progname, (int)MAX_BLACK_GATEWAY_TO_CORNER_PATTERNS);
        exit(1);
    }

    int i,j;
    unsigned short edge, edge_reversed;

    for (i = 0; i < MAX_BLACK_GATEWAY_TO_CORNER_PATTERNS; i++) {
        char *s = black_gateway_to_corner_patterns[i];
        edge = 0;
        for (j = 0; j < 8; j++) {
            if (s[j] == 'W') {
                edge |= (WHITE << (14-2*j));
            } else if (s[j] == 'B') {
                edge |= (BLACK << (14-2*j));
            }
        }
        edge_reversed = REVERSE(edge);
        setbit(black_gateway_to_corner_bitmap, edge);
        setbit(black_gateway_to_corner_bitmap, edge_reversed);
    }

    for (i = 0; i < MAX_BLACK_GATEWAY_TO_CORNER_PATTERNS; i++) {
        char *s = black_gateway_to_corner_patterns[i];
        edge = 0;
        for (j = 0; j < 8; j++) {
            if (s[j] == 'W') {
                edge |= (BLACK << (14-2*j));
            } else if (s[j] == 'B') {
                edge |= (WHITE << (14-2*j));
            }
        }
        edge_reversed = REVERSE(edge);
        setbit(white_gateway_to_corner_bitmap, edge);
        setbit(white_gateway_to_corner_bitmap, edge_reversed);
    }
}

static int reasonable_moves(board_t *b, possible_moves_t *pm)
{
    int i, cnt = pm->max;

    for (i = 0; i < pm->max; i++) {
        int r,c;
        move_to_rc(pm->move[i], &r, &c);
        if ((r == 2 && c == 2 && b->pos[1][1] == NONE) ||
            (r == 2 && c == 7 && b->pos[1][8] == NONE) ||
            (r == 7 && c == 2 && b->pos[8][1] == NONE) ||
            (r == 7 && c == 7 && b->pos[8][8] == NONE))
        {
            cnt--;
        }
    }

    return cnt;
}

// - - - - - - heuristic   - - - - - - -

static int heuristic(board_t *b, bool maximizing_player, bool game_over, possible_moves_t *pm)
{
    int value;

    // handle game over case
    if (game_over) {
        int piece_cnt_diff;

        // the game is over, return a large positive or negative value, 
        // incorporating by how many pieces the game has been won or lost
        piece_cnt_diff = (b->whose_turn == BLACK ? b->black_cnt - b->white_cnt
                                                 : b->white_cnt - b->black_cnt);
        value = (piece_cnt_diff) * 10000000;

        // the returned heuristic value measures the favorability 
        // for the maximizing player
        //printf("EOG HEURISTIC = %d\n", (maximizing_player ? value : -value));

        if (!maximizing_player) {
              value = -value;
        }

        return value;
    }

    // game is not over ...

    // The following board characteristics are utilized to generate the heuristic value.
    //
    // These are listed in order of importance / numeric weight.
    //
    // Except as noted, each characteristic is evaluated as a poistive for the color 
    // whose turn it is, and a negative for the other color. For example, if it is
    // black's turn and black has pieces in 2 corners and white has pieces in 1 corner
    // then the corner_count would be 1.
    //
    // - corner_count: count of occupied corners
    //
    // - corner_moves: number of corners that can be captured
    //
    // - diagonal_gateways_to_corner: number of pieces that are diagonally inside
    //   an unoccupied corner; this count is negative for my_color and positive for other_color
    //
    // - edge_gateways_to_corner: certain edge patterns can lead to future corner 
    //   capture; this counts the number of occurances of an edge pattern that can
    //   lead to future corner capture
    //
    // - reasonable_moves: this characteristic is evaluated only for the color whose turn it
    //   is; this is the count of possible moves minus possible moves that are not
    //   reasonable; the moves that are not considered reasonable are moves that provide
    //   a diagonal gateway to a corner
    //
    // - random value so that the same move is not always chosen; this covers the
    //   case where heuristic value calculated from the above characteristics are the
    //   same for differnent board inputs

    value = 0;
    value += (corner_count(b))                * 1000000;
    value += (corner_moves(b))                * 100000;
    value += (diagonal_gateways_to_corner(b)) * 10000;
    value += (edge_gateway_to_corner(b))      * 1000;
    value += (reasonable_moves(b, pm)         * 10);
    value += (random() % 10);

    // the returned heuristic value measures the favorability 
    // for the maximizing player
    if (!maximizing_player) {
        value = -value;
    }

    return value;
}
