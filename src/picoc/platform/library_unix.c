#include "../interpreter.h"

#include <sdlx.h>
#include <utils.h>
#include <svcs.h>
#include <private.h>

struct StdVararg
{
    struct Value **Param;
    int NumArgs;
};

int StdioBasePrintf(struct ParseState *Parser, FILE *Stream, char *StrOut,
    int StrOutLen, char *Format, struct StdVararg *Args);

// -----------------  SDL PLATFORM ROUTINES  ----------------------------

//
// init & quit
//

void Sdlx_init(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int subsys = (int)Param[0]->Val->Integer;

    int retval;
    retval = sdlx_init(subsys);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_quit(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int subsys = (int)Param[0]->Val->Integer;

    sdlx_quit(subsys);
}

//
// video - display init and present
//

void Sdlx_display_init(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_color_t color = (sdlx_color_t)Param[0]->Val->UnsignedInteger;
    int orientation    = Param[1]->Val->Integer;

    sdlx_display_init(color, orientation);
}

void Sdlx_display_present(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_display_present();
}

//
// video - colors
//

void Sdlx_create_color(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int r = (int)Param[0]->Val->Integer;
    int g = (int)Param[1]->Val->Integer;
    int b = (int)Param[2]->Val->Integer;
    int a = (int)Param[3]->Val->Integer;

    sdlx_color_t retval;
    retval = sdlx_create_color(r, g, b, a);
    ReturnValue->Val->UnsignedInteger = retval;
}

void Sdlx_scale_color(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_color_t color = (sdlx_color_t)Param[0]->Val->UnsignedInteger;
    double       inten = (double)Param[1]->Val->FP;

    sdlx_color_t retval;
    retval = sdlx_scale_color(color, inten);
    ReturnValue->Val->UnsignedInteger = retval;
}

void Sdlx_set_color_alpha(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_color_t color = (sdlx_color_t)Param[0]->Val->UnsignedInteger;
    int          alpha = (int)Param[1]->Val->Integer;

    sdlx_color_t retval;
    retval = sdlx_set_color_alpha(color, alpha);
    ReturnValue->Val->UnsignedInteger = retval;
}

void Sdlx_wavelength_to_color(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int wavelength = (int)Param[0]->Val->Integer;

    sdlx_color_t retval;
    retval = sdlx_wavelength_to_color(wavelength);
    ReturnValue->Val->UnsignedInteger = retval;
}

//
// video - render text
//

void Sdlx_print_set_default(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int          fontid = (int)Param[0]->Val->Integer;
    sdlx_color_t color  = (sdlx_color_t)Param[1]->Val->UnsignedInteger;

    sdlx_print_set_default(fontid, color);
}

void Sdlx_render_printf(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int    x   = (int)Param[0]->Val->Integer;
    int    y   = (int)Param[1]->Val->Integer;
    char * fmt = (char *)Param[2]->Val->Pointer;

    struct StdVararg PrintfArgs;
    char             str[500] = "";
    PrintfArgs.Param = Param + 2;
    PrintfArgs.NumArgs = NumArgs - 3;
    StdioBasePrintf(Parser, NULL, str, sizeof(str), fmt, &PrintfArgs);
    
    sdlx_loc_t *loc;
    loc = sdlx_render_printf(x, y, "%s", str);
    ReturnValue->Val->Pointer = loc;
}

void Sdlx_char_width(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int fontid = (int)Param[0]->Val->Integer;

    int retval;
    retval = sdlx_char_width(fontid);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_char_height(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int fontid = (int)Param[0]->Val->Integer;

    int retval;
    retval = sdlx_char_height(fontid);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_render_printf_ex1(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int          x      = (int)Param[0]->Val->Integer;
    int          y      = (int)Param[1]->Val->Integer;
    int          fontid = (int)Param[2]->Val->Integer;
    sdlx_color_t color  = (sdlx_color_t)Param[3]->Val->UnsignedInteger;
    char *       fmt    = (char *)Param[4]->Val->Pointer;

    struct StdVararg PrintfArgs;
    char             str[500] = "";
    PrintfArgs.Param = Param + 4;
    PrintfArgs.NumArgs = NumArgs - 5;
    StdioBasePrintf(Parser, NULL, str, sizeof(str), fmt, &PrintfArgs);

    sdlx_loc_t *loc;
    loc = sdlx_render_printf_ex1(x, y, fontid, color, "%s", str);
    ReturnValue->Val->Pointer = loc;
}

void Sdlx_render_printf_ex2(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int          x      = (int)Param[0]->Val->Integer;
    int          y      = (int)Param[1]->Val->Integer;
    int          fontid = (int)Param[2]->Val->Integer;
    sdlx_color_t color  = (sdlx_color_t)Param[3]->Val->UnsignedInteger;
    unsigned int flags  = (int)Param[4]->Val->Integer;
    char *       fmt    = (char *)Param[5]->Val->Pointer;

    struct StdVararg PrintfArgs;
    char             str[500] = "";
    PrintfArgs.Param = Param + 5;
    PrintfArgs.NumArgs = NumArgs - 6;
    StdioBasePrintf(Parser, NULL, str, sizeof(str), fmt, &PrintfArgs);

    sdlx_loc_t *loc;
    loc = sdlx_render_printf_ex2(x, y, fontid, color, flags, "%s", str);
    ReturnValue->Val->Pointer = loc;
}

void Sdlx_render_multiline_text(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int            x         = (int)Param[0]->Val->Integer;
    int            y         = (int)Param[1]->Val->Integer;
    int            y_top     = (int)Param[2]->Val->Integer;
    int            y_bottom  = (int)Param[3]->Val->Integer;
    int            fontid    = (int)Param[4]->Val->Integer;
    char * *       lines     = (char * *)Param[5]->Val->Pointer;
    sdlx_color_t * colors    = (sdlx_color_t *)Param[6]->Val->Pointer;
    int            num_lines = (int)Param[7]->Val->Integer;

    sdlx_render_multiline_text(x, y, y_top, y_bottom, fontid, lines, colors, num_lines);
}

//
// video - render rectangle, lines, circles, points
//

void Sdlx_render_rect(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int          x          = (int)Param[0]->Val->Integer;
    int          y          = (int)Param[1]->Val->Integer;
    int          w          = (int)Param[2]->Val->Integer;
    int          h          = (int)Param[3]->Val->Integer;
    int          line_width = (int)Param[4]->Val->Integer;
    sdlx_color_t color      = (sdlx_color_t)Param[5]->Val->UnsignedInteger;

    sdlx_render_rect(x, y, w, h, line_width, color);
}

void Sdlx_render_fill_rect(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int          x     = (int)Param[0]->Val->Integer;
    int          y     = (int)Param[1]->Val->Integer;
    int          w     = (int)Param[2]->Val->Integer;
    int          h     = (int)Param[3]->Val->Integer;
    sdlx_color_t color = (sdlx_color_t)Param[4]->Val->UnsignedInteger;

    sdlx_render_fill_rect(x, y, w, h, color);
}

void Sdlx_render_line(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int          x1    = (int)Param[0]->Val->Integer;
    int          y1    = (int)Param[1]->Val->Integer;
    int          x2    = (int)Param[2]->Val->Integer;
    int          y2    = (int)Param[3]->Val->Integer;
    sdlx_color_t color = (sdlx_color_t)Param[4]->Val->UnsignedInteger;

    sdlx_render_line(x1, y1, x2, y2, color);
}

void Sdlx_render_lines(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_point_t * points = (sdlx_point_t *)Param[0]->Val->Pointer;
    int            count  = (int)Param[1]->Val->Integer;
    sdlx_color_t   color  = (sdlx_color_t)Param[2]->Val->UnsignedInteger;

    sdlx_render_lines(points, count, color);
}

void Sdlx_render_circle(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int          x_ctr      = (int)Param[0]->Val->Integer;
    int          y_ctr      = (int)Param[1]->Val->Integer;
    int          radius     = (int)Param[2]->Val->Integer;
    int          line_width = (int)Param[3]->Val->Integer;
    sdlx_color_t color      = (sdlx_color_t)Param[4]->Val->UnsignedInteger;

    sdlx_render_circle(x_ctr, y_ctr, radius, line_width, color);
}

void Sdlx_render_fill_circle(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int          x_ctr  = (int)Param[0]->Val->Integer;
    int          y_ctr  = (int)Param[1]->Val->Integer;
    int          radius = (int)Param[2]->Val->Integer;
    sdlx_color_t color  = (sdlx_color_t)Param[3]->Val->UnsignedInteger;

    sdlx_render_fill_circle(x_ctr, y_ctr, radius, color);
}

void Sdlx_render_point(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int          x          = (int)Param[0]->Val->Integer;
    int          y          = (int)Param[1]->Val->Integer;
    sdlx_color_t color      = (sdlx_color_t)Param[2]->Val->UnsignedInteger;
    int          point_size = (int)Param[3]->Val->Integer;

    sdlx_render_point(x, y, color, point_size);
}

void Sdlx_render_points(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_point_t * points     = (sdlx_point_t *)Param[0]->Val->Pointer;
    int            count      = (int)Param[1]->Val->Integer;
    sdlx_color_t   color      = (sdlx_color_t)Param[2]->Val->UnsignedInteger;
    int            point_size = (int)Param[3]->Val->Integer;

    sdlx_render_points(points, count, color, point_size);
}

//
// video - textures
//

void Sdlx_create_texture(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int w = (int)Param[0]->Val->Integer;
    int h = (int)Param[1]->Val->Integer;

    sdlx_texture_t * retval;
    retval = sdlx_create_texture(w, h);
    ReturnValue->Val->Pointer = retval;
}

void Sdlx_destroy_texture(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * t = (sdlx_texture_t *)Param[0]->Val->Pointer;

    sdlx_destroy_texture(t);
}

void Sdlx_query_texture(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * t = (sdlx_texture_t *)Param[0]->Val->Pointer;
    int *            w = (int *)Param[1]->Val->Pointer;
    int *            h = (int *)Param[2]->Val->Pointer;

    sdlx_query_texture(t, w, h);
}

void Sdlx_clear_texture(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * t     = (sdlx_texture_t *)Param[0]->Val->Pointer;
    sdlx_color_t     color = (sdlx_color_t)Param[1]->Val->UnsignedInteger;

    sdlx_clear_texture(t, color);
}

void Sdlx_color_mod_texture(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * t = (sdlx_texture_t *)Param[0]->Val->Pointer;
    float            r = (float)Param[1]->Val->FP32;
    float            g = (float)Param[2]->Val->FP32;
    float            b = (float)Param[3]->Val->FP32;

    sdlx_color_mod_texture(t, r, g, b);
}

void Sdlx_set_texture_pixels(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * t      = (sdlx_texture_t *)Param[0]->Val->Pointer;
    unsigned int *   pixels = (unsigned int *)Param[1]->Val->Pointer;

    sdlx_set_texture_pixels(t, pixels);
}

void Sdlx_get_texture_pixels(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * t = (sdlx_texture_t *)Param[0]->Val->Pointer;
    int *            w = (int *)Param[1]->Val->Pointer;
    int *            h = (int *)Param[2]->Val->Pointer;

    unsigned int * retval;
    retval = sdlx_get_texture_pixels(t, w, h);
    ReturnValue->Val->Pointer = retval;
}

void Sdlx_render_texture(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * t = (sdlx_texture_t *)Param[0]->Val->Pointer;
    int              x = (int)Param[1]->Val->Integer;
    int              y = (int)Param[2]->Val->Integer;

    sdlx_render_texture(t, x, y);
}

void Sdlx_render_texture_ex1(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * t = (sdlx_texture_t *)Param[0]->Val->Pointer;
    int              x = (int)Param[1]->Val->Integer;
    int              y = (int)Param[2]->Val->Integer;
    int              w = (int)Param[3]->Val->Integer;
    int              h = (int)Param[4]->Val->Integer;

    sdlx_render_texture_ex1(t, x, y, w, h);
}

void Sdlx_render_texture_ex2(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * t     = (sdlx_texture_t *)Param[0]->Val->Pointer;
    int              x     = (int)Param[1]->Val->Integer;
    int              y     = (int)Param[2]->Val->Integer;
    int              w     = (int)Param[3]->Val->Integer;
    int              h     = (int)Param[4]->Val->Integer;
    double           angle = (double)Param[5]->Val->FP;

    sdlx_render_texture_ex2(t, x, y, w, h, angle);
}

void Sdlx_render_texture_ex3(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * texture = (sdlx_texture_t *)Param[0]->Val->Pointer;
    int              x       = (int)Param[1]->Val->Integer;
    int              y       = (int)Param[2]->Val->Integer;
    int              w       = (int)Param[3]->Val->Integer;
    int              h       = (int)Param[4]->Val->Integer;
    double           angle   = (double)Param[5]->Val->FP;
    int              xctr    = (int)Param[6]->Val->Integer;
    int              yctr    = (int)Param[7]->Val->Integer;

    sdlx_render_texture_ex3(texture, x, y, w, h, angle, xctr, yctr);
}

void Sdlx_set_render_target(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * t = (sdlx_texture_t *)Param[0]->Val->Pointer;

    sdlx_set_render_target(t);
}

// xxx new
void Sdlx_render_texture_new(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_texture_t * t    = (sdlx_texture_t *)Param[0]->Val->Pointer;
    sdlx_loc_t *     src  = (sdlx_loc_t *)Param[1]->Val->Pointer;
    sdlx_loc_t *     dest = (sdlx_loc_t *)Param[2]->Val->Pointer;

    sdlx_render_texture_new(t, src, dest);
}

//
// audio
//

void Sdlx_audio_stop(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int retval;
    retval = sdlx_audio_stop();
    ReturnValue->Val->Integer = retval;
}

void Sdlx_audio_pause(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_audio_pause();
}

void Sdlx_audio_resume(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_audio_resume();
}

void Sdlx_audio_get_state(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_audio_state_t * state = (sdlx_audio_state_t *)Param[0]->Val->Pointer;

    sdlx_audio_get_state(state);
}

void Sdlx_get_audio_samples(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int     num_ret_samples = (int)Param[0]->Val->Integer;
    int     num_downsample  = (int)Param[1]->Val->Integer;
    int     which_channel   = (int)Param[2]->Val->Integer;
    float * ret_samples     = (float *)Param[3]->Val->Pointer;

    sdlx_get_audio_samples(num_ret_samples, num_downsample, which_channel, ret_samples);
}

void Sdlx_audio_play_file(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char * dir      = (char *)Param[0]->Val->Pointer;
    char * filename = (char *)Param[1]->Val->Pointer;

    int retval;
    retval = sdlx_audio_play_file(dir, filename);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_audio_set_play_file_time(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int secs = Param[0]->Val->Integer;

    sdlx_audio_set_play_file_time(secs);
}

void Sdlx_audio_play_tones(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_tone_t * tones = (sdlx_tone_t *)Param[0]->Val->Pointer;

    int retval;
    retval = sdlx_audio_play_tones(tones);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_audio_play_buff(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    float * samples                = (float *)Param[0]->Val->Pointer;
    int     num_samples            = (int)Param[1]->Val->Integer;
    int     num_channels           = (int)Param[2]->Val->Integer;
    int     loops                  = (int)Param[3]->Val->Integer;
    bool    free_samples_when_done = (bool)Param[4]->Val->Integer;

    int retval;
    retval = sdlx_audio_play_buff(samples, num_samples, num_channels, loops, free_samples_when_done);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_audio_record_from_mic(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char * dir               = (char *)Param[0]->Val->Pointer;
    char * filename          = (char *)Param[1]->Val->Pointer;
    int    auto_stop_secs    = (int)Param[2]->Val->Integer;
    bool   append            = (bool)Param[3]->Val->Integer;
    bool   start_paused      = (bool)Param[4]->Val->Integer;

    int retval;
    retval = sdlx_audio_record_from_mic(dir, filename, auto_stop_secs, append, start_paused);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_audio_record_from_device(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char * dir          = (char *)Param[0]->Val->Pointer;
    char * filename     = (char *)Param[1]->Val->Pointer;
    bool   append       = (bool)Param[2]->Val->Integer;
    bool   start_paused = (bool)Param[3]->Val->Integer;

    int retval;
    retval = sdlx_audio_record_from_device(dir, filename, append, start_paused);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_create_test_file(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char * dir           = (char *)Param[0]->Val->Pointer;
    char * filename      = (char *)Param[1]->Val->Pointer;
    int    freq1         = (int)Param[2]->Val->Integer;
    int    freq2         = (int)Param[3]->Val->Integer;
    int    duration_secs = (int)Param[4]->Val->Integer;

    sdlx_create_test_file(dir, filename, freq1, freq2, duration_secs);
}

//
// sensors
//

void Sdlx_sensor_get_info_tbl(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int * max = (int *)Param[0]->Val->Pointer;

    sdlx_sensor_info_t * retval;
    retval = sdlx_sensor_get_info_tbl(max);
    ReturnValue->Val->Pointer = retval;
}

void Sdlx_sensor_find(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int type = (int)Param[0]->Val->Integer;

    int retval;
    retval = sdlx_sensor_find(type);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_sensor_read_raw(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int     id         = (int)Param[0]->Val->Integer;
    float * data       = (float *)Param[1]->Val->Pointer;
    int     num_values = (int)Param[2]->Val->Integer;

    int retval;
    retval = sdlx_sensor_read_raw(id, data, num_values);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_sensor_read_step_counter(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    unsigned long * step_count = (unsigned long *)Param[0]->Val->Pointer;

    int retval;
    retval = sdlx_sensor_read_step_counter(step_count);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_sensor_read_mag_heading(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    double * mag_heading = (double *)Param[0]->Val->Pointer;

    int retval;
    retval = sdlx_sensor_read_mag_heading(mag_heading);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_sensor_read_gravity_accel(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    double * ax = (double *)Param[0]->Val->Pointer;
    double * ay = (double *)Param[1]->Val->Pointer;
    double * az = (double *)Param[2]->Val->Pointer;

    int retval;
    retval = sdlx_sensor_read_gravity_accel(ax, ay, az);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_sensor_read_device_accel(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    double * ax = (double *)Param[0]->Val->Pointer;
    double * ay = (double *)Param[1]->Val->Pointer;
    double * az = (double *)Param[2]->Val->Pointer;

    int retval;
    retval = sdlx_sensor_read_device_accel(ax, ay, az);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_sensor_read_roll_pitch(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    double * roll  = (double *)Param[0]->Val->Pointer;
    double * pitch = (double *)Param[1]->Val->Pointer;

    int retval;
    retval = sdlx_sensor_read_roll_pitch(roll, pitch);
    ReturnValue->Val->Integer = retval;
}

void Sdlx_sensor_read_pressure(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    double * millibars = (double *)Param[0]->Val->Pointer;

    int retval;
    retval = sdlx_sensor_read_pressure(millibars);
    ReturnValue->Val->Integer = retval;
}

//
// events
//

void Sdlx_register_event(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    sdlx_loc_t * loc      = (sdlx_loc_t *)Param[0]->Val->Pointer;
    int          event_id = (int)Param[1]->Val->Integer;

    sdlx_register_event(loc, event_id);
}

void Sdlx_register_control_events(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int          evid1       = (int)Param[0]->Val->Integer;
    char *       evstr1      = (char *)Param[1]->Val->Pointer;
    int          evid2       = (int)Param[2]->Val->Integer;
    char *       evstr2      = (char *)Param[3]->Val->Pointer;
    int          evid3       = (int)Param[4]->Val->Integer;
    char *       evstr3      = (char *)Param[5]->Val->Pointer;

    sdlx_register_control_events(evid1, evstr1, evid2, evstr2, evid3, evstr3);
}

void Sdlx_get_event(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    long           timeout_us = (long)Param[0]->Val->LongInteger;
    sdlx_event_t * event      = (sdlx_event_t *)Param[1]->Val->Pointer;

    sdlx_get_event(timeout_us, event);
}

//
// misc
//

void Sdlx_show_toast(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char * message = (char *)Param[0]->Val->Pointer;

    sdlx_show_toast(message);
}

void Sdlx_get_input_str(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char *prompt         = (char *)Param[0]->Val->Pointer;
    bool  numeric_keybd  = (bool)Param[1]->Val->Integer;
    char *dflt_input_str = (char *)Param[2]->Val->Pointer;

    char * retval;
    retval = sdlx_get_input_str(prompt, numeric_keybd, dflt_input_str);
    ReturnValue->Val->Pointer = retval;
}

// -----------------  SDL REGISTRATION  ---------------------------------

void SdlSetupFunction(Picoc *pc)
{
    #define PLATFORM_VAR(name, type, writeable) \
        do { \
            VariableDefinePlatformVar(pc, NULL, #name, type, \
                                      (union AnyValue *)&name, writeable); \
        } while (0)
        
    PLATFORM_VAR(sdlx_win_width,        &pc->IntType, false);
    PLATFORM_VAR(sdlx_win_height,       &pc->IntType, false);
    PLATFORM_VAR(sdlx_char_width_dflt,  &pc->IntType, false);
    PLATFORM_VAR(sdlx_char_height_dflt, &pc->IntType, false);
}

struct LibraryFunction SdlFunctions[] = {
    // init & quit
    { Sdlx_init,                     "int sdlx_init(int subsys);" },
    { Sdlx_quit,                     "void sdlx_quit(int subsys);" },

    // video - display init and present
    { Sdlx_display_init,             "void sdlx_display_init(sdlx_color_t color, int orientation);" },
    { Sdlx_display_present,          "void sdlx_display_present(void);" },

    // video - colors
    { Sdlx_create_color,             "sdlx_color_t sdlx_create_color(int r, int g, int b, int a);" },
    { Sdlx_scale_color,              "sdlx_color_t sdlx_scale_color(sdlx_color_t color, double inten);" },
    { Sdlx_set_color_alpha,          "sdlx_color_t sdlx_set_color_alpha(sdlx_color_t color, int alpha);" },
    { Sdlx_wavelength_to_color,      "sdlx_color_t sdlx_wavelength_to_color(int wavelength);" },

    // video - render text
    { Sdlx_print_set_default,        "void sdlx_print_set_default(int fontid, sdlx_color_t color);" },
    { Sdlx_render_printf,            "sdlx_loc_t *sdlx_render_printf(int x, int y, char *fmt, ...) ;" },
    { Sdlx_char_width,               "int sdlx_char_width(int fontid);" },
    { Sdlx_char_height,              "int sdlx_char_height(int fontid);" },
    { Sdlx_render_printf_ex1,        "sdlx_loc_t *sdlx_render_printf_ex1(int x, int y, int fontid, sdlx_color_t color, char * fmt, ...);" },
    { Sdlx_render_printf_ex2,        "sdlx_loc_t *sdlx_render_printf_ex2(int x, int y, int fontid, sdlx_color_t color, unsigned int flags, char *fmt, ...);" },
    { Sdlx_render_multiline_text,    "void sdlx_render_multiline_text(int x, int y, int y_top, int y_bottom, int fontid, char **lines, sdlx_color_t *colors, int num_lines);" },

    // video - render rectangle, lines, circles, points
    { Sdlx_render_rect,              "void sdlx_render_rect(int x, int y, int w, int h, int line_width, sdlx_color_t color);" },
    { Sdlx_render_fill_rect,         "void sdlx_render_fill_rect(int x, int y, int w, int h, sdlx_color_t color);" },
    { Sdlx_render_line,              "void sdlx_render_line(int x1, int y1, int x2, int y2, sdlx_color_t color);" },
    { Sdlx_render_lines,             "void sdlx_render_lines(sdlx_point_t *points, int count, sdlx_color_t color);" },
    { Sdlx_render_circle,            "void sdlx_render_circle(int x_ctr, int y_ctr, int radius, int line_width, sdlx_color_t color);" },
    { Sdlx_render_fill_circle,       "void sdlx_render_fill_circle(int x_ctr, int y_ctr, int radius, sdlx_color_t color);" },
    { Sdlx_render_point,             "void sdlx_render_point(int x, int y, sdlx_color_t color, int point_size);" },
    { Sdlx_render_points,            "void sdlx_render_points(sdlx_point_t *points, int count, sdlx_color_t color, int point_size);" },

    // video - textures
    { Sdlx_create_texture,           "sdlx_texture_t *sdlx_create_texture(int w, int h);" },
    { Sdlx_destroy_texture,          "void sdlx_destroy_texture(sdlx_texture_t *t);" },
    { Sdlx_query_texture,            "void sdlx_query_texture(sdlx_texture_t *t, int *w, int *h);" },
    { Sdlx_clear_texture,            "void sdlx_clear_texture(sdlx_texture_t *t, sdlx_color_t color);" },
    { Sdlx_color_mod_texture,        "void sdlx_color_mod_texture(sdlx_texture_t *t, float r, float g, float b);" },
    { Sdlx_set_texture_pixels,       "void sdlx_set_texture_pixels(sdlx_texture_t *t, unsigned int *pixels);" },
    { Sdlx_get_texture_pixels,       "unsigned int *sdlx_get_texture_pixels(sdlx_texture_t *t, int *w, int *h);" },
    { Sdlx_render_texture,           "void sdlx_render_texture(sdlx_texture_t *t, int x, int y);" },
    { Sdlx_render_texture_ex1,       "void sdlx_render_texture_ex1(sdlx_texture_t *t, int x, int y, int w, int h);" },
    { Sdlx_render_texture_ex2,       "void sdlx_render_texture_ex2(sdlx_texture_t *t, int x, int y, int w, int h, double angle);" },
    { Sdlx_render_texture_ex3,       "void sdlx_render_texture_ex3(sdlx_texture_t *texture, int x, int y, int w, int h, double angle, int xctr, int yctr);" },
    // xxx new
    { Sdlx_render_texture_new,       "void sdlx_render_texture_new(sdlx_texture_t *src_texture, sdlx_loc_t *src, sdlx_loc_t *dest);" },
    { Sdlx_set_render_target,        "void sdlx_set_render_target(sdlx_texture_t *t);" },

    // audio
    { Sdlx_audio_stop,               "int sdlx_audio_stop(void);" },
    { Sdlx_audio_pause,              "void sdlx_audio_pause(void);" },
    { Sdlx_audio_resume,             "void sdlx_audio_resume(void);" },
    { Sdlx_audio_get_state,          "void sdlx_audio_get_state(sdlx_audio_state_t * state);" },
    { Sdlx_get_audio_samples,        "void sdlx_get_audio_samples(int num_ret_samples, int num_downsample, int which_channel, float *ret_samples);" },
    { Sdlx_audio_play_file,          "int sdlx_audio_play_file(char *dir, char *filename);" },
    { Sdlx_audio_set_play_file_time, "void sdlx_audio_set_play_file_time(int secs);" },
    { Sdlx_audio_play_tones,         "int sdlx_audio_play_tones(sdlx_tone_t *tones);" },
    { Sdlx_audio_play_buff,          "int sdlx_audio_play_buff(float *samples, int num_samples, int num_channels, int loops, bool free_samples_when_done);" },
    { Sdlx_audio_record_from_mic,    "int sdlx_audio_record_from_mic(char *dir, char *filename, int auto_stop_secs, bool append, bool start_paused);" },
    { Sdlx_audio_record_from_device, "int sdlx_audio_record_from_device(char *dir, char *filename, bool append, bool start_paused);" },
    { Sdlx_create_test_file,         "void sdlx_create_test_file(char *dir, char *filename, int freq1, int freq2, int duration_secs);" },

    // sensors
    { Sdlx_sensor_get_info_tbl,      "sdlx_sensor_info_t *sdlx_sensor_get_info_tbl(int *max);" },
    { Sdlx_sensor_find,              "int sdlx_sensor_find(int type);" },
    { Sdlx_sensor_read_raw,          "int sdlx_sensor_read_raw(int id, float *data, int num_values);" },
    { Sdlx_sensor_read_step_counter, "int sdlx_sensor_read_step_counter(unsigned long *step_count);" },
    { Sdlx_sensor_read_mag_heading,  "int sdlx_sensor_read_mag_heading(double *mag_heading);" },
    { Sdlx_sensor_read_device_accel, "int sdlx_sensor_read_device_accel(double *ax, double *ay, double *az);" },
    { Sdlx_sensor_read_gravity_accel,"int sdlx_sensor_read_gravity_accel(double *ax, double *ay, double *az);" },
    { Sdlx_sensor_read_roll_pitch,   "int sdlx_sensor_read_roll_pitch(double *roll, double *pitch);" },
    { Sdlx_sensor_read_pressure,     "int sdlx_sensor_read_pressure(double *millibars);" },

    // events
    { Sdlx_register_event,           "void sdlx_register_event(sdlx_loc_t *loc, int event_id);" },
    { Sdlx_register_control_events,  "void sdlx_register_control_events(int evid1, char *evstr1, int evid2, char *evstr2, int evid3, char *evstr3);" },
    { Sdlx_get_event,                "void sdlx_get_event(long timeout_us, sdlx_event_t *event);" },

    // misc
    { Sdlx_show_toast,               "void sdlx_show_toast(char *message);" },
    { Sdlx_get_input_str,            "char *sdlx_get_input_str(char *prompt, bool numeric_keybd, char *dflt_input_str);" },

    { NULL, NULL } };

const char SdlDefs[] = "\
/* misc */ \n\
#define INVALID_NUMBER 999999999 \n\
\n\
/* init/quit */ \n\
#define SUBSYS_VIDEO  1 \n\
#define SUBSYS_AUDIO  2 \n\
#define SUBSYS_SENSOR 4 \n\
\n\
/* video typedefs  */ \n\
typedef unsigned int sdlx_color_t; \n\
typedef struct sdlx_texture sdlx_texture_t; \n\
typedef struct { \n\
    int x; \n\
    int y; \n\
    int w; \n\
    int h; \n\
} sdlx_loc_t; \n\
typedef struct { \n\
    int x; \n\
    int y; \n\
} sdlx_point_t; \n\
\n\
/* video display orientation, for call to sdlx_display_init */ \n\
#define PORTRAIT  0 \n\
#define LANDSCAPE 1 \n\
/* video colors */ \n\
#define BYTES_PER_PIXEL    4 \n\
#define COLOR_BLACK        0xff000000   /* abgr */ \n\
#define COLOR_WHITE        0xffffffff \n\
#define COLOR_RED          0xff0000ff \n\
#define COLOR_ORANGE       0xff0080ff \n\
#define COLOR_YELLOW       0xff00ffff \n\
#define COLOR_GREEN        0xff00ff00 \n\
#define COLOR_BLUE         0xffff0000 \n\
#define COLOR_INDIGO       0xff82004b \n\
#define COLOR_VIOLET       0xffee82ee \n\
#define COLOR_PURPLE       0xffff007f \n\
#define COLOR_LIGHT_BLUE   0xffffff00 \n\
#define COLOR_LIGHT_GREEN  0xff90ee90 \n\
#define COLOR_PINK         0xffb469ff \n\
#define COLOR_TEAL         0xff808000 \n\
#define COLOR_LIGHT_GRAY   0xffc0c0c0 \n\
#define COLOR_GRAY         0xff808080 \n\
#define COLOR_DARK_GRAY    0xff404040 \n\
\n\
/* video fonts */ \n\
#define FONT_TINY     40   /* 40 chars fit in display width */ \n\
#define FONT_SMALL    30   /* 30 chars fit in display width */ \n\
#define FONT_NORMAL   20   /* etc */ \n\
#define FONT_LARGE    10 \n\
#define ROW2Y(r)      ((r) * sdlx_char_height_dflt) \n\
#define COL2X(c)      ((c) * sdlx_char_width_dflt) \n\
#define FLAG_WRAP_MASK     0x00000fff \n\
#define FLAG_X_CTR         0x00001000 \n\
#define FLAG_Y_CTR         0x00002000 \n\
#define FLAG_ROT_CTR_90    0x00004000 \n\
#define FLAG_ROT_CTR_180   0x00008000 \n\
#define FLAG_ROT_CTR_270   0x00010000 \n\
#define FLAG_BG_BLACK      0x00020000 \n\
#define FLAG_BG_WHITE      0x00040000 \n\
#define FLAG_XY_CTR        (FLAG_X_CTR | FLAG_Y_CTR) \n\
/* video misc */ \n\
#define MAX_POINT_SIZE 9 \n\
\n\
/* audio */ \n\
#define FRAMES_PER_SEC 48000 \n\
#define AUDIO_STATE_IDLE                0 \n\
#define AUDIO_STATE_PLAY_FILE           1 \n\
#define AUDIO_STATE_PLAY_TONES_SEQUENCE 2 \n\
#define AUDIO_STATE_PLAY_BUFF           3 \n\
#define AUDIO_STATE_RECORD_FROM_MIC     4 \n\
#define AUDIO_STATE_RECORD_FROM_DEVICE  5 \n\
#define GET_SAMPLES_MONO          0 \n\
#define GET_SAMPLES_LEFT_CHANNEL  1 \n\
#define GET_SAMPLES_RIGHT_CHANNEL 2 \n\
typedef struct { \n\
    short freq; \n\
    short intvl_ms; \n\
} sdlx_tone_t; \n\
typedef struct { \n\
    int    state; \n\
    bool   stopping; \n\
    bool   paused; \n\
    int    play_current_secs; \n\
    int    play_total_secs; \n\
    int    record_secs; \n\
    int    play_tones_freq; \n\
    int    play_tones_seqnum; \n\
    double volume; \n\
} sdlx_audio_state_t; \n\
\n\
typedef struct { \n\
    int   id; \n\
    int   type; \n\
    char *name; \n\
} sdlx_sensor_info_t; \n\
\n\
/* events */ \n\
#define EVID_MOTION  10000 \n\
#define EVID_PINCH   10001 \n\
#define EVID_QUIT    10009 \n\
typedef struct { \n\
    int event_id; \n\
    union { \n\
        struct { \n\
            double x; \n\
            double y; \n\
            double xrel; \n\
            double yrel; \n\
        } motion; \n\
        struct { \n\
            double scale; \n\
            double span_x; \n\
            double span_y; \n\
            double focus_x; \n\
            double focus_y; \n\
        } pinch; \n\
    } u; \n\
} sdlx_event_t; \n\
";

// xxx maybe pad struct so entire length is 64   OR dont pad at all,  AND keybd

// -----------------  UTILS PLATFORM ROUTINES  --------------------------

//
// utils time routines
//

void Util_microsec_timer (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    ReturnValue->Val->LongInteger = util_microsec_timer();
}

void Util_get_real_time_microsec (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    ReturnValue->Val->LongInteger = util_get_real_time_microsec();
}

void Util_time2str (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *str          = Param[0]->Val->Pointer;
    long  us           = Param[1]->Val->LongInteger;
    int   gmt          = Param[2]->Val->Integer;
    int   display_ms   = Param[3]->Val->Integer;
    int   display_date = Param[4]->Val->Integer;
    char *s;

    s = util_time2str(str, us, gmt, display_ms, display_date);
    ReturnValue->Val->Pointer = s;
}

//
// utils file write / read routines
//

void Util_write_file (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *dir  = Param[0]->Val->Pointer;
    char *fn   = Param[1]->Val->Pointer;
    void *data = Param[2]->Val->Pointer;
    int   len  = Param[3]->Val->Integer;
    int   ret;

    ret = util_write_file(dir, fn, data, len);
    ReturnValue->Val->Integer = ret;
}

void Util_read_file (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *dir  = Param[0]->Val->Pointer;
    char *fn   = Param[1]->Val->Pointer;
    int  *len  = Param[2]->Val->Pointer;
    void *file_contents;

    file_contents = util_read_file(dir, fn, len);
    ReturnValue->Val->Pointer = file_contents;
}

void Util_delete_file (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *dir  = Param[0]->Val->Pointer;
    char *fn   = Param[1]->Val->Pointer;

    util_delete_file(dir, fn);
}

void Util_rename_file(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char * old_dir = (char *)Param[0]->Val->Pointer;
    char * old_fn  = (char *)Param[1]->Val->Pointer;
    char * new_dir = (char *)Param[2]->Val->Pointer;
    char * new_fn  = (char *)Param[3]->Val->Pointer;

    util_rename_file(old_dir, old_fn, new_dir, new_fn);
}

void Util_file_exists (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *dir   = Param[0]->Val->Pointer;
    char *fn    = Param[1]->Val->Pointer;
    bool exists;

    exists = util_file_exists(dir, fn);
    ReturnValue->Val->Integer = exists;
}

void Util_file_mtime (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *dir   = Param[0]->Val->Pointer;
    char *fn    = Param[1]->Val->Pointer;
    long mtime;

    mtime = util_file_mtime(dir, fn);
    ReturnValue->Val->LongInteger = mtime;
}

void Util_file_size (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *dir   = Param[0]->Val->Pointer;
    char *fn    = Param[1]->Val->Pointer;
    long size;

    size = util_file_size(dir, fn);
    ReturnValue->Val->LongInteger = size;
}

//
// utils directory routines
//

void Util_create_dir (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *dir            = Param[0]->Val->Pointer;
    char *dir_to_create  = Param[1]->Val->Pointer;

    util_create_dir(dir, dir_to_create);
}

void Util_delete_dir (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *dir            = Param[0]->Val->Pointer;
    char *dir_to_delete  = Param[1]->Val->Pointer;

    util_delete_dir(dir, dir_to_delete);
}

//
// utils file map routines
//

void Util_map_file (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *dir              = Param[0]->Val->Pointer;
    char *file             = Param[1]->Val->Pointer;
    int   len              = Param[2]->Val->Integer;
    bool  create_if_needed = Param[3]->Val->Integer;
    int   *created_flag    = Param[4]->Val->Pointer;
    void *addr;

    addr = util_map_file(dir, file, len, create_if_needed, created_flag);
    ReturnValue->Val->Pointer = addr;
}

void Util_unmap_file (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *addr = Param[0]->Val->Pointer;
    int   len  = Param[1]->Val->Integer;

    util_unmap_file(addr, len);
}

void Util_sync_file (struct ParseState *Parser, struct Value *ReturnValue,
	struct Value **Param, int NumArgs)
{
    char *addr = Param[0]->Val->Pointer;
    int   len  = Param[1]->Val->Integer;

    util_sync_file(addr, len);
}

//
// utils params
//

void Util_get_str_param(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char *dir           = Param[0]->Val->Pointer;
    char *name          = Param[1]->Val->Pointer;
    char *default_value = Param[2]->Val->Pointer;
    char *value;

    value = util_get_str_param(dir, name, default_value);
    ReturnValue->Val->Pointer = value;
}

void Util_set_str_param(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char *dir   = Param[0]->Val->Pointer;
    char *name  = Param[1]->Val->Pointer;
    char *value = Param[2]->Val->Pointer;

    util_set_str_param(dir, name, value);
}

void Util_get_numeric_param(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char  *dir           = Param[0]->Val->Pointer;
    char  *name          = Param[1]->Val->Pointer;
    double default_value = Param[2]->Val->FP;
    double value;

    value = util_get_numeric_param(dir, name, default_value);
    ReturnValue->Val->FP = value;
}

void Util_set_numeric_param(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char  *dir   = Param[0]->Val->Pointer;
    char  *name  = Param[1]->Val->Pointer;
    double value = Param[2]->Val->FP;

    util_set_numeric_param(dir, name, value);
}

void Util_print_params(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char *dir = Param[0]->Val->Pointer;

    util_print_params(dir);
}

//
// utils network
//

void Util_get_ipaddr(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char *ipaddr_str = (char*)Param[0]->Val->Pointer;

    ReturnValue->Val->Pointer = util_get_ipaddr(ipaddr_str);
}

//
// utils json decoder
//

void Util_json_parse(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char *str =      (char*)Param[0]->Val->Pointer;
    char **end_ptr = (char**)Param[1]->Val->Pointer;
    void *json_root;

    json_root = util_json_parse(str, end_ptr);

    ReturnValue->Val->Pointer = json_root;
}

void Util_json_free(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    void *json_root = Param[0]->Val->Pointer;

    util_json_free(json_root);
}

void Util_json_get_value(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    #define MAX_ARGS 10
    void         *json_item = Param[0]->Val->Pointer;
    struct        Value *ThisArg = Param[0];
    int           i;
    char         *args[MAX_ARGS];
    json_value_t *value;
    bool          last_arg_is_expected_null = false;

    if (NumArgs-1 > MAX_ARGS) {
        ProgramFail(Parser, "util_json_get_value: too many args");
    }

    memset(args, 0, sizeof(args));

    for (i = 0; i < NumArgs-1; i++) {
        ThisArg = (struct Value*)((char*)ThisArg +
                            MEM_ALIGN(sizeof(struct Value)+TypeStackSizeValue(ThisArg)));
        if (ThisArg->Typ->Base == TypePointer) {
            args[i] = (char*)ThisArg->Val->Pointer;
        } else if (ThisArg->Typ->Base == TypeArray &&
                   ThisArg->Typ->FromType->Base == TypeChar)
        {
            args[i] = &ThisArg->Val->ArrayMem[0];
        } else if (ThisArg->Typ->Base == TypeInt && 
                   ThisArg->Val->Integer == 0 &&
                   i == NumArgs-2)
        {
            last_arg_is_expected_null = true;
        } else {
            ProgramFail(Parser, "util_json_get_value: invalid arg");
        }
    }

    if (!last_arg_is_expected_null) {
        ProgramFail(Parser, "util_json_get_value: last arg must be NULL");
    }

    value = util_json_get_value(
                json_item,
                args[0], args[1], args[2], args[3], args[4], 
                args[5], args[6], args[7], args[8], args[9]);

    ReturnValue->Val->Pointer = value;
}

//
// utils read/write 32-bit RGBA png files
//

void Util_read_png_file(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char           *dir      = Param[0]->Val->Pointer;
    char           *filename = Param[1]->Val->Pointer;
    unsigned char **pixels   = Param[2]->Val->Pointer;
    int            *w        = Param[3]->Val->Pointer;
    int            *h        = Param[4]->Val->Pointer;
    int             rc;

    rc = util_read_png_file(dir, filename, pixels, w, h);

    ReturnValue->Val->Integer = rc;
}

void Util_write_png_file(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char          *dir      = Param[0]->Val->Pointer;
    char          *filename = Param[1]->Val->Pointer;
    unsigned char *pixels   = Param[2]->Val->Pointer;
    int            w        = Param[3]->Val->Integer;
    int            h        = Param[4]->Val->Integer;
    int            rc;

    rc = util_write_png_file(dir, filename, pixels, w, h);

    ReturnValue->Val->Integer = rc;
}

//
// utils fft
//

void Util_fft_real_to_real(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int     n_fft          = (int)Param[0]->Val->Integer;
    float * input          = (float *)Param[1]->Val->Pointer;
    float * output         = (float *)Param[2]->Val->Pointer;
    bool    scale_by_n_fft = (bool)Param[3]->Val->Integer;

    util_fft_real_to_real(n_fft, input, output, scale_by_n_fft);
}

void Util_fft_real_to_complex(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int         n_fft          = (int)Param[0]->Val->Integer;
    float *     input          = (float *)Param[1]->Val->Pointer;
    complex_t * cpx_output     = (complex_t *)Param[2]->Val->Pointer;
    bool        scale_by_n_fft = (bool)Param[3]->Val->Integer;

    util_fft_real_to_complex(n_fft, input, cpx_output, scale_by_n_fft);
}

void Util_fft_inverse_complex_to_real(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int         n_fft          = (int)Param[0]->Val->Integer;
    complex_t * cpx_input      = (complex_t *)Param[1]->Val->Pointer;
    float *     output         = (float *)Param[2]->Val->Pointer;
    bool        scale_by_n_fft = (bool)Param[3]->Val->Integer;

    util_fft_inverse_complex_to_real(n_fft, cpx_input, output, scale_by_n_fft);
}

void Util_fft_test(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    long * duration_usec = (long *)Param[0]->Val->Pointer;

    ReturnValue->Val->Integer = util_fft_test(duration_usec);
}

void Util_rms_float(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    float * x = (float *)Param[0]->Val->Pointer;
    int     n = (int)Param[1]->Val->Integer;

    double retval;
    retval = util_rms_float(x, n);
    ReturnValue->Val->FP = retval;
}

//
// utils java methods
//

// get location: latitude, longitude, and altitude & alt_is_wgs84
void Util_get_location(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    double *lat          = (double*)Param[0]->Val->Pointer;
    double *lng          = (double*)Param[1]->Val->Pointer;
    double *alt_ft       = (double*)Param[2]->Val->Pointer;
    bool   *alt_is_wgs84 = (bool*)Param[3]->Val->Pointer;

    util_get_location(lat, lng, alt_ft, alt_is_wgs84);
}

// text to speech
void Util_text_to_speech(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char *text = Param[0]->Val->Pointer;

    util_text_to_speech(text);
}

void Util_text_to_speech_stop(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    util_text_to_speech_stop();
}

// flashlight
void Util_turn_flashlight_on(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    util_turn_flashlight_on();
}

void Util_turn_flashlight_off(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    util_turn_flashlight_off();
}

void Util_toggle_flashlight(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    util_toggle_flashlight();
}

void Util_is_flashlight_on(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    bool is_on;

    is_on = util_is_flashlight_on();
    ReturnValue->Val->Integer = is_on;
}

// playback capture   
void Util_start_playbackcapture(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    util_start_playbackcapture();
}

void Util_stop_playbackcapture(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    util_stop_playbackcapture();
}

void Util_get_playbackcapture_audio(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    float *array = Param[0]->Val->Pointer;
    int    num_elements = Param[1]->Val->Integer;

    util_get_playbackcapture_audio(array, num_elements);
}

//
// utils camera support
//

void Util_take_picture(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int rc;

    rc = util_take_picture();
    ReturnValue->Val->Integer = rc;
}

void Util_decode_jpeg_to_raw(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int    fd          = Param[0]->Val->Integer;
    int   *out_width   = Param[1]->Val->Pointer;
    int   *out_height  = Param[2]->Val->Pointer;
    void **out_pixels  = Param[3]->Val->Pointer;
    int    rc;

    rc = util_decode_jpeg_to_raw(fd, out_width, out_height, out_pixels);
    ReturnValue->Val->Integer = rc;
}

// -----------------  UTILS REGISTRATION  -------------------------------

void UtilsSetupFunction(Picoc *pc)
{
}

struct LibraryFunction UtilsFunctions[] = {
    // time
    { Util_microsec_timer,   "long util_microsec_timer(void);" },
    { Util_get_real_time_microsec, "long util_get_real_time_microsec(void);" },
    { Util_time2str,         "char *util_time2str(char * str, long us, bool gmt, bool display_ms, bool display_date);" },
    // file utils     
    { Util_write_file,       "int util_write_file(char *dir, char *fn, void *data, int len);" },
    { Util_read_file,        "void *util_read_file(char *dir, char *fn, int *len);" },
    { Util_delete_file,      "void *util_delete_file(char *dir, char *fn);" },
    { Util_rename_file,      "void util_rename_file(char *old_dir, char *old_fn, char *new_dir, char *new_fn);" },
    { Util_file_exists,      "bool util_file_exists(char *dir, char *fn);" },
    { Util_file_mtime,       "long util_file_mtime(char *dir, char *fn);" },
    { Util_file_size,        "long util_file_size(char *dir, char *fn);" },
    // directory utils
    { Util_create_dir,       "void util_create_dir(char *dir, char *dir_to_create);" },
    { Util_delete_dir,       "void util_delete_dir(char *dir, char *dir_to_delete);" },
    // file map
    { Util_map_file,         "void *util_map_file(char *dir, char *file, int len, bool create_if_needed, int *created_flag);" },
    { Util_unmap_file,       "void util_unmap_file(void *addr, int len);" },
    { Util_sync_file,        "void util_sync_file(void *addr, int len);" },
    // params get/set
    { Util_get_str_param,    "char *util_get_str_param(char *dir, char *name, char *default_value);" },
    { Util_set_str_param,    "void util_set_str_param(char *dir, char *name, char *value);" },
    { Util_get_numeric_param,"double util_get_numeric_param(char *dir, char *name, double default_value);" },
    { Util_set_numeric_param,"void util_set_numeric_param(char *dir, char *name, double value);" },
    { Util_print_params,     "void util_print_params(char *dir);" },
    // network
    { Util_get_ipaddr,       "char *util_get_ipaddr(char *ipaddr_str);" },
    // json
    { Util_json_parse,       "void *util_json_parse(char *str, char **end_ptr);" },
    { Util_json_free,        "void util_json_free(void *json_root);" },
    { Util_json_get_value,   "json_value_t *util_json_get_value(void *json_item, ...);" },
    // png file read/write
    { Util_read_png_file,    "int util_read_png_file(char *dir, char *filename, unsigned char **pixels, int *w, int *h);" },
    { Util_write_png_file,   "int util_write_png_file(char *dir, char *filename, unsigned char *pixels, int w, int h);" },
    // fft
    { Util_fft_real_to_real,    "void util_fft_real_to_real(int n_fft, float *input, float *output, bool scale_by_n_fft);" },
    { Util_fft_real_to_complex, "void util_fft_real_to_complex(int n_fft, float *input, complex_t *cpx_output, bool scale_by_n_fft);" },
    { Util_fft_inverse_complex_to_real, "void util_fft_inverse_complex_to_real(int n_fft, complex_t *cpx_input, float *output, bool scale_by_n_fft);" },
    { Util_fft_test,            "int util_fft_test(long *duration_usec);" },
    { Util_rms_float,           "double util_rms_float(float *x, int n);" },
    // call java: location
    { Util_get_location,        "void util_get_location(double *latitude, double *longitude, double *altitude_ft, bool *alt_is_wgs84);" },
    // call java: text to speech
    { Util_text_to_speech,      "void util_text_to_speech(char *text);" },
    { Util_text_to_speech_stop, "void util_text_to_speech_stop(void);" },
    // call java: flashlight
    { Util_turn_flashlight_on,  "void util_turn_flashlight_on(void);" },
    { Util_turn_flashlight_off, "void util_turn_flashlight_off(void);" },
    { Util_toggle_flashlight,   "void util_toggle_flashlight(void);" },
    { Util_is_flashlight_on,    "bool util_is_flashlight_on(void);" },
    // call java: playbackcapture
    { Util_start_playbackcapture,     "void util_start_playbackcapture(void);" },
    { Util_stop_playbackcapture,      "void util_stop_playbackcapture(void);" },
    { Util_get_playbackcapture_audio, "void util_get_playbackcapture_audio(float *array, int num_array_elements);" },
    // camera support
    { Util_take_picture,        "int util_take_picture(void);" },
    { Util_decode_jpeg_to_raw,  "int util_decode_jpeg_to_raw(int fd, int* out_width, int* out_height, void** out_pixels);" },

    { NULL, NULL } };

const char UtilsDefs[] = "\
/* time */ \n\
#define MAX_TIME_STR 30 \n\
/* json */ \n\
#define JSON_TYPE_UNDEFINED 0 \n\
#define JSON_TYPE_FLAG      1 \n\
#define JSON_TYPE_NUMBER    2 \n\
#define JSON_TYPE_STRING    3 \n\
#define JSON_TYPE_ARRAY     4 \n\
#define JSON_TYPE_OBJECT    5 \n\
\n\
typedef struct { \n\
    int type; \n\
    union { \n\
        bool   flag; \n\
        double number; \n\
        char  *string; \n\
        void  *array; \n\
        void  *object; \n\
    } u; \n\
} json_value_t; \n\
/* fft */ \n\
typedef struct { \n\
    float r; \n\
    float i; \n\
} complex_t; \n\
";

// -----------------  SVCS PLATFORM ROUTINES  --------------------------

//
// make service request routine, called by apps
//

void Svc_make_req(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char      *svc_name     = Param[0]->Val->Pointer;
    svc_req_t *req          = Param[1]->Val->Pointer;
    int        timeout_secs = Param[2]->Val->Integer;
    int        ret;

    ret = svc_make_req(svc_name, req, timeout_secs);
    ReturnValue->Val->Integer = ret;
}

void Svc_make_req_status_to_str(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    int rc = Param[0]->Val->Integer;

    ReturnValue->Val->Pointer = svc_make_req_status_to_str(rc);
}

//
// process service request routines, called by svcs
//

void Svc_wait_for_req(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char       *svc_name      = Param[0]->Val->Pointer;
    svc_req_t **req           = Param[1]->Val->Pointer;
    int         timeout_secs  = Param[2]->Val->Integer;
    int         ret;

    ret = svc_wait_for_req(svc_name, req, timeout_secs);
    ReturnValue->Val->Integer = ret;
}

void Svc_req_completed(struct ParseState *Parser, struct Value *ReturnValue,
        struct Value **Param, int NumArgs)
{
    char      *svc_name    = Param[0]->Val->Pointer;
    svc_req_t *req         = Param[1]->Val->Pointer;
    int        comp_status = Param[2]->Val->Integer;

    svc_req_completed(svc_name, req, comp_status);
}

// -----------------  SVCS REGISTRATION  -------------------------------

void SvcsSetupFunction(Picoc *pc)
{
}

struct LibraryFunction SvcsFunctions[] = {
    // make service request routine, called by apps
    { Svc_make_req, "int svc_make_req(char *svc_name, svc_req_t *req, int timeout_secs);" },

    // convert status return from svc_make_req to string
    { Svc_make_req_status_to_str, "char *svc_make_req_status_to_str(int rc);" },

    // process service request routines, called by svcs
    { Svc_wait_for_req,  "int svc_wait_for_req(char *svc_name, svc_req_t **req, int timeout_secs);" },
    { Svc_req_completed, "void svc_req_completed(char *svc_name, svc_req_t *req, int comp_status);" },

    { NULL, NULL } };

const char SvcsDefs[] = "\
// common values for request id \n\
#define SVC_REQ_ID_STOP 1 \n\
\n\
// sizeof of req->data \n\
#define MAX_SVC_REQ_DATA 100 \n\
\n\
// svc request struct \n\
typedef struct { \n\
    int  id; \n\
    int  comp_status; \n\
    char data[MAX_SVC_REQ_DATA]; \n\
} svc_req_t; \n\
";

// -----------------  PLATFORM INIT PROC  -------------------------------

void PlatformLibraryInit(Picoc *pc)
{
    IncludeRegister(
        pc, 
        "sdlx.h", 
        SdlSetupFunction,
        SdlFunctions, 
        SdlDefs);

    IncludeRegister(
        pc, 
        "utils.h", 
        UtilsSetupFunction,
        UtilsFunctions, 
        UtilsDefs);

    IncludeRegister(
        pc, 
        "svcs.h", 
        SvcsSetupFunction,
        SvcsFunctions, 
        SvcsDefs);
}
