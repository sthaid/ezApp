#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef LINUX
#include <sys/wait.h>
#endif

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"

//
// Stock prices and history come from the Yahoo Finance chart API:
//   https://query1.finance.yahoo.com/v8/finance/chart/SYMBOL
//

#define NUM_DUR      7
#define MAX_POINTS   600
#define MAX_SYMBOL   20
#define CACHE_SECS   60

#define EVID_SYMBOL  1
#define EVID_RELOAD  2
#define EVID_DUR     10

#define DUR_1D   0
#define DUR_5D   1
#define DUR_1M   2
#define DUR_6M   3
#define DUR_1Y   4
#define DUR_5Y   5
#define DUR_YTD  6

#define HEADER "\"User-Agent: Mozilla/5.0 (Linux; Android 14) ezApp-Stock\""

#define BTN_H  120
#define BTN_GAP 36

//
// typedefs
//

typedef struct {
    char   symbol[MAX_SYMBOL];
    char   name[100];
    char   currency[12];
    double price;
    double prev_close;
    double day_change;
    double day_pct;
    int    price_hint;
    long   market_time;
    bool   has_price;
    bool   has_prev;
    bool   has_day;
    int    npoints;
    double pt_price[MAX_POINTS];
    long   pt_time[MAX_POINTS];
    bool   pt_valid[MAX_POINTS];
} quote_t;

//
// variables
//

char *progname;
char *data_dir;

char *dur_label[NUM_DUR]    = { "1D", "5D", "1M", "6M", "1Y", "5Y", "YTD" };
char *dur_range[NUM_DUR]    = { "1d", "5d", "1mo", "6mo", "1y", "5y", "ytd" };
char *dur_interval[NUM_DUR] = { "5m", "15m", "1d", "1d", "1d", "1wk", "1d" };
char *dow_name[7]           = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

char    ticker[MAX_SYMBOL];
int     duration;
bool    need_load;
bool    force_reload;
bool    have_quote;
char    error_msg[160];

quote_t quote;
quote_t scratch;

//
// prototypes
//

void load_params(void);
int load_quote(void);
int download_chart(char *filename);
int parse_chart_file(char *filename);
int read_meta(void *result, double *chg_pct, bool *has_pct);
int read_points(void *result);
int finalize_quote(double chg_pct, bool has_pct);
int count_timestamps(void *result);

void draw_loading(void);
void draw_error(void);
void draw_quote(void);
void draw_graph(int top);
void draw_duration_buttons(void);
void draw_screen(void);

void select_duration(int d);
void enter_ticker(void);
void process_event(sdlx_event_t *event);

void set_error(char *msg);
void copy_str(char *dst, char *src, int dstsz);
bool sanitize_ticker(char *s);
void url_encode_symbol(char *src, char *dst, int dstsz);
void chart_filename(char *buf, int buflen);
int duration_button_y(void);
int font_for_text(char *s, int min_id, int max_id);
sdlx_color_t change_color(double v);
bool series_ends(double *p0, double *p1);
void format_price(char *buf, int buflen, double value, int hint);
void format_money(char *buf, int buflen, double value, int hint, char *currency);
void format_change_line(char *buf, int buflen, double change, double pct, int hint, char *currency);
void format_clock(struct tm *tm, char *buf, int buflen);
void format_asof(char *buf, int buflen, long unix_t);
void format_axis_time(char *buf, int buflen, long unix_t);
char *dow_str(int w);
int price_to_y(double price, double axis_min, double axis_max, int top, int height);
int index_to_x(int i, int n, int left, int width);
void draw_text_right(int right_x, int y, int fontid, sdlx_color_t color, char *s);

// -----------------  MAIN  ------------------------------------------

int main(int argc, char **argv)
{
    sdlx_event_t event;
    bool end_program = false;

    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", "Stock");
        return 1;
    }
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);
    load_params();
    need_load = true;

    while (!end_program) {
        sdlx_display_init(COLOR_BLACK, PORTRAIT);
        draw_screen();
        reg_event_show_readme_file();
        sdlx_register_control_events(EVID_SYMBOL, "Ticker",
                                     EVID_RELOAD, "R",
                                     EVID_QUIT, "X");
        sdlx_display_present();

        if (need_load) {
            int rc;
            need_load = false;
            rc = load_quote();
            if (rc == 0) {
                quote = scratch;
                have_quote = true;
                error_msg[0] = '\0';
                printf("I %s: %s price %0.4f points %d\n",
                       progname, quote.symbol, quote.price, quote.npoints);
            } else if (have_quote) {
                sdlx_show_toast("Update failed");
            }
            continue;
        }

        sdlx_get_event(-1, &event);
        if (event.event_id == -1) {
            continue;
        }
        process_event(&event);
        if (event.event_id == EVID_QUIT) {
            end_program = true;
        }
    }

    printf("I %s: terminating\n", progname);
    return 0;
}

