#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/ColrOrgn/common.h"
#include "lib/lib.h"

//
// defines
//

// bands
#define LOW_BAND    0   // red
#define MID_BAND    1   // green
#define HIGH_BAND   2   // blue

// events
#define EVID_FILTER_SLCT           1001
#define EVID_COLOR_ORGAN_SLCT      1002
#define EVID_LOW_BAND_INCREASE     1010
#define EVID_LOW_BAND_DECREASE     1011
#define EVID_MID_BAND_INCREASE     1012
#define EVID_MID_BAND_DECREASE     1013
#define EVID_HIGH_BAND_INCREASE    1014
#define EVID_HIGH_BAND_DECREASE    1015
#define EVID_FFT_INCREASE          1016
#define EVID_FFT_DECREASE          1017

// filters used to reduce visual jitter
#define MAX_FILTER         3
#define FILTER_NONE        0
#define FILTER_EXP_SMOOTH  1
#define FILTER_SNAP        2

// color organ display formats
#define MAX_COLOR_ORGAN      3
#define COLOR_ORGAN_BARS     0
#define COLOR_ORGAN_CIRCLES  1
#define COLOR_ORGAN_FFT      2

// misc
#define DISP_SCALE_FACTOR_DURATION 50

// default param values
#define DFLT_COLOR_ORGAN       COLOR_ORGAN_BARS
#define DFLT_FILTER            FILTER_SNAP
#define DFLT_LOW_BAND_SCALE    10
#define DFLT_MID_BAND_SCALE    15
#define DFLT_HIGH_BAND_SCALE   25
#define DFLT_FFT_SCALE         10
#define DFLT_EXP_FLTR_K      0.6
#define DFLT_SNAP_FLTR_K     0.08

//
// variables
//

// params
int   which_color_organ;
int   which_filter;
int   band_scale[3];
int   fft_scale;
float exp_fltr_k;
float snap_fltr_decay;

// color organ band freq ranges
int band_start_freq[3] = {  60, 200,  800 };
int band_end_freq[3]   = { 150, 600, 2200 };

// names
char *color_organ_name[MAX_COLOR_ORGAN] = {"BARS", "CIRC", "FFT"};
char *filter_name[MAX_FILTER] = {"NONE", "EXP", "SNAP"};

// circles used in COLOR_ORGAN_CIRCLES mode
sdlx_texture_t *red_circle_texture;
sdlx_texture_t *green_circle_texture;
sdlx_texture_t *blue_circle_texture;

// used to display band scaling values for a short time interval
int disp_scale_factor;

//
// prototypes
//

void settings_init(void);

// -----------------  INIT & CLEANUP  --------------------------------

sdlx_texture_t *create_circle_texture(sdlx_color_t color);

void color_organ_init(void)
{
    // initialize settings from params file
    settings_init();

    // create textures used by COLOR_ORGAN_CIRCLES
    red_circle_texture   = create_circle_texture(COLOR_RED);
    green_circle_texture = create_circle_texture(COLOR_GREEN);
    blue_circle_texture  = create_circle_texture(COLOR_BLUE);
}

void color_organ_cleanup(void)
{
    // destroy textures
    sdlx_destroy_texture(red_circle_texture);
    sdlx_destroy_texture(green_circle_texture);
    sdlx_destroy_texture(blue_circle_texture);
}

sdlx_texture_t *create_circle_texture(sdlx_color_t color)
{
    sdlx_texture_t *t;

    t = sdlx_create_texture(200,200);
    sdlx_set_render_target(t);
    sdlx_render_fill_circle(100, 100, 100, color);
    sdlx_set_render_target(NULL);

    return t;
}

// -----------------  COLOR ORGAN DISPLAY  ---------------------------

#define MAX_SAMPLES 600
#define MAX_FFT     301
#define DELTA_F     20.0

void color_organ_display_bars(float *fft);
void color_organ_display_circles(float *fft);
void color_organ_display_fft(float *fft);
void filter(float *val, float new_val);
void init_loc(sdlx_loc_t *loc, int x, int y, int w, int h);

