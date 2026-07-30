#ifndef __PRIVATE_H__
#define __PRIVATE_H__

// The declarations in this file (private.h) are not available in picoc.

#ifdef __cplusplus 
extern "C" {
#endif

#define ATTRIBUTE_UNUSED __attribute__((unused))

// --------------------
// logging.c
// --------------------

#define INFO(fmt, args...) \
    do { \
        log_msg("I", __func__, fmt, ## args); \
    } while (0)
#define ERROR(fmt, args...) \
    do { \
        log_msg("E", __func__, fmt, ## args); \
    } while (0)

int log_init(void);
void log_msg(const char * lvl, const char * func, const char * fmt, ...) __attribute__ ((format (printf, 3, 4)));

// --------------------
// sdlx_video.c
// --------------------

extern double scale_events_x;
extern double scale_events_y;
extern int    orientation;
extern int    logical_win_width, logical_win_height;
extern int    logical_win_width_portrait, logical_win_height_portrait;
extern int    logical_win_width_landscape, logical_win_height_landscape;

int sdlx_video_init(void);
void sdlx_video_quit(void);
void sdlx_minimize_window(void);

// --------------------
// sdlx_audio.c
// --------------------

#ifdef ANDROID
    #define DEFAULT_RECORD_GAIN 5
#else
    #define DEFAULT_RECORD_GAIN 1
#endif
#define DEFAULT_RECORD_SILENCE 0.1

typedef struct {
    double record_gain;
    double record_silence;
} sdlx_audio_params_t;

int sdlx_audio_init(void);
void sdlx_audio_quit(void);
void sdlx_audio_main_thread_periodic(void);
void sdlx_audio_set_params(sdlx_audio_params_t *ap);
void sdlx_audio_get_params(sdlx_audio_params_t *ap);

// --------------------
// sdlx_sensor.c
// --------------------

int sdlx_sensor_init(void);
void sdlx_sensor_quit(void);

// --------------------
// sdlx_event.c
// --------------------

#define EVID_KEYBD   10002
#define CONTROL_AREA_SIZE 150

void sdlx_reset_events(void);
void sdlx_event_box_ctrl(bool event_box_enable);

// --------------------
// sdlx_misc.c
// --------------------

#define SUBSYS_VIDEO  1
#define SUBSYS_AUDIO  2
#define SUBSYS_SENSOR 4

int sdlx_init(int subsys);
void sdlx_quit(int subsys);
char *sdlx_get_storage_path(void);
void sdlx_copy_asset_file(char *asset_filename, char *dest_dir);
int sdlx_get_permission(char *name);
int sdlx_create_detached_thread(int (*thread_fn)(void*), char *thread_name, void *cx);

// --------------------
// svc.c
// --------------------

void svcs_start_all(void);
void svcs_stop_all(void);
void svcs_display(int bg_color);

// --------------------
// utils_android.cpp
// --------------------

void util_android_utils_init(void);
void util_android_utils_destroy(void);
void util_start_foreground(void);
void util_stop_foreground(void);
bool util_is_foreground_enabled(void);

// ----------------------
// utils.c  openssl support
// ----------------------

#define SSL_TEXTLEN 128

typedef struct {
    unsigned char nonce[12];
    unsigned char tag[16]; 
    unsigned char ciphertext[SSL_TEXTLEN];
} ssl_payload_t;

unsigned char *ssl_keygen(char *password);
int ssl_encrypt(unsigned char *key, char *plaintext, ssl_payload_t *payload);
int ssl_decrypt(unsigned char *key, ssl_payload_t *payload, char **plaintext);

// --------------------
// run.c
// --------------------

int run(char *name, bool is_svc);

#ifdef __cplusplus
}
#endif

#endif