void load_params(void)
{
    char *saved;

    saved = util_get_str_param(data_dir, "ticker", "AAPL");
    copy_str(ticker, saved, sizeof(ticker));
    free(saved);
    if (!sanitize_ticker(ticker)) {
        copy_str(ticker, "AAPL", sizeof(ticker));
    }

    duration = (int)util_get_numeric_param(data_dir, "duration", DUR_1D);
    if (duration < 0 || duration >= NUM_DUR) {
        duration = DUR_1D;
    }
}

// -----------------  LOAD QUOTE  ------------------------------------

int load_quote(void)
{
    char filename[64];
    int rc;

    chart_filename(filename, sizeof(filename));
    rc = download_chart(filename);
    if (rc != 0) {
        return -1;
    }
    rc = parse_chart_file(filename);
    if (rc != 0) {
        util_delete_file(data_dir, filename);
        return -1;
    }
    return 0;
}

int download_chart(char *filename)
{
    char url[300];
    char cmd[900];
    char enc[64];
    long tnow, mtime;
    int rc;

    if (!force_reload) {
        mtime = util_file_mtime(data_dir, filename);
        tnow = time(NULL);
        if (mtime != 0) {
            if ((tnow - mtime / 1000000) < CACHE_SECS) {
                printf("I %s: using cached %s\n", progname, filename);
                return 0;
            }
        }
    }
    force_reload = false;

    url_encode_symbol(ticker, enc, sizeof(enc));
    snprintf(url, sizeof(url),
             "https://query1.finance.yahoo.com/v8/finance/chart/%s?range=%s&interval=%s&includePrePost=false",
             enc, dur_range[duration], dur_interval[duration]);
    snprintf(cmd, sizeof(cmd),
             "curl --silent --max-time 30 --output \"%s/%s\" --header %s \"%s\"",
             data_dir, filename, HEADER, url);
    printf("I %s: %s\n", progname, cmd);
    rc = system(cmd);
    rc = WEXITSTATUS(rc);
    if (rc != 0) {
        printf("E %s: curl failed, rc=%d\n", progname, rc);
        set_error("Download failed");
        util_delete_file(data_dir, filename);
        return -1;
    }
    return 0;
}

int parse_chart_file(char *filename)
{
    char *str = NULL;
    void *root = NULL;
    void *result;
    json_value_t jv;
    double chg_pct = 0;
    bool has_pct = false;
    int ret = -1;

    memset(&scratch, 0, sizeof(scratch));
    scratch.price_hint = 2;

    str = util_read_file(data_dir, filename, NULL);
    if (str == NULL) {
        set_error("No data file");
        goto done;
    }

    root = util_json_parse(str, NULL);
    if (root == NULL) {
        set_error("Bad response");
        goto done;
    }

    jv = *util_json_get_value(root, "chart", "result", "0", NULL);
    if (jv.type != JSON_TYPE_OBJECT) {
        jv = *util_json_get_value(root, "chart", "error", "description", NULL);
        if (jv.type == JSON_TYPE_STRING) {
            set_error(jv.u.string);
        } else {
            set_error("Ticker not found");
        }
        goto done;
    }
    result = jv.u.object;

    if (read_meta(result, &chg_pct, &has_pct) != 0) {
        goto done;
    }
    if (read_points(result) != 0) {
        goto done;
    }
    if (finalize_quote(chg_pct, has_pct) != 0) {
        goto done;
    }

    ret = 0;

done:
    util_json_free(root);
    free(str);
    return ret;
}

