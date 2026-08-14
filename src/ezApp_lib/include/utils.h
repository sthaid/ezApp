#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

// --------------------
// TIME UTILS
// --------------------

#define MAX_TIME_STR 30

// Returns monotonic time.
long util_microsec_timer(void);

// Returns real time, in microsecs since the Unix Epoch.
long util_get_real_time_microsec(void);

// Converts real time in microsecs to time str, 
// str arg should have size MAX_TIME_STR or larger.
// For example, when gmt, display_ms, and display_data are all true:
// "07/24/26 12:15:39.173 GMT"
char *util_time2str(char *str, long us, int gmt, int display_ms, int display_date);

// --------------------
// FILE UTILS
// --------------------

// The ezApp Current Working Directory (CWD) is always the files directory.
//
// The following routines create the pathname by catenating the dir and fn args;
// and then performs their operation.
//
// Notes:
// - either dir or fn can be NULL
// - caller of util_read_file must free the returned ptr
// - util_read_file adds an extra '\0' char to the end of the data buffer;
//   this extra char is not included in the returned file length
// - util_file_mtime: returns modification time in Unix Epoch microsecs

int util_write_file(char *dir, char *fn, void *data, int len);
void *util_read_file(char *dir, char *fn, int *len_optional);
void util_delete_file(char *dir, char *fn);
void util_rename_file(char *old_dir, char *old_fn, char *new_dir, char *new_fn);
bool util_file_exists(char *dir, char *fn);
long util_file_mtime(char *dir, char *fn);
long util_file_size(char *dir, char *fn);

// --------------------
// DIRECTORY UTILS
// --------------------

// mkdir -p <dir>/<dir_to_create>
void util_create_dir(char *dir, char *dir_to_create);

// rm -rf <dir>/<dir_to_delete>
void util_delete_dir(char *dir, char *dir_to_delete);

// --------------------
// MAP FILE UTILS
// --------------------

// Map file in to program address space.
// Caller must call util_unmap_file when miniApp or miniSvc terminates, or
// when the mapping is no longer needed.
// Args:
// - dir, file: are catenated to form pathname
// - len: length of the mapping to create
// - create_if_needed: if pathname does not exist a zero filled file is 
//   first created, then mapped
// - created_flag: set to true if the file was created
void *util_map_file(char *dir, char *file, int len, bool create_if_needed, int *created_flag_optional);

// Remove the mapping, and flush changes to filesystem.
// - addr: the address returned by util_map_file
// - len: the length passed to util_map_file
void util_unmap_file(void *addr, int len);

// Flush changes made to the memory copy of the file to the filesystem.
// Calling util_sync_file periodically helps to ensure that data is not lost;
// for example, if the program were to crash.
void util_sync_file(void *addr, int len);

// --------------------
// GET/SET PARAMS UTILS
// --------------------

// MiniApps and miniSvcs can save values to a 'params' file.
//
// For example, the Morse miniApp saves the 'wpm' setting in 'apps/Morse/params'.
// The Morse params file will contain "wpm = 10.000", when the Morse miniApp is
// configured for Morse Code at 10 Words Per Minute.
//
// Notes:
// - numeric params are written to the params file in "%0.3f" format.
// - callers of util_get_str_param must free the returned string
// - when getting a param that does not yet exist, the param is 
//   created with caller supplied default_value
// - util_print_params: debug prints the defined params

char *util_get_str_param(char *dir, char *param_name, char *param_default_value);
void util_set_str_param(char *dir, char *param_name, char *param_new_value);

double util_get_numeric_param(char *dir, char *param_name, double param_default_value);
void util_set_numeric_param(char *dir, char *param_name, double param_new_value);

void util_print_params(char *dir);

// --------------------
// NETWORK UTILS    
// --------------------

// Util_get_ipaddr returns the device IP address, in the caller supplied str.
// Recommended sizeof str is >= 30.
char *util_get_ipaddr(char *str); 

// --------------------
// JSON FILE UTILS   
// --------------------