void color_organ_display(void)
{
    int          num_downsample = 4;
    int          num_samples = nearbyint(FRAMES_PER_SEC / num_downsample * 0.050);  // equals 600
    float        delta_f = ((double)FRAMES_PER_SEC/num_downsample) / num_samples;   // equals 20
    float        samples[MAX_SAMPLES], fft[MAX_FFT];

    // sanity check
    if (num_samples != MAX_SAMPLES) {
        printf("E %s: BUG num_samples=%d should be %d\n", progname, num_samples, MAX_SAMPLES);
        return;
    }
    if (delta_f != DELTA_F) {
        printf("E %s: BUG delta_f=%f should be %f\n", progname, delta_f, DELTA_F);
        return;
    }

    // get audio samples and perform fft
    sdlx_get_audio_samples(num_samples, num_downsample, GET_SAMPLES_MONO, samples);
    util_fft_real_to_real(num_samples, samples, fft, true);

    // display color organ in format: COLOR_ORGAN_BARS and COLOR_ORGAN_CIRCLES
    if (which_color_organ == COLOR_ORGAN_BARS) {
        color_organ_display_bars(fft);
    } else if (which_color_organ == COLOR_ORGAN_CIRCLES) {
        color_organ_display_circles(fft);
    } else if (which_color_organ == COLOR_ORGAN_FFT) {
        color_organ_display_fft(fft);
    }

    // stop displaying the scale values after a short time interval
    if (disp_scale_factor) {
        disp_scale_factor--;
    }

    // register events to select the color organ display format and filter 
    if (show_controls) {
        if (orientation == PORTRAIT) {
            int y_controls = COH_P + LINE_SPACING;
            reg_event(COL2X(11), y_controls, COLOR_LIGHT_BLUE,
                    color_organ_name[which_color_organ], EVID_COLOR_ORGAN_SLCT);
            reg_event(COL2X(16), y_controls, COLOR_LIGHT_BLUE, 
                    filter_name[which_filter], EVID_FILTER_SLCT);
        } else {
            int x_controls = sdlx_win_width - 8*sdlx_char_width_dflt;
            int y_controls = 3 * LINE_SPACING;
            reg_event(x_controls, y_controls, COLOR_LIGHT_BLUE,
                    color_organ_name[which_color_organ], EVID_COLOR_ORGAN_SLCT);
            y_controls += LINE_SPACING;
            reg_event(x_controls, y_controls, COLOR_LIGHT_BLUE, 
                    filter_name[which_filter], EVID_FILTER_SLCT);
        }
    }
}