int read_meta(void *result, double *chg_pct, bool *has_pct)
{
    json_value_t jv;

    jv = *util_json_get_value(result, "meta", "symbol", NULL);
    if (jv.type == JSON_TYPE_STRING) {
        copy_str(scratch.symbol, jv.u.string, sizeof(scratch.symbol));
    }
    if (scratch.symbol[0] == '\0') {
        copy_str(scratch.symbol, ticker, sizeof(scratch.symbol));
    }

    jv = *util_json_get_value(result, "meta", "longName", NULL);
    if (jv.type != JSON_TYPE_STRING) {
        jv = *util_json_get_value(result, "meta", "shortName", NULL);
    }
    if (jv.type == JSON_TYPE_STRING) {
        copy_str(scratch.name, jv.u.string, sizeof(scratch.name));
    }

    jv = *util_json_get_value(result, "meta", "currency", NULL);
    if (jv.type == JSON_TYPE_STRING) {
        copy_str(scratch.currency, jv.u.string, sizeof(scratch.currency));
    }

    jv = *util_json_get_value(result, "meta", "priceHint", NULL);
    if (jv.type == JSON_TYPE_NUMBER) {
        scratch.price_hint = (int)jv.u.number;
        if (scratch.price_hint < 0) {
            scratch.price_hint = 2;
        }
        if (scratch.price_hint > 6) {
            scratch.price_hint = 6;
        }
    }

    jv = *util_json_get_value(result, "meta", "regularMarketPrice", NULL);
    if (jv.type == JSON_TYPE_NUMBER) {
        scratch.price = jv.u.number;
        scratch.has_price = true;
    }

    jv = *util_json_get_value(result, "meta", "previousClose", NULL);
    if (jv.type == JSON_TYPE_NUMBER) {
        scratch.prev_close = jv.u.number;
        scratch.has_prev = true;
    }

    jv = *util_json_get_value(result, "meta", "regularMarketChangePercent", NULL);
    if (jv.type == JSON_TYPE_NUMBER) {
        *chg_pct = jv.u.number;
        *has_pct = true;
    }

    jv = *util_json_get_value(result, "meta", "regularMarketTime", NULL);
    if (jv.type == JSON_TYPE_NUMBER) {
        scratch.market_time = (long)jv.u.number;
    }

    return 0;
}

int count_timestamps(void *result)
{
    int n = 0;
    char idx[16];
    json_value_t jv;

    while (n < 4000) {
        snprintf(idx, sizeof(idx), "%d", n);
        jv = *util_json_get_value(result, "timestamp", idx, NULL);
        if (jv.type != JSON_TYPE_NUMBER) {
            break;
        }
        n++;
    }
    return n;
}

int read_points(void *result)
{
    json_value_t jv;
    bool use_adj = false;
    int count, stride, i, n;

    jv = *util_json_get_value(result, "indicators", "adjclose", NULL);
    if (jv.type == JSON_TYPE_ARRAY) {
        use_adj = true;
    }

    count = count_timestamps(result);
    stride = 1;
    if (count > MAX_POINTS) {
        stride = (count + MAX_POINTS - 2) / (MAX_POINTS - 1);
        if (stride < 1) {
            stride = 1;
        }
    }

    n = 0;
    for (i = 0; i < count; i++) {
        char idx[16];
        double close_v = 0;
        bool close_ok = false;
        bool take = false;
        long ts;

        if ((i % stride) == 0) {
            take = true;
        }
        if (i == count - 1) {
            take = true;
        }
        if (!take) {
            continue;
        }

        snprintf(idx, sizeof(idx), "%d", i);
        jv = *util_json_get_value(result, "timestamp", idx, NULL);
        if (jv.type != JSON_TYPE_NUMBER) {
            continue;
        }
        ts = (long)jv.u.number;

        jv = *util_json_get_value(result, "indicators", "quote", "0", "close", idx, NULL);
        if (jv.type == JSON_TYPE_NUMBER) {
            close_v = jv.u.number;
            close_ok = true;
        }
        if (use_adj) {
            jv = *util_json_get_value(result, "indicators", "adjclose", "0", "adjclose", idx, NULL);
            if (jv.type == JSON_TYPE_NUMBER) {
                close_v = jv.u.number;
                close_ok = true;
            }
        }

        if (n >= MAX_POINTS) {
            n = MAX_POINTS - 1;
        }
        scratch.pt_time[n] = ts;
        scratch.pt_valid[n] = close_ok;
        if (close_ok) {
            scratch.pt_price[n] = close_v;
        }
        n++;
    }
    scratch.npoints = n;
    return 0;
}

int finalize_quote(double chg_pct, bool has_pct)
{
    int i;

    if (!scratch.has_price) {
        for (i = scratch.npoints - 1; i >= 0; i--) {
            if (scratch.pt_valid[i]) {
                scratch.price = scratch.pt_price[i];
                scratch.has_price = true;
                if (scratch.market_time == 0) {
                    scratch.market_time = scratch.pt_time[i];
                }
                break;
            }
        }
    }

    if (!scratch.has_price) {
        set_error("No price");
        return -1;
    }

    if (!scratch.has_prev && has_pct) {
        if (chg_pct > -100.0) {
            scratch.prev_close = scratch.price / (1.0 + chg_pct / 100.0);
            scratch.has_prev = true;
        }
    }

    if (scratch.has_prev && scratch.prev_close > 0) {
        scratch.day_change = scratch.price - scratch.prev_close;
        scratch.day_pct = scratch.day_change / scratch.prev_close * 100.0;
        scratch.has_day = true;
    }

    if (scratch.market_time == 0 && scratch.npoints > 0) {
        scratch.market_time = scratch.pt_time[scratch.npoints - 1];
    }
    return 0;
}

