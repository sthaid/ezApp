#ifndef __SDLX_H__
#define __SDLX_H__

#ifdef __cplusplus
extern "C" {
#endif

// The sdlx routines are built upon the SDL, SDL_mixer, and SDL_ttf libraries.
// The purpose of these routines is to provide easy access to the SDL features
// applicable to miniApps.
// 
// Sdlx provides routines for:
// - video:   rendering to the display
// - audio:   audio play and record
// - sensors: read Android device sensors
// - events:  registration and detection of events

// Refer to the miniApp source code for examples.
// The files/apps/Template/template.c is a good example of a minimal miniApp.

#define INVALID_NUMBER 999999999

// --------------------
// VIDEO    
// --------------------

// Sdlx_video defines a logical display that has pixel dimensions based on orientation:
// - Portrait WxH:  1000 x 2000
// - Landscape WxH: 2000 x 1000
// MiniApps do not use the real pixel dimensions of the physical display.
// 
// A texture of either (WxH) 1000x2000 (portrait), or 2000x1000 (landscape) is defined by sdlx
// Rendering is usually performed to this texture. When preenting the display, this texture
// is scaled (and rotated when in landscape mode), to fit the physical display.
//
// Additional textures can be created. For example, the Compass miniApp creates a texture 
// that is initialized to the compass image pixels.
//
// Terminology:
// - display_texture:   This is the 1000x2000 (portrait), or 2000x1000 (landscape texture.
//                      This texture is defined when sdlx_display_init is called. 
//                      And this texture becomes visible on the display when sdlx_display_present
//                      is called.
// - rendering_texture: This is texture that is selected for rendering. When sdlx_display_init 
//                      is called the rendering_texture is set to the display_texture.
// 
// The top left of the display area is (x,y) = (0,0).
// In Portrait Mode, the bottom right of the display is (x,y) = (999,2199).
// In Landscape Mode, the bottom right of the display is (x,y) = (2199,999).
// 
// Sdlx video example, display a purple circle:
//    sdlx_display_init(COLOR_BLACK, PORTRAIT);
//    int x_ctr = sdlx_win_width/2;
//    int y_ctr = sdlx_win_height/2;
//    int radius = 250;
//    sdlx_render_fill_circle(x_ctr, y_ctr, radius, COLOR_PURPLE);
//    sdlx_display_present();

typedef unsigned int sdlx_color_t;

typedef struct {
    int x, y;   // top left coords of rectangle
    int w, h;   // rectangle width and height 
} sdlx_loc_t;

typedef struct {
    int x, y;
} sdlx_point_t;

typedef struct sdlx_texture sdlx_texture_t;

// These external variables provide the display area size. They are set based
// on the orientation (PORTRAIT or LANDSCAPE) value passed to sdlx_display_init.
//  Orientation     sdlx_win_width      sdlx_win_height
//  -----------     --------------      ---------------
//  PORTRAIT            1000                2000
//  LANDSCAPE           2000                1000
extern int sdlx_win_width;
extern int sdlx_win_height;

// Video: Display Init and Present
// -------------------------------

#define PORTRAIT  0
#define LANDSCAPE 1

// This is the first step when rendering the display.
// All pixels of the display_texture are set to the specified color.
// The display orientation to be used (PORTRAIT or LANDSCAPE) is specified.
void sdlx_display_init(sdlx_color_t color, int orientation);

// Calling this routine makes the display_texture visible.
void sdlx_display_present(void);

// Video: Colors
// -------------

#define BYTES_PER_PIXEL   4

// Reference: https://www.w3schools.com/colors/colors_converter.asp
// These colors are opaque (alpha equals 255)
//                                         Red      Green       Blue      Alpha
#define COLOR_BLACK       ((sdlx_color_t)(   0  |    0<<8 |    0<<16 |  255<<24 ))
#define COLOR_WHITE       ((sdlx_color_t)( 255  |  255<<8 |  255<<16 |  255<<24 ))
#define COLOR_RED         ((sdlx_color_t)( 255  |    0<<8 |    0<<16 |  255<<24 ))
#define COLOR_ORANGE      ((sdlx_color_t)( 255  |  128<<8 |    0<<16 |  255<<24 ))
#define COLOR_YELLOW      ((sdlx_color_t)( 255  |  255<<8 |    0<<16 |  255<<24 ))
#define COLOR_GREEN       ((sdlx_color_t)(   0  |  255<<8 |    0<<16 |  255<<24 ))
#define COLOR_BLUE        ((sdlx_color_t)(   0  |    0<<8 |  255<<16 |  255<<24 ))
#define COLOR_INDIGO      ((sdlx_color_t)(  75  |    0<<8 |  130<<16 |  255<<24 ))
#define COLOR_VIOLET      ((sdlx_color_t)( 238  |  130<<8 |  238<<16 |  255<<24 ))
#define COLOR_PURPLE      ((sdlx_color_t)( 127  |    0<<8 |  255<<16 |  255<<24 ))
#define COLOR_LIGHT_BLUE  ((sdlx_color_t)(   0  |  255<<8 |  255<<16 |  255<<24 ))
#define COLOR_LIGHT_GREEN ((sdlx_color_t)( 144  |  238<<8 |  144<<16 |  255<<24 ))
#define COLOR_PINK        ((sdlx_color_t)( 255  |  105<<8 |  180<<16 |  255<<24 ))
#define COLOR_TEAL        ((sdlx_color_t)(   0  |  128<<8 |  128<<16 |  255<<24 ))
#define COLOR_LIGHT_GRAY  ((sdlx_color_t)( 192  |  192<<8 |  192<<16 |  255<<24 ))
#define COLOR_GRAY        ((sdlx_color_t)( 128  |  128<<8 |  128<<16 |  255<<24 ))
#define COLOR_DARK_GRAY   ((sdlx_color_t)(  64  |   64<<8 |   64<<16 |  255<<24 ))

// The sdlx_color_t is the 32 bit RGBA value.
// These routines perform their function and return the 32 bit RGBA value.
// Example: sdlx_create_color(255,0,0,255) is equivalent to the above '#define COLOR_RED'.
sdlx_color_t sdlx_create_color(int r, int g, int b, int a);
sdlx_color_t sdlx_scale_color(sdlx_color_t color, double intensity);
sdlx_color_t sdlx_set_color_alpha(sdlx_color_t color, int alpha);
sdlx_color_t sdlx_wavelength_to_color(int wavelength);

// Video: Render Text, Basic API
// -----------------------------

// The font used by ezApp is 'FreeMonoBold.ttf', which is a fixed width font.

// When a miniApp starts the default fontid & color are preset to FONT_NORMAL, COLOR_WHITE.

// These values use used for the fontid arg, and specify the size of the font.
// Other values are okay to use, in range 5 (largest font) to 143 (smallest font).
#define FONT_TINY     40   // 40 chars fit across portrait display
#define FONT_SMALL    30   // 30 chars fit across portrait display
#define FONT_NORMAL   20   // etc
#define FONT_LARGE    10

// This routine sets the default fontid and color.
void sdlx_print_set_default(int fontid, sdlx_color_t color);

// This printf routine prints to the rendering_texture:
// - x,y specify the upper left corner location of the printed text.
// - the default fontid and color are used.
// - the returned sdlx_loc_t contains the location of the printed text
sdlx_loc_t *sdlx_render_printf(int x, int y, char *fmt, ...) __attribute__ ((format (printf, 3, 4)));

// External variables providing the size, in pixels, of a font character,
// for the currently selected default fontid.
extern int sdlx_char_width_dflt;
extern int sdlx_char_height_dflt;

// Macros to convert from default font row,column location to x,y.
#define ROW2Y(r) ((r) * sdlx_char_height_dflt)
#define COL2X(c) ((c) * sdlx_char_width_dflt)

// Video: Render Text, Advanced API
// --------------------------------

// The Render Text Advanced API does not use the default font and color that are
// used in the Basic API.

// These printf routines are similar to sdlx_render_printf. 
// - The main difference is the fontid, and color are provided as args,
//   instead of using default fontid and color.
// - The flags arg to sdlx_render_printf_ex2 provides additional capabilities, see below.
// - the returned sdlx_loc_t contains the location of the printed text
sdlx_loc_t *sdlx_render_printf_ex1(int x, int y, int fontid, sdlx_color_t color, 
                                   char * fmt, ...)
                                   __attribute__ ((format (printf, 5, 6)));
sdlx_loc_t *sdlx_render_printf_ex2(int x, int y, int fontid, sdlx_color_t color, unsigned int flags,
                                   char *fmt, ...) 
                                   __attribute__ ((format (printf, 6, 7)));

// The sdlx_render_multiline_text routine displays text that spans multiple lines.
// - The x,y args specify the location of the top left corner of the text.
//   The x and y values can be negative.
// - The x and y values are often adjusted on receipt of the EVID_MOTION event,
//   this scrolls the text horizontally and vertically.
// - The y_top and y_bottom args specify range of y locations to which text will be displayed;
//   text is not displayed above y_top or below y_bottom.
// - The lines and colors args are parallel arrays. Color[n] specifies the color of line[n].
// - There are 2 typical use cases:
//     a) Just one line, with embedded newline chars.
//     b) multiple lines, without embedded newline chars.
// - For an example of the one line use case, refer to show_file routine in lib/lib.c.
void sdlx_render_multiline_text(int x, int y, int y_top, int y_bottom, 
                                int fontid, char **lines, sdlx_color_t *colors, int num_lines);

// These routines return the character witdh and height based on fontid.
int sdlx_char_width(int fontid);
int sdlx_char_height(int fontid);

// Values for the flags arg to sdlx_render_printf_ex2 ...
#define FLAG_WRAP_MASK     0x00000fff   // wrap text at this number of pixels; 
                                        // use zero for wrapping text only on newline char
#define FLAG_X_CTR         0x00001000   // x arg is text center instead of left
#define FLAG_Y_CTR         0x00002000   // y arg is text center instead of top
#define FLAG_ROT_CTR_90    0x00004000   // rotate text about center
#define FLAG_ROT_CTR_180   0x00008000
#define FLAG_ROT_CTR_270   0x00010000
#define FLAG_BG_BLACK      0x00020000   // use black background
#define FLAG_BG_WHITE      0x00040000   // use white background
#define FLAG_XY_CTR        (FLAG_X_CTR | FLAG_Y_CTR)

// Video: Render Rectangle, Lines, Circles, Points
// -----------------------------------------------

#define MAX_POINT_SIZE 9  // for call to sdlx_render_point and sdlx_render_points

// These routines perform the function implied by their name.
// Rendering occurs to the rendering_texture.
// - For sdlx_render_rect and sdlx_render_fill_rect, x,y are the top left corner
//   of the rectangle.
// - For sdlx_render_circle and sdlx_render_fill_circle, x_ctr,y_ctr are the 
//   center of the circle.
// - The point_size arg to sdlx_render_point and sdlx_render_points, ranges from
//   0 (smallest) to MAX_POINT_SIZE (largest).
void sdlx_render_rect(int x, int y, int w, int h, int line_width, sdlx_color_t color);
void sdlx_render_fill_rect(int x, int y, int w, int h, sdlx_color_t color);
void sdlx_render_line(int x1, int y1, int x2, int y2, sdlx_color_t color);
void sdlx_render_lines(sdlx_point_t *points, int count, sdlx_color_t color);
void sdlx_render_circle(int x_ctr, int y_ctr, int radius, int line_width, sdlx_color_t color);
void sdlx_render_fill_circle(int x_ctr, int y_ctr, int radius, sdlx_color_t color);
void sdlx_render_point(int x, int y, sdlx_color_t color, int point_size);
void sdlx_render_points(sdlx_point_t *points, int count, sdlx_color_t color, int point_size);

// Video: Textures
// ---------------

// Textures are memory buffers that can be selected as render target.
// When set as render target, the texture is the rendering_texture.
// Subsequent rendering occurs to the rendering_texture.

// For example, the Compass miniApp uses a texture to display the compass image:
// - initialization
//   . the compass image pixels are read from a png file
//   - sdlx_create_texture is called to create a texture of the same width/height as the png file
//   - sdlx_set_texture_pixels is called to copy the png file pixels to this new texture
// - run time
//   - the compass_heading is determined from the Android magnetic field sensor
//   - sdlx_render_texture_rotated is called to copy (scale & rotate) 
//     the compass texture to the display_texture 
// - termination
//   - sdlx_destroy_texture is called to free the GPU memory 

// Another way textures can be initialized is by using sdlx_set_render_target.
// For example, during initialization, create a 100x100 texture containing a yellow circle:
//    sdlx_texture_t *circle = sdlx_create_texture(100, 100);
//    sdlx_set_render_target(circle);  // sets rendering_texture to the created circle texture
//    sdlx_render_fill_circle(50, 50, 50, COLOR_YELLOW);
//    sdlx_set_render_target(NULL);    // restores rendering_texture to display_texture
// At runtime, the circle texture can be scaled to fill the entire display, the scaling
// will result in the circle being stretched to an ellipse:
//    sdlx_render_texture_ex1(circle, NULL, NULL);
//    The first NULL specifies that the entire circle texture is to be copied.
//    The second NULL specifies that the destination is the entire display area.
// Destroy the circle texture when the program terminates, freeing GPU memory:
//    sdlx_destroy_texture(circle);

// Create a texture with width w and height h.
sdlx_texture_t *sdlx_create_texture(int w, int h);
// Destroy a texture. Texture arg may be NULL.
void sdlx_destroy_texture(sdlx_texture_t *t);
// Query a texture for its width and height.
void sdlx_query_texture(sdlx_texture_t *t, int *w, int *h);
// Set all pixels of a texture to the specified color.
void sdlx_clear_texture(sdlx_texture_t *t, sdlx_color_t color);
// Adjust all pixels in the texture; r,g,b args each range from 0 to 1.
// For example, setting each of r,g,b to 0.5 would reduce the intensity by half.
void sdlx_color_mod_texture(sdlx_texture_t *t, float r, float g, float b);

// Copy pixels to the texture. The pixels arg must contain texture_w * texture_h pixels.
void sdlx_set_texture_pixels(sdlx_texture_t *t, unsigned int *pixels);
// Returns array containing the texture pixels; optionally returns texture width & height.
// Caller must free the returned pixels array.
unsigned int *sdlx_get_texture_pixels(sdlx_texture_t *t, int *w_optional, int *h_optional);

// Copy src_texture src_rect to the rendering_texture dest_rect.
// If src_rect is NULL, the entire src_texture is copied.
// If dest_rect is NULL, then the destination is the entire rendering_texture.
void sdlx_render_texture(sdlx_texture_t *src_texture, sdlx_loc_t *src_rect, sdlx_loc_t *dest_rect);

// Copy src_texture src_rect to the rendering_texture dest_rect.
// If src_rect is NULL, the entire src_texture is copied.
// If dest_rect is NULL, then the destination is the entire rendering_texture.
// The dest_rect is rotated by angle (degrees), about the center param.
// If the center param is NULL the rotation is about dest_rect.w/2, dest_rect.h/2.
// If the flip param is not FLIP_NONE, the flipping action is performed.
#define FLIP_NONE                     0
#define FLIP_HORIZONTAL               1
#define FLIP_VERTICAL                 2
#define FLIP_HORIZONTAL_AND_VERTICAL  3
void sdlx_render_texture_rotated(sdlx_texture_t *src_texture, sdlx_loc_t *src_rect, sdlx_loc_t *dest_rect,
                                 double angle, sdlx_point_t *center, int flip);

// Set rendering_texture.
// It the rendering_texture param is NULL, the rendering_texture 
// will be set to the display_texture.
void sdlx_set_render_target(sdlx_texture_t *rendering_texture);

// --------------------
// AUDIO
// --------------------

// Capabilities:
// - record from microphone to mp3 file
// - record from Android device to mp3 file
// - play tone sequence
// - play buffer of raw samples
// - play mp3 file
//
// The audio processing is performed in seperate threads.
// Thus calls to routines that play or record audio will initiate
// the operation, and return immedeately.
//
// Control routines are provided to stop, pause, and resume recording or playback.
//
// For routines that deal with audio samples, these samples are 
// float values, in the range -1.0 to +1.0.

// The frame rate is fixed at 48000 frames/sec.
// A frame contains 1 sample for mono; and 2 samples for stereo.
// A stereo frame caontains the left sample followed by the right sample.
#define FRAMES_PER_SEC 48000

// Values of sdlx_audio_state_t state field.
#define AUDIO_STATE_IDLE                0
#define AUDIO_STATE_PLAY_FILE           1
#define AUDIO_STATE_PLAY_TONES_SEQUENCE 2
#define AUDIO_STATE_PLAY_BUFF           3
#define AUDIO_STATE_RECORD_FROM_MIC     4
#define AUDIO_STATE_RECORD_FROM_DEVICE  5

// For call to sdlx_get_audio_samples, arg which_channel.
#define GET_SAMPLES_MONO          0
#define GET_SAMPLES_LEFT_CHANNEL  1
#define GET_SAMPLES_RIGHT_CHANNEL 2

// Used by sdlx_audio_play_tones.
typedef struct {
    short freq;
    short intvl_ms;
} sdlx_tone_t;

// Returned by sdlx_audio_get_state.
typedef struct {
    int    state;
    bool   stopping;
    bool   paused;
    int    play_current_secs;
    int    play_total_secs;
    int    record_secs;
    int    play_tones_freq;
    int    play_tones_seqnum;
    double volume;
} sdlx_audio_state_t;

// Audio: Control
// --------------

// Stop, pause, or resume in progress playback or recording.
int sdlx_audio_stop(void);
void sdlx_audio_pause(void);
void sdlx_audio_resume(void);

// Get a copy of the current audio state.
void sdlx_audio_get_state(sdlx_audio_state_t *state);

// Audio: Get Most Recent Samples
// ------------------------------

// Get copy of the most recent audio samples, either being played or recorded.
// - num_ret_samples: number of samples to return
// - num_downsample:  downsample interval, 
//                    use 1 to return all samples, 2 to return every other sample, etc
// - which_channel:   GET_SAMPLES_MONO, GET_SAMPLES_LEFT_CHANNEL, or GET_SAMPLES_RIGHT_CHANNEL
// - ret_samples:     array to hold the returned samples
//
// For an example, refer to the Color Organ miniApp (files/apps/ColrOrgn/color_organ.c).
void sdlx_get_audio_samples(int num_ret_samples, int num_downsample, int which_channel, float *ret_samples);

// Audio: Playback
// ---------------

// The sdlx_audio_play_file, sdlx_audio_play_buff, and sdlx_audio_play_tones
// routines return imedeately, the audio play is handled by a thread.

// Play an audio file.
// This routine uses SDL_mixer routines to play the file.
// The specified file would usually be an mp3 file, because that is the file type
//  created by sdlx_audio_record_from_mic and sdlx_audio_record_from_device.
// However it is possible to play a file that is in any format supported
//  by SDL_mixer. SDL_mixer supported formats: MP3, WAV, Ogg Vorbis, FLAC, Opus, MIDI.
int sdlx_audio_play_file(char *dir, char *filename);

// When playing a file, the sdlx_audio_set_play_file_time routine will set the
// file position to 'secs'.
void sdlx_audio_set_play_file_time(int secs);

// Play a sequence of tones and gaps, as specified in the tones array.
// The tones array must be terminated with an entry containing intvl_ms=0.
// Use 'freq' equal 0 for a gap.
int sdlx_audio_play_tones(sdlx_tone_t *tones);

// Play a buffer containing raw audio samples.
// - samples:                array of raw audio samples; 
//                           interleaved left,right when num_channels=2 (stereo);
//                           sample value range is -1.0 to 1.0
// - num_samples:            number of samples in the array
// - num_channels:           number of channels, 1=mono, 2=stereo
// - loops:                  play the buffer 'loops' times
// - free_samples_when_done: when set, the samples buffer will be freed
// For example:
// - 1 second mono:   num_samples=48000, num_channes=1
// - 1 second stereo: num_samples=96000, num_channes=2
int sdlx_audio_play_buff(float *samples, int num_samples, int num_channels,
                         int loops, bool free_samples_when_done);

// Audio: Record
// -------------

// Record an mp3 file from the Android microphone.
// - dir/filename:   path of the mp3 file to create
// - audo_stop_secs: if greater than 0 the recording will automatically stop
//                   following this number of seconds of silence
// - append:         when set, the recording is appended to the specified dir/filename
// - start_paused:   if set the recording state is initially paused;
//                   a subsequent call to sdlx_audio_resume is needed to start recording
int sdlx_audio_record_from_mic(char *dir, char *filename, int auto_stop_secs, bool append, bool start_paused);

// Record an mp3 file from the Android device; for example record from YouTube Music.
// - dir/filename:   path of the mp3 file to create
// - append:         when set, the recording is appended to the specified dir/filename
// - start_paused:   if set the recording state is initially paused;
//                   a subsequent call to sdlx_audio_resume is needed to start recording
int sdlx_audio_record_from_device(char *dir, char *filename, bool append, bool start_paused);

// Audio: Create Test File
// -----------------------

// Create an mp3 file containing a frequency sweep from freq1 to freq2 of duration_secs.
// The filename must have '.mp3' extension.
// The created file is stereo.
void sdlx_create_test_file(char *dir, char *filename, int freq1, int freq2, int duration_secs);

// --------------------
// SENSORS
// --------------------

// Sensors: High-Level Sensor Access
// ---------------------------------

// These routines read the Android device sensors.
// The return value is 0 for success, and -1 for failure. 
// When returning -1 for failure, the returned arg values are INVALID_NUMBER.

// Provides number of steps since the Android device was booted.
int sdlx_sensor_read_step_counter(unsigned long *step_count);

// The magnetic sensor is read, this sensor provide magnetic field stength in the x,y,z directions.
// The magnetic heading of the device is then calculated based on these 3 field strength values,
// adjusting for the device roll & pitch.
// The device magnetic heading is provided in range 0 to 359.999 degrees,
// referenced to the top of the device.
int sdlx_sensor_read_mag_heading(double *mag_heading);

// Read gravity accelerometer sensor. Units are m/s^2.
// - x-axis: left to right
// - y-axis: bottom to top
// - z-axis: perpendicular to the screen pointing to user
int sdlx_sensor_read_gravity_accel(double *ax, double *ay, double *az);

// Read device acceleration sensor; this is the acceleration of the device
// with the acceleration due to gravity removed. Units are m/s^2.
// - x-axis: left to right
// - y-axis: bottom to top
// - z-axis: perpendicular to the screen pointing to user
int sdlx_sensor_read_device_accel(double *ax, double *ay, double *az);

// The gravity accelerometer sensor is read.
// The device roll & pitch are calculated from the gravity acceleration sensor values.
// The roll and pitch values are in degrees.
// When the device is horizontal, the roll and pitch are 0.
// Roll is positive when the right side of the device is lowered.
// Pitch is positive when the top of the device is raised.
int sdlx_sensor_read_roll_pitch(double *roll, double *pitch);

// Provides  atmospheric pressure in millibars.
// Standard atmospheric pressure at sea level is 1,013.25 millibars.
int sdlx_sensor_read_pressure(double *millibars);

// Sensors: Low-Level Sensor Access
// --------------------------------

// Android device sensors each have:
// - id:   identifies the sensor
// - type  the type of sensor
// - name: name of the sensor

// The sensor type values can be found here:
//   ~/android/sdk/ndk/*/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/android/sensor.h
// or in the "NOTES" section of src/ezApp_lib/sdlx_sensor.c.

typedef struct {
    int   id;
    int   type;
    char *name;
} sdlx_sensor_info_t;

// Returns table of sdlx_sensor_into_t.
// For an example see: files/apps/Test/test.c "PAGE 8: SENSOR INFO TBL".
// Do not free the returned sensor info table.
sdlx_sensor_info_t *sdlx_sensor_get_info_tbl(int *max);

// Search for a sensor of the type specified.
// Returns -1 if not found; otherwise the id of the sensor found is returned.
int sdlx_sensor_find(int type);

// Read the raw data from the specified sensor id.
// Most sensors return float values; except for the step_counter sensor,
// which returns uint64_t. To read the step_counter with this routine:
//   unsigned long step_count;
//   sdlx_sensor_read_raw(id_step_counter, (float*)&step_count, 2);
int sdlx_sensor_read_raw(int id, float *data, int num_values);

// --------------------
// EVENTS   
// --------------------

// Define common events:
// - EVID_MOTION: This event occurs when the display is tapped and dragged.
//                The current x,y coordinates; and the relative motion in the x,y
//                directions are returned by the call to sdlx_get_event.
// - EVID_PINCH:  This event occurs, on the Android device, when the user
//                performs a two finger pinch action .
// - EVID_QUIT:   This event is usually registered by calling 
//                  sdlx_register_control_events(..., EVID_QUIT, "X");
//                When the "X" is tapped, the EVID_QUIT event occurs.
#define EVID_MOTION  1000000
#define EVID_PINCH   1000001
#define EVID_QUIT    1000009

// This structure returns the event that occurred, by call to sdlx_get_event.
// When the display is tapped at a location associated with a registered
// event, the registered event_id is returned.
// If the EVID_MOTION event is registered, and the display is dragged, then 
// the EVID_MOTION event is returned, along with the values contained in 
// the embedded motion struct.
typedef struct {
    int event_id;
    union {
        struct {
            double x, y, xrel, yrel;
        } motion;
        struct {
            double scale, span_x, span_y, focus_x, focus_y;
        } pinch;
        struct {  // the u.private_keybd struct is not available in picoc
            unsigned int keycode;
        } private_keybd;
    } u;
} sdlx_event_t;

// Events: Registration
// --------------------

// Register an event.
// If EVID_MOTION is being registered then loc should be NULL.
// Otherwise, a miniApp defined event is being registered, the display
// location associated with the event is supplied by the caller in the loc arg.
// The value of event_id for miniApp defined events should be in range 1 - 9999.
// Sdlx_register_event must be called for every display update, and called
// following sdlx_display_init.
void sdlx_register_event(sdlx_loc_t *loc, int event_id);

// Register control area events. The control area is the Teal bar at the bottom
// of the display. This bar is below the sdlx_win_height; and can only be 
// rendered as a result of calling sdlx_register_control_events.
// Up to 3 events can be registered in the control area.
// Pass NULL for evstr1,2,3 to not register that event.
// Usually evid3,evstr3 are set to EVID_QUIT,"X".
void sdlx_register_control_events(int evid1, char *evstr1,
                                  int evid2, char *evstr2,
                                  int evid3, char *evstr3);

// Events: Wait For Event
// ----------------------

// Wait for an event, if timeout occurs then event->event_id is set to -1.
// Timeout_usecs values:
//  -1:  wait forever
//   0:  don't wait
//  >0:  timeout interval in microsecs
void sdlx_get_event(long timeout_usecs, sdlx_event_t *event);

// --------------------
// MISC
// --------------------

// Display an Android 'Toast' popup message.
void sdlx_show_toast(char *message);

// Read input text string from user.
// The return value points to a static variable, and must not be freed.
// When numeric_keybd is true the Android numeric keyboard is displayed,
// when false, the full keyboard is displayed.
// The prompt arg can contain newline characters.
char *sdlx_get_input_str(char *prompt_optional, bool numeric_keybd, char *dflt_input_str_optional);

#ifdef __cplusplus
}
#endif

#endif