// The json utils provide a simple API for parsing json, making use 
// of the https://github.com/DaveGamble/cJSON json parser.
//
// First call util_json_parse, passing in the json_text. if the json_text 
// contains multiple blocks of json, the end_ptr will return the location of
// the next block. Util_json_parse returns opaque json_root.
//
// Next call util_json_get_value, repeatedly, to extract the values of interest.
//
// When done, call util_json_free(json_root).
//
// Refer to examples/json.c

#define JSON_TYPE_UNDEFINED 0
#define JSON_TYPE_FLAG      1
#define JSON_TYPE_NUMBER    2
#define JSON_TYPE_STRING    3
#define JSON_TYPE_ARRAY     4
#define JSON_TYPE_OBJECT    5

typedef struct {
    int type;
    union {
        bool   flag;
        double number;
        char  *string;
        void  *array;
        void  *object;
    } u;
} json_value_t;

void *util_json_parse(char *str, char **end_ptr_optional);
json_value_t *util_json_get_value(void *json_item, ...);
void util_json_free(void *json_root);

// --------------------
// PNG FILE UTILS   
// --------------------

// These routines read/write 32-bit RGBA png files.
// The https://github.com/lvandeve/lodepng PNG encoder/decoder is used.
//
// Callers of util_read_png_file must free pixels.

int util_read_png_file(char *dir, char *filename, unsigned char **pixels, int *w, int *h);
int util_write_png_file(char *dir, char *filename, unsigned char *pixels, int w, int h);

// --------------------
// FFT
// --------------------

// These util_fft routines perform Fast Fourier Transforms
// The https://github.com/mborgerding/kissfft package is used.

typedef struct {
    float r;
    float i;
} complex_t;

void util_fft_real_to_real(int n_fft, float *input, float *output, bool scale_by_n_fft);
void util_fft_real_to_complex(int n_fft, float *input, complex_t *cpx_output, bool scale_by_n_fft);
void util_fft_inverse_complex_to_real(int n_fft, complex_t *cpx_input, float *output, bool scale_by_n_fft);

// Used for unit_test, by the Test miniApp. Returns 0 on success.
// Returns duration in microsecs of 1000 sample real-to-real fft.
int util_fft_test(long *duration_usec);

// The util_rms routines computes the RMS magnitude of the supplied data.
double util_rms_float(float *x, int n);

// ----------------------
// CALL ANDROID JAVA CODE
// ----------------------

// The routines in this section work when run on the Android device.
// These routines are stubs when ezApp is being tested on Linux.
//
// To perform the functions, code in utils_android.cpp makes calls to NDK routines,
// such as GetMethodID and CallDoubleMethod to access the ezApp SDL java code extensions.

// Get location: latitude, longitude, and altitude.
// Prior to Android 14 (API level34) Android did not support converting GPS altitude from
// WGS84 to Mean Sea Level (MSL); in this case the less accurate WGS84 altitude is provided..
// The returned alt_is_wgs84 flag indicates whether the altitude is WGS84 or MSL.
void util_get_location(double *latitude_optional, double *longitude_optional, 
                       double *altitude_ft_optional, bool *alt_is_wgs84_optional);

// Android text-to-speech.
void util_text_to_speech(char *text);
void util_text_to_speech_stop(void);

// Control flashlight.
void util_turn_flashlight_on(void);
void util_turn_flashlight_off(void);
bool util_is_flashlight_on(void);
void util_toggle_flashlight(void);

// Capture device audio:
// 1) call util_start_playbackcapture
// 2) repeatedly call util_get_playbackcapture_audio to obtain raw audio samples.
// 3) when done collecting audio samples, call util_stop_playbackcapture
int util_start_playbackcapture(void);
void util_stop_playbackcapture(void);
int util_get_playbackcapture_audio(float *array, int num_array_elements);


// Camera xxx more comments needed
int util_take_picture(void);
int util_decode_jpeg_to_raw(int fd, int* out_width, int* out_height, void** out_pixels); //xxx move or update comment

#ifdef __cplusplus
}
#endif

#endif