// -----------------  DISPLAY  ---------------------------------------

void draw_screen(void)
{
    if (need_load) {
        draw_loading();
    } else if (have_quote) {
        draw_quote();
    } else {
        draw_error();
    }
}

void draw_loading(void)
{
    char msg[64];

    snprintf(msg, sizeof(msg), "Loading %s", ticker);
    sdlx_render_printf_ex(sdlx_win_width / 2, sdlx_win_height / 2,
                          FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR,
                          "%s", msg);
    draw_duration_buttons();
}

void draw_error(void)
{
    int y;
    int fontid;
    sdlx_loc_t *loc;
    sdlx_loc_t hit;
    char *msg;

    y = 80;
    fontid = font_for_text(ticker, 12, 18);
    loc = sdlx_render_printf_ex(sdlx_win_width / 2, y, fontid, COLOR_LIGHT_BLUE, FLAG_X_CTR,
                                "%s", ticker);
    hit = *loc;
    if (hit.w < 320) {
        hit.x = hit.x - (320 - hit.w) / 2;
        hit.w = 320;
    }
    if (hit.x < 0) {
        hit.x = 0;
    }
    hit.y = hit.y - 12;
    hit.h = hit.h + 24;
    sdlx_register_event(&hit, EVID_SYMBOL);

    y = y + sdlx_char_height(fontid) + 40;
    msg = error_msg;
    if (msg[0] == '\0') {
        msg = "No data";
    }
    sdlx_render_printf_ex(sdlx_win_width / 2, y, FONT_NORMAL, COLOR_RED, FLAG_X_CTR | 900,
                          "%s", msg);
    draw_duration_buttons();
}

void draw_quote(void)
{
    int y;
    int fontid;
    int name_lines;
    int per_line;
    sdlx_loc_t *loc;
    sdlx_loc_t hit;
    sdlx_color_t col = COLOR_WHITE;
    char price_s[48];
    char change_s[64];
    char asof[64];
    char range_s[32];
    bool have_range;

    y = 24;

    fontid = font_for_text(quote.symbol, 12, 18);
    loc = sdlx_render_printf_ex(sdlx_win_width / 2, y, fontid, COLOR_LIGHT_BLUE, FLAG_X_CTR,
                                "%s", quote.symbol);
    hit = *loc;
    if (hit.w < 320) {
        hit.x = hit.x - (320 - hit.w) / 2;
        hit.w = 320;
    }
    if (hit.x < 0) {
        hit.x = 0;
    }
    hit.y = hit.y - 8;
    hit.h = hit.h + 16;
    sdlx_register_event(&hit, EVID_SYMBOL);
    y = y + sdlx_char_height(fontid) + 6;

    if (quote.name[0] != '\0') {
        per_line = 27;
        name_lines = ((int)strlen(quote.name) + per_line - 1) / per_line;
        if (name_lines < 1) {
            name_lines = 1;
        }
        if (name_lines > 3) {
            name_lines = 3;
        }
        sdlx_render_printf_ex(sdlx_win_width / 2, y, FONT_SMALL, COLOR_LIGHT_GRAY,
                              FLAG_X_CTR | 920, "%s", quote.name);
        y = y + sdlx_char_height(FONT_SMALL) * name_lines + 16;
    }

    format_money(price_s, sizeof(price_s), quote.price, quote.price_hint, quote.currency);
    fontid = font_for_text(price_s, 8, 16);
    sdlx_render_printf_ex(sdlx_win_width / 2, y, fontid, COLOR_WHITE, FLAG_X_CTR, "%s", price_s);
    y = y + sdlx_char_height(fontid) + 8;

    if (quote.has_day) {
        format_change_line(change_s, sizeof(change_s), quote.day_change, quote.day_pct,
                           quote.price_hint, quote.currency);
        col = change_color(quote.day_change);
        fontid = FONT_NORMAL;
        if ((int)strlen(change_s) > 18) {
            fontid = FONT_SMALL;
        }
        sdlx_render_printf_ex(sdlx_win_width / 2, y, fontid, col, FLAG_X_CTR, "%s", change_s);
        y = y + sdlx_char_height(fontid) + 8;
    }

    have_range = false;
    range_s[0] = '\0';
    if (duration != DUR_1D) {
        double p0, p1, pct, mag;
        char sign;
        char num[32];
        if (series_ends(&p0, &p1)) {
            if (p0 > 0) {
                pct = (p1 - p0) / p0 * 100.0;
                sign = '+';
                mag = pct;
                if (pct < 0) {
                    sign = '-';
                    mag = -pct;
                }
                format_price(num, sizeof(num), mag, 2);
                snprintf(range_s, sizeof(range_s), "%s %c%s%%", dur_label[duration], sign, num);
                have_range = true;
                col = change_color(pct);
            }
        }
    }

    if (quote.market_time > 0) {
        format_asof(asof, sizeof(asof), quote.market_time);
        sdlx_render_printf_ex(20, y, FONT_TINY, COLOR_LIGHT_GRAY, FLAG_NONE, "%s", asof);
    }
    if (have_range) {
        draw_text_right(sdlx_win_width - 20, y, FONT_TINY, col, range_s);
    }
    if (quote.market_time > 0 || have_range) {
        y = y + sdlx_char_height(FONT_TINY) + 16;
    }

    draw_graph(y);
    draw_duration_buttons();
}