void color_organ_display_bars(float *fft)
{
    int   first_bin, last_bin;
    float raw_new_vol;
    int   x, y, w, h, cow, coh;
    sdlx_color_t color;
    sdlx_loc_t loc;

    static float filtered_vol[3];

    // loop over the 3 bands
    for (int band = 0; band < 3; band++) {
        // determine raw_new_vol for this band
        first_bin = nearbyint(band_start_freq[band] / DELTA_F);
        last_bin = nearbyint(band_end_freq[band] / DELTA_F);
        raw_new_vol = util_rms_float(&fft[first_bin], last_bin-first_bin+1);

        // apply scale factor
        raw_new_vol *= band_scale[band];
        if (raw_new_vol > 1) raw_new_vol = 1;

        // apply filter
        filter(&filtered_vol[band], raw_new_vol);

        // display the band volume
        cow = (orientation == PORTRAIT ? COW_P : COW_L);
        coh = (orientation == PORTRAIT ? COH_P : COH_L);

        w = cow / 3;;
        h = coh * filtered_vol[band];
        x = band * w;
        y = coh - h;
        if (orientation == LANDSCAPE) {
            x += (sdlx_win_width - COW_L) / 2;
        }

        color = (band == LOW_BAND ? COLOR_RED : (band == MID_BAND ? COLOR_GREEN : COLOR_BLUE));
        sdlx_render_fill_rect(x, y, w, h, color);

        // register events to adjust scale factor
        init_loc(&loc, x, 0, w, coh/2);
        sdlx_register_event(&loc, band == 0 ? EVID_LOW_BAND_INCREASE : 
                                 (band == 1 ? EVID_MID_BAND_INCREASE : 
                                              EVID_HIGH_BAND_INCREASE));
        loc.y += coh/2;
        sdlx_register_event(&loc, band == 0 ? EVID_LOW_BAND_DECREASE : 
                                 (band == 1 ? EVID_MID_BAND_DECREASE : 
                                              EVID_HIGH_BAND_DECREASE));

        // display scale factor
        if (disp_scale_factor) {
            sdlx_render_printf_ex2(x+w/2, coh/2,
                                   FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%d", band_scale[band]);
        }
    }
}

void color_organ_display_circles(float *fft)
{
    int          first_bin, last_bin;
    float        raw_new_vol;
    static float filtered_vol[3];
    sdlx_loc_t   dest;

    // loop over the 3 bands
    for (int band = 0; band < 3; band++) {
        // determine raw_new_vol for this band
        first_bin = nearbyint(band_start_freq[band] / DELTA_F);
        last_bin = nearbyint(band_end_freq[band] / DELTA_F);
        raw_new_vol = util_rms_float(&fft[first_bin], last_bin-first_bin+1);

        // apply scale factor
        raw_new_vol *= band_scale[band];
        if (raw_new_vol > 1) raw_new_vol = 1;

        // apply filter
        filter(&filtered_vol[band], raw_new_vol);

        // display the band volume by varying the 
        // brightness of the red/green/blue circle
        sdlx_texture_t *t;
        sdlx_loc_t      loc;
        int             cow, coh, win_ctr, radius, x_ctr, y_ctr;
        float           k, band_volume;

        cow     = (orientation == PORTRAIT ? COW_P : COW_L);
        coh     = (orientation == PORTRAIT ? COH_P : COH_L);
        win_ctr = sdlx_win_width / 2;
        radius  = coh / (2 + sqrt(3));
        k       = 2.0;  // determined emppirically for good visual

        if (band == LOW_BAND) {
            t = red_circle_texture;
            x_ctr = win_ctr - radius;
            y_ctr = coh - radius;
        } else if (band == MID_BAND) {
            t = green_circle_texture;
            x_ctr = win_ctr + radius;
            y_ctr = coh - radius;
        } else {
            t = blue_circle_texture;
            x_ctr = win_ctr;
            y_ctr = radius;
        }

        band_volume = k * filtered_vol[band];
        if (band_volume > 1) {
            band_volume = 1;
        }
        sdlx_color_mod_texture(t, band_volume, band_volume, band_volume);

        dest.x = x_ctr-radius;
        dest.y = y_ctr-radius;
        dest.w = 2*radius;
        dest.h = 2*radius;
        sdlx_render_texture(t, NULL, &dest);

        // register events to adjust scale factor
        init_loc(&loc, x_ctr - radius/2, y_ctr-radius, radius, radius);
        sdlx_register_event(&loc, band == 0 ? EVID_LOW_BAND_INCREASE : 
                                 (band == 1 ? EVID_MID_BAND_INCREASE : 
                                              EVID_HIGH_BAND_INCREASE));
        loc.y += radius;
        sdlx_register_event(&loc, band == 0 ? EVID_LOW_BAND_DECREASE : 
                                 (band == 1 ? EVID_MID_BAND_DECREASE : 
                                              EVID_HIGH_BAND_DECREASE));

        // display scale factor
        if (disp_scale_factor) {
            sdlx_render_printf_ex2(x_ctr, y_ctr,
                                   FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%d", band_scale[band]);
        }
    }
}

void color_organ_display_fft(float *fft)
{
    int          x, y, w, h, wavelength;
    sdlx_color_t color;
    float        raw_scaled;
    sdlx_loc_t   loc;
    int          cow, coh;
    static float filtered[MAX_FFT];

    // visible light range is 380 (violet) to 750 (red) nm;
    // wavelength variable range is from 699 (red) down to 400 (violet)

    // loop over the fft output array;
    // display each fft bin value as a rectangle, w=3, and 
    // h indicating the fft bin value
    for (int i = 1; i < MAX_FFT; i++) {
        raw_scaled = fft[i] * fft_scale; 
        if (raw_scaled > 1) raw_scaled = 1;

        filter(&filtered[i], raw_scaled);

        if (orientation == PORTRAIT) {
            w = 3;
            h = COH_P * filtered[i];
            x = 3*(i-1) + 50;
            y = COH_P - h;
        } else {
            w = 4;
            h = COH_L * filtered[i];
            x = 4*(i-1) + (sdlx_win_width - COW_L) / 2;
            y = COH_L - h;
        }

        wavelength = 700 - i;
        color = sdlx_wavelength_to_color(wavelength);
        sdlx_render_fill_rect(x, y, w, h, color);
    }

    // register events to adjust scale factor
    cow = (orientation == PORTRAIT ? COW_P : COW_L);
    coh = (orientation == PORTRAIT ? COH_P : COH_L);
    x = (orientation == PORTRAIT ? 0 : ((sdlx_win_width - COW_L) / 2));
    init_loc(&loc, x, 0, cow, coh/2);
    sdlx_register_event(&loc, EVID_FFT_INCREASE);
    loc.y += coh/2;
    sdlx_register_event(&loc, EVID_FFT_DECREASE);

    // display scale factor
    if (disp_scale_factor) {
        sdlx_render_printf_ex2(x+cow/2, coh/2,
                               FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, 
                               "%d", fft_scale);
    }
}

void filter(float *val, float raw_new_val)
{
    if (which_filter == FILTER_NONE) {
        *val = raw_new_val;
    } else if (which_filter == FILTER_EXP_SMOOTH) {
        *val = *val + exp_fltr_k * (raw_new_val - *val);
    } else if (which_filter == FILTER_SNAP) {
        if (raw_new_val > *val) {
            *val = raw_new_val;
        } else {
            *val -= snap_fltr_decay;
            if (*val < raw_new_val) *val = raw_new_val;
        }
    } else {
        *val = 0;
    }

    if (*val < 0) *val = 0;
    if (*val > 1) *val = 1;
}

void init_loc(sdlx_loc_t *loc, int x, int y, int w, int h)
{
    loc->x = x;
    loc->y = y;
    loc->w = w;
    loc->h = h;
}

// -----------------  COLOR ORGAN EVENT HANDLING  --------------------

void adjust_scale(int *scale, bool increase);

void color_organ_process_event(sdlx_event_t *ev)
{
    switch (ev->event_id) {
    case EVID_FILTER_SLCT:
        which_filter = (which_filter + 1) % MAX_FILTER;
        util_set_numeric_param(data_dir, "filter", which_filter);
        break;
    case EVID_COLOR_ORGAN_SLCT:
        which_color_organ = (which_color_organ + 1) % MAX_COLOR_ORGAN;
        util_set_numeric_param(data_dir, "color_organ", which_color_organ);
        break;
    case EVID_LOW_BAND_INCREASE:
    case EVID_LOW_BAND_DECREASE:
        if (disp_scale_factor == 0) {
            disp_scale_factor = DISP_SCALE_FACTOR_DURATION;
            break;
        }
        adjust_scale(&band_scale[LOW_BAND], ev->event_id==EVID_LOW_BAND_INCREASE);
        disp_scale_factor = DISP_SCALE_FACTOR_DURATION;
        util_set_numeric_param(data_dir, "low_band_scale", band_scale[LOW_BAND]);
        break;
    case EVID_MID_BAND_INCREASE:
    case EVID_MID_BAND_DECREASE:
        if (disp_scale_factor == 0) {
            disp_scale_factor = DISP_SCALE_FACTOR_DURATION;
            break;
        }
        adjust_scale(&band_scale[MID_BAND], ev->event_id==EVID_MID_BAND_INCREASE);
        disp_scale_factor = DISP_SCALE_FACTOR_DURATION;
        util_set_numeric_param(data_dir, "mid_band_scale", band_scale[MID_BAND]);
        break;
    case EVID_HIGH_BAND_INCREASE:
    case EVID_HIGH_BAND_DECREASE:
        if (disp_scale_factor == 0) {
            disp_scale_factor = DISP_SCALE_FACTOR_DURATION;
            break;
        }
        adjust_scale(&band_scale[HIGH_BAND], ev->event_id==EVID_HIGH_BAND_INCREASE);
        disp_scale_factor = DISP_SCALE_FACTOR_DURATION;
        util_set_numeric_param(data_dir, "high_band_scale", band_scale[HIGH_BAND]);
        break;
    case EVID_FFT_INCREASE:
    case EVID_FFT_DECREASE:
        if (disp_scale_factor == 0) {
            disp_scale_factor = DISP_SCALE_FACTOR_DURATION;
            break;
        }
        adjust_scale(&fft_scale, ev->event_id==EVID_FFT_INCREASE);
        disp_scale_factor = DISP_SCALE_FACTOR_DURATION;
        util_set_numeric_param(data_dir, "fft_scale", fft_scale);
        break;
    }
}

void adjust_scale(int *scale, bool increase)
{
    if (increase) {
        if (*scale < 5) {
            *scale += 1;
        } else {
            *scale += 5;
        }
    } else {
        if (*scale == 1) {
            // do nothing
        } else if (*scale <= 5) {
            *scale -= 1;
        } else {
            *scale -= 5;
        }
    }
}            

// -----------------  COLOR ORGAN SETTINGS  --------------------------

void settings_init(void)
{
    band_scale[LOW_BAND]  = util_get_numeric_param(data_dir, "low_band_scale",    DFLT_LOW_BAND_SCALE);
    band_scale[MID_BAND]  = util_get_numeric_param(data_dir, "mid_band_scale",    DFLT_MID_BAND_SCALE);
    band_scale[HIGH_BAND] = util_get_numeric_param(data_dir, "high_band_scale",   DFLT_HIGH_BAND_SCALE);
    fft_scale             = util_get_numeric_param(data_dir, "fft_scale",         DFLT_FFT_SCALE);
    which_color_organ     = util_get_numeric_param(data_dir, "color_organ",       DFLT_COLOR_ORGAN);
    which_filter          = util_get_numeric_param(data_dir, "filter",            DFLT_FILTER);
    exp_fltr_k            = util_get_numeric_param(data_dir, "exp_fltr_k",        DFLT_EXP_FLTR_K);
    snap_fltr_decay       = util_get_numeric_param(data_dir, "snap_fltr_decay",   DFLT_SNAP_FLTR_K);
    debug_flags           = util_get_numeric_param(data_dir, "debug_flags",       0);
}

void settings_reset_to_dflt(void)
{
    util_set_numeric_param(data_dir, "low_band_scale",    DFLT_LOW_BAND_SCALE);
    util_set_numeric_param(data_dir, "mid_band_scale",    DFLT_MID_BAND_SCALE);
    util_set_numeric_param(data_dir, "high_band_scale",   DFLT_HIGH_BAND_SCALE);
    util_set_numeric_param(data_dir, "fft_scale",         DFLT_FFT_SCALE);
    util_set_numeric_param(data_dir, "color_organ",       DFLT_COLOR_ORGAN);
    util_set_numeric_param(data_dir, "filter",            DFLT_FILTER);
    util_set_numeric_param(data_dir, "exp_fltr_k",        DFLT_EXP_FLTR_K);
    util_set_numeric_param(data_dir, "snap_fltr_decay",   DFLT_SNAP_FLTR_K);
    util_set_numeric_param(data_dir, "debug_flags",       0);

    settings_init();
}

#define EVID_SETTINGS_RESET             2001
#define EVID_SETTINGS_CREATE_TEST_FILES 2002
#define EVID_SETTINGS_EXP_FLTR_K        2003
#define EVID_SETTINGS_SNAP_FLTR_DECAY   2004
#define EVID_SETTINGS_DEBUG_FLAGS       2005

void color_organ_settings(void)
{
    bool         done = false;
    sdlx_event_t event;
    int          y;
    sdlx_loc_t  *loc;
    char        *str;

    while (!done) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // display settings
        sdlx_print_set_default(24, COLOR_WHITE);
        y = 0;
        sdlx_render_printf(0, y, "%-15s = %d",    "low_band_scale",   band_scale[LOW_BAND]);
        y += sdlx_char_height_dflt;
        sdlx_render_printf(0, y, "%-15s = %d",    "mid_band_scale",   band_scale[MID_BAND]);
        y += sdlx_char_height_dflt;
        sdlx_render_printf(0, y, "%-15s = %d",    "high_band_scale",  band_scale[HIGH_BAND]);
        y += sdlx_char_height_dflt;
        sdlx_render_printf(0, y, "%-15s = %d",    "fft_scale",        fft_scale);
        y += sdlx_char_height_dflt;
        sdlx_render_printf(0, y, "%-15s = %s",    "color_organ",      color_organ_name[which_color_organ]);
        y += sdlx_char_height_dflt;
        sdlx_render_printf(0, y, "%-15s = %s",    "filter",           filter_name[which_filter]);
        y += sdlx_char_height_dflt;
        sdlx_render_printf(0, y, "%-15s = %0.3f", "exp_fltr_k",       exp_fltr_k);
        y += sdlx_char_height_dflt;
        sdlx_render_printf(0, y, "%-15s = %0.3f", "snap_fltr_decay",  snap_fltr_decay);
        y += sdlx_char_height_dflt;
        sdlx_render_printf(0, y, "%-15s = %04x",  "debug_flags",      debug_flags);
        sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);
        y += 2*sdlx_char_height_dflt;

        // register events
        loc = sdlx_render_printf_ex1(0, y, FONT_NORMAL, COLOR_LIGHT_BLUE, "RESET");
        sdlx_register_event(loc, EVID_SETTINGS_RESET);
        y += 2*sdlx_char_height_dflt;

        loc = sdlx_render_printf_ex1(0, y, FONT_NORMAL, COLOR_LIGHT_BLUE, "CREATE_TEST_FILES");
        sdlx_register_event(loc, EVID_SETTINGS_CREATE_TEST_FILES);
        y += 2*sdlx_char_height_dflt;

        loc = sdlx_render_printf_ex1(0, y, FONT_NORMAL, COLOR_LIGHT_BLUE, "EXP_FLTR_K");
        sdlx_register_event(loc, EVID_SETTINGS_EXP_FLTR_K);
        y += 2*sdlx_char_height_dflt;

        loc = sdlx_render_printf_ex1(0, y, FONT_NORMAL, COLOR_LIGHT_BLUE, "SNAP_FLTR_DECAY");
        sdlx_register_event(loc, EVID_SETTINGS_SNAP_FLTR_DECAY);
        y += 2*sdlx_char_height_dflt;

        loc = sdlx_render_printf_ex1(0, y, FONT_NORMAL, COLOR_LIGHT_BLUE, "DEBUG_FLAGS");
        sdlx_register_event(loc, EVID_SETTINGS_DEBUG_FLAGS);
        y += 2*sdlx_char_height_dflt;

        // register events
        sdlx_register_control_events(0, NULL,
                                     0, NULL,
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with infinite timeout
        sdlx_get_event(-1, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process event
        switch (event.event_id) {
        case EVID_SETTINGS_RESET:
            printf("I %s: resetting settings to default\n", progname);
            settings_reset_to_dflt();
            break;
        case EVID_SETTINGS_CREATE_TEST_FILES:
            printf("I %s: creating test files\n", progname);

            // create test files for the color organ bands
            sdlx_create_test_file(files_dir, "test_low.mp3",
                                  band_start_freq[LOW_BAND], band_end_freq[LOW_BAND], 10);
            sdlx_create_test_file(files_dir, "test_mid.mp3",
                                  band_start_freq[MID_BAND], band_end_freq[MID_BAND], 10);
            sdlx_create_test_file(files_dir, "test_high.mp3",
                                  band_start_freq[HIGH_BAND], band_end_freq[HIGH_BAND], 10);
            sdlx_create_test_file(files_dir, "test_bands.mp3",
                                  band_start_freq[LOW_BAND], band_end_freq[HIGH_BAND], 10);

            // create test file for the fft span
            sdlx_create_test_file(files_dir, "test_span.mp3", 100, 6000, 10);
            break;
        case EVID_SETTINGS_EXP_FLTR_K:
            str = sdlx_get_input_str("exp_fltr_k", true, NULL);
            if (sscanf(str, "%f", &exp_fltr_k) != 1) break;
            util_set_numeric_param(data_dir, "exp_fltr_k", exp_fltr_k);
            break;
        case EVID_SETTINGS_SNAP_FLTR_DECAY:
            str = sdlx_get_input_str("snap_fltr_decay", true, NULL);
            if (sscanf(str, "%f", &snap_fltr_decay) != 1) break;
            util_set_numeric_param(data_dir, "snap_fltr_decay", snap_fltr_decay);
            break;
        case EVID_SETTINGS_DEBUG_FLAGS:
            str = sdlx_get_input_str("debug_flags", true, NULL);
            if (sscanf(str, "%x", &debug_flags) != 1) break;
            util_set_numeric_param(data_dir, "debug_flags", debug_flags);
            break;
        case EVID_QUIT:
            done = true;
            break;
        }
    }
}