void draw_graph(int top)
{
    int bottom;
    int height;
    int graph_x, graph_w;
    int i;
    int label_chars;
    int cw, ch;
    double data_min, data_max, mid, pad, axis_min, axis_max, span;
    bool seen;
    bool flat;
    sdlx_color_t col;
    char label[32];
    int prev_x, prev_y;
    bool prev_ok;
    int last_x, last_y;
    bool have_last;
    int n;

    bottom = duration_button_y() - sdlx_char_height(FONT_TINY) - 20;
    height = bottom - top;
    if (height < 160) {
        return;
    }

    n = quote.npoints;
    seen = false;
    data_min = 0;
    data_max = 0;
    for (i = 0; i < n; i++) {
        if (!quote.pt_valid[i]) {
            continue;
        }
        if (!seen) {
            data_min = quote.pt_price[i];
            data_max = quote.pt_price[i];
            seen = true;
        } else {
            if (quote.pt_price[i] < data_min) {
                data_min = quote.pt_price[i];
            }
            if (quote.pt_price[i] > data_max) {
                data_max = quote.pt_price[i];
            }
        }
    }

    if (!seen) {
        sdlx_render_printf_ex(sdlx_win_width / 2, top + height / 2,
                              FONT_NORMAL, COLOR_LIGHT_GRAY, FLAG_XY_CTR,
                              "%s", "No history");
        return;
    }

    flat = false;
    span = data_max - data_min;
    if (span < 0.0000001) {
        flat = true;
        pad = data_max * 0.01;
        if (pad < 0.01) {
            pad = 0.01;
        }
    } else {
        pad = span * 0.08;
    }
    axis_min = data_min - pad;
    axis_max = data_max + pad;
    if (data_min > 0 && axis_min < 0) {
        axis_min = 0;
    }
    mid = (data_min + data_max) / 2.0;

    cw = sdlx_char_width(FONT_TINY);
    ch = sdlx_char_height(FONT_TINY);
    label_chars = 0;
    format_price(label, sizeof(label), data_max, quote.price_hint);
    if ((int)strlen(label) > label_chars) {
        label_chars = (int)strlen(label);
    }
    format_price(label, sizeof(label), data_min, quote.price_hint);
    if ((int)strlen(label) > label_chars) {
        label_chars = (int)strlen(label);
    }
    graph_x = 16 + label_chars * cw + 14;
    if (graph_x < 120) {
        graph_x = 120;
    }
    if (graph_x > 280) {
        graph_x = 280;
    }
    graph_w = sdlx_win_width - 24 - graph_x;
    if (graph_w < 200) {
        graph_w = 200;
    }

    sdlx_render_rect(graph_x, top, graph_w, height, 3, COLOR_GRAY);

    if (duration == DUR_1D && quote.has_day) {
        col = change_color(quote.day_change);
    } else {
        double p0, p1;
        col = COLOR_WHITE;
        if (series_ends(&p0, &p1)) {
            col = change_color(p1 - p0);
        }
    }

    if (!flat) {
        int gy;
        gy = price_to_y(data_max, axis_min, axis_max, top, height);
        sdlx_render_line(graph_x + 4, gy, graph_x + graph_w - 4, gy, COLOR_DARK_GRAY);
        format_price(label, sizeof(label), data_max, quote.price_hint);
        draw_text_right(graph_x - 10, gy - ch / 2, FONT_TINY, COLOR_LIGHT_GRAY, label);

        gy = price_to_y(mid, axis_min, axis_max, top, height);
        sdlx_render_line(graph_x + 4, gy, graph_x + graph_w - 4, gy, COLOR_DARK_GRAY);
        format_price(label, sizeof(label), mid, quote.price_hint);
        draw_text_right(graph_x - 10, gy - ch / 2, FONT_TINY, COLOR_LIGHT_GRAY, label);

        gy = price_to_y(data_min, axis_min, axis_max, top, height);
        sdlx_render_line(graph_x + 4, gy, graph_x + graph_w - 4, gy, COLOR_DARK_GRAY);
        format_price(label, sizeof(label), data_min, quote.price_hint);
        draw_text_right(graph_x - 10, gy - ch / 2, FONT_TINY, COLOR_LIGHT_GRAY, label);
    } else {
        int gy = price_to_y(data_max, axis_min, axis_max, top, height);
        sdlx_render_line(graph_x + 4, gy, graph_x + graph_w - 4, gy, COLOR_DARK_GRAY);
        format_price(label, sizeof(label), data_max, quote.price_hint);
        draw_text_right(graph_x - 10, gy - ch / 2, FONT_TINY, COLOR_LIGHT_GRAY, label);
    }

    prev_ok = false;
    prev_x = 0;
    prev_y = 0;
    have_last = false;
    last_x = 0;
    last_y = 0;
    for (i = 0; i < n; i++) {
        int x, y;
        if (!quote.pt_valid[i]) {
            prev_ok = false;
            continue;
        }
        x = index_to_x(i, n, graph_x, graph_w);
        y = price_to_y(quote.pt_price[i], axis_min, axis_max, top, height);
        if (prev_ok) {
            sdlx_render_line(prev_x, prev_y, x, y, col);
            sdlx_render_line(prev_x, prev_y + 1, x, y + 1, col);
        }
        if (n <= 60) {
            sdlx_render_point(x, y, col, 3);
        }
        prev_ok = true;
        prev_x = x;
        prev_y = y;
        last_x = x;
        last_y = y;
        have_last = true;
    }
    if (have_last) {
        sdlx_render_fill_circle(last_x, last_y, 8, col);
        sdlx_render_fill_circle(last_x, last_y, 3, COLOR_WHITE);
    }

    if (n >= 1) {
        if (quote.pt_time[0] > 0) {
            format_axis_time(label, sizeof(label), quote.pt_time[0]);
            sdlx_render_printf_ex(graph_x, bottom + 8, FONT_TINY, COLOR_LIGHT_GRAY, FLAG_NONE, "%s", label);
        }
    }
    if (n >= 3) {
        int mid_i = n / 2;
        int x;
        if (quote.pt_time[mid_i] > 0) {
            format_axis_time(label, sizeof(label), quote.pt_time[mid_i]);
            x = index_to_x(mid_i, n, graph_x, graph_w) - ((int)strlen(label) * cw) / 2;
            if (x < graph_x) {
                x = graph_x;
            }
            sdlx_render_printf_ex(x, bottom + 8, FONT_TINY, COLOR_LIGHT_GRAY, FLAG_NONE, "%s", label);
        }
    }
    if (n >= 2) {
        if (quote.pt_time[n - 1] > 0) {
            format_axis_time(label, sizeof(label), quote.pt_time[n - 1]);
            draw_text_right(graph_x + graph_w, bottom + 8, FONT_TINY, COLOR_LIGHT_GRAY, label);
        }
    }
}

int duration_button_y(void)
{
    return sdlx_win_height - BTN_H - BTN_GAP;
}

void draw_duration_buttons(void)
{
    int gap = 10;
    int btn_w;
    int btn_y;
    int i;

    btn_w = (sdlx_win_width - gap * (NUM_DUR + 1)) / NUM_DUR;
    btn_y = duration_button_y();

    for (i = 0; i < NUM_DUR; i++) {
        int x;
        sdlx_color_t bg, fg;

        x = gap + i * (btn_w + gap);
        if (i == duration) {
            bg = COLOR_TEAL;
            fg = COLOR_WHITE;
        } else {
            bg = COLOR_DARK_GRAY;
            fg = COLOR_LIGHT_BLUE;
        }
        reg_event_fill_rect(x, btn_y, btn_w, BTN_H, bg, EVID_DUR + i);
        sdlx_render_printf_ex(x + btn_w / 2, btn_y + BTN_H / 2,
                              FONT_SMALL, fg, FLAG_XY_CTR,
                              "%s", dur_label[i]);
    }
}

// -----------------  EVENTS  ----------------------------------------

void process_event(sdlx_event_t *event)
{
    if (event->event_id >= EVID_DUR && event->event_id < EVID_DUR + NUM_DUR) {
        select_duration(event->event_id - EVID_DUR);
        return;
    }

    switch (event->event_id) {
    case EVID_QUIT:
        break;
    case EVID_SHOW_README_FILE:
        show_file(data_dir, "README");
        break;
    case EVID_SYMBOL:
        enter_ticker();
        break;
    case EVID_RELOAD:
        force_reload = true;
        need_load = true;
        break;
    }
}

void select_duration(int d)
{
    if (d < 0 || d >= NUM_DUR) {
        return;
    }
    if (d == duration) {
        return;
    }
    duration = d;
    util_set_numeric_param(data_dir, "duration", duration);
    have_quote = false;
    need_load = true;
}

void enter_ticker(void)
{
    char *input;
    char tmp[100];

    input = sdlx_get_input_str("Ticker Symbol", false, ticker);
    if (input[0] == '\0') {
        sdlx_show_toast("Canceled");
        return;
    }
    copy_str(tmp, input, sizeof(tmp));
    if (!sanitize_ticker(tmp)) {
        sdlx_show_toast("Invalid ticker");
        return;
    }
    if (strcmp(tmp, ticker) == 0) {
        return;
    }
    copy_str(ticker, tmp, sizeof(ticker));
    util_set_str_param(data_dir, "ticker", ticker);
    have_quote = false;
    need_load = true;
}

// -----------------  UTILS  -----------------------------------------

void set_error(char *msg)
{
    copy_str(error_msg, msg, sizeof(error_msg));
    printf("E %s: %s\n", progname, error_msg);
}

void copy_str(char *dst, char *src, int dstsz)
{
    int i;

    if (dstsz <= 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    i = 0;
    while (i < dstsz - 1) {
        if (src[i] == '\0') {
            break;
        }
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

bool sanitize_ticker(char *s)
{
    int i, j;
    char c;

    j = 0;
    for (i = 0; s[i] != '\0'; i++) {
        c = s[i];
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '.' || c == '-' || c == '^' || c == '=') {
            if (j < MAX_SYMBOL - 1) {
                s[j] = c;
                j++;
            }
        }
    }
    s[j] = '\0';
    if (j == 0) {
        return false;
    }
    return true;
}

void url_encode_symbol(char *src, char *dst, int dstsz)
{
    int i, j;

    j = 0;
    for (i = 0; src[i] != '\0'; i++) {
        if (src[i] == '^') {
            if (j + 3 >= dstsz) {
                break;
            }
            dst[j] = '%';
            j++;
            dst[j] = '5';
            j++;
            dst[j] = 'E';
            j++;
        } else {
            if (j + 1 >= dstsz) {
                break;
            }
            dst[j] = src[i];
            j++;
        }
    }
    dst[j] = '\0';
}

void chart_filename(char *buf, int buflen)
{
    snprintf(buf, buflen, "%s_%s.json", ticker, dur_range[duration]);
}

int font_for_text(char *s, int min_id, int max_id)
{
    int n;
    int id;

    n = (int)strlen(s);
    if (n < 1) {
        n = 1;
    }
    id = n + 2;
    if (id < min_id) {
        id = min_id;
    }
    if (id > max_id) {
        id = max_id;
    }
    return id;
}

sdlx_color_t change_color(double v)
{
    if (v > 0) {
        return COLOR_GREEN;
    }
    if (v < 0) {
        return COLOR_RED;
    }
    return COLOR_WHITE;
}

bool series_ends(double *p0, double *p1)
{
    int i;
    int a = -1;
    int b = -1;

    for (i = 0; i < quote.npoints; i++) {
        if (!quote.pt_valid[i]) {
            continue;
        }
        if (a < 0) {
            a = i;
        }
        b = i;
    }
    if (a < 0) {
        return false;
    }
    *p0 = quote.pt_price[a];
    *p1 = quote.pt_price[b];
    return true;
}

void format_price(char *buf, int buflen, double value, int hint)
{
    if (hint <= 0) {
        snprintf(buf, buflen, "%0.0f", value);
    } else if (hint == 1) {
        snprintf(buf, buflen, "%0.1f", value);
    } else if (hint == 2) {
        snprintf(buf, buflen, "%0.2f", value);
    } else if (hint == 3) {
        snprintf(buf, buflen, "%0.3f", value);
    } else if (hint == 4) {
        snprintf(buf, buflen, "%0.4f", value);
    } else if (hint == 5) {
        snprintf(buf, buflen, "%0.5f", value);
    } else {
        snprintf(buf, buflen, "%0.6f", value);
    }
}

void format_money(char *buf, int buflen, double value, int hint, char *currency)
{
    char num[32];
    double mag;

    mag = value;
    if (mag < 0) {
        mag = -mag;
    }
    format_price(num, sizeof(num), mag, hint);
    if (strcmp(currency, "USD") == 0) {
        if (value < 0) {
            snprintf(buf, buflen, "-$%s", num);
        } else {
            snprintf(buf, buflen, "$%s", num);
        }
    } else if (currency[0] != '\0') {
        if (value < 0) {
            snprintf(buf, buflen, "-%s %s", num, currency);
        } else {
            snprintf(buf, buflen, "%s %s", num, currency);
        }
    } else if (value < 0) {
        snprintf(buf, buflen, "-%s", num);
    } else {
        snprintf(buf, buflen, "%s", num);
    }
}

void format_change_line(char *buf, int buflen, double change, double pct, int hint, char *currency)
{
    char cnum[40];
    char pnum[32];
    char cs, ps;
    double cm, pm;

    cs = '+';
    cm = change;
    if (change < 0) {
        cs = '-';
        cm = -change;
    }
    ps = '+';
    pm = pct;
    if (pct < 0) {
        ps = '-';
        pm = -pct;
    }
    format_money(cnum, sizeof(cnum), cm, hint, currency);
    format_price(pnum, sizeof(pnum), pm, 2);
    snprintf(buf, buflen, "%c%s  (%c%s%%)", cs, cnum, ps, pnum);
}

void format_clock(struct tm *tm, char *buf, int buflen)
{
    int hour;
    char *ampm;

    hour = tm->tm_hour;
    ampm = "AM";
    if (hour >= 12) {
        ampm = "PM";
    }
    if (hour > 12) {
        hour = hour - 12;
    }
    if (hour == 0) {
        hour = 12;
    }
    snprintf(buf, buflen, "%d:%02d %s", hour, tm->tm_min, ampm);
}

void format_asof(char *buf, int buflen, long unix_t)
{
    struct tm tm;
    time_t t;
    char clock[20];

    t = (time_t)unix_t;
    localtime_r(&t, &tm);
    format_clock(&tm, clock, sizeof(clock));
    snprintf(buf, buflen, "As of %s %d/%d %s",
             dow_str(tm.tm_wday), tm.tm_mon + 1, tm.tm_mday, clock);
}

void format_axis_time(char *buf, int buflen, long unix_t)
{
    struct tm tm;
    time_t t;
    char clock[20];

    t = (time_t)unix_t;
    localtime_r(&t, &tm);
    if (duration == DUR_1D) {
        format_clock(&tm, clock, sizeof(clock));
        snprintf(buf, buflen, "%s", clock);
    } else if (duration == DUR_5D || duration == DUR_1M) {
        snprintf(buf, buflen, "%d/%d", tm.tm_mon + 1, tm.tm_mday);
    } else {
        snprintf(buf, buflen, "%s %d", get_month_str(tm.tm_mon + 1), tm.tm_year + 1900);
    }
}

char *dow_str(int w)
{
    if (w < 0 || w > 6) {
        return "";
    }
    return dow_name[w];
}

int price_to_y(double price, double axis_min, double axis_max, int top, int height)
{
    double span;
    double frac;
    int y;

    span = axis_max - axis_min;
    if (span <= 0) {
        span = 1;
    }
    frac = (price - axis_min) / span;
    y = top + (height - 1) - (int)(frac * (double)(height - 1));
    if (y < top) {
        y = top;
    }
    if (y > top + height - 1) {
        y = top + height - 1;
    }
    return y;
}

int index_to_x(int i, int n, int left, int width)
{
    if (n <= 1) {
        return left + width / 2;
    }
    return left + (int)((double)i / (double)(n - 1) * (double)(width - 1));
}

void draw_text_right(int right_x, int y, int fontid, sdlx_color_t color, char *s)
{
    int x;

    x = right_x - (int)strlen(s) * sdlx_char_width(fontid);
    if (x < 0) {
        x = 0;
    }
    sdlx_render_printf_ex(x, y, fontid, color, FLAG_NONE, "%s", s);
}
