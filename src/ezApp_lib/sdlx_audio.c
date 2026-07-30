#include <std_hdrs.h>

#include <sdlx.h>
#include <utils.h>
#include <private.h>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <lame.h>

// Capabilities:
// - record from microphone to mp3 file
// - record Android device audio to mp3 file
// - play tone sequence
// - play buffer of raw samples
// - play mp3 file

//
// defines
//

#define TWO_MS  2000

#define FRAMES_PER_SEC 48000
#define FRAMES_PER_MS  (FRAMES_PER_SEC/1000)

#define MP3_LAME_MODE_STEREO           0
#define MP3_LAME_MODE_JOINT_STEREO     1
#define MP3_LAME_MODE_DUAL_CHANNEL     2
#define MP3_LAME_MODE_MONO             3

#define MAX_RECORD_SECS (12 * 3600)

//
// typedefs
//

//
// variables
//

static sdlx_audio_state_t  state;
static sdlx_audio_params_t audio_params = { DEFAULT_RECORD_GAIN, DEFAULT_RECORD_SILENCE };

static SDL_AudioStream    *audio_stream_rec_mono;
static SDL_AudioStream    *audio_stream_play_mono;
static SDL_AudioStream    *audio_stream_play_stereo;
static SDL_AudioStream    *Audio_stream;

static MIX_Mixer          *mixer_dev;
static MIX_Track          *mixer_track;
static MIX_Audio          *mixer_audio;

//
// prototypes
//

static void set_state_idle(void);

static double calc_volume(float *samples, int n);

static void *mp3_file_open(char *dir, char *filename, int num_channels, bool append);
static void mp3_file_write(void *cx_arg, float *samples, int num_samples);
static void mp3_file_close(void *cx_arg);

// -----------------  INITIALIZE  ---------------------------------

static void audio_print_list_of_devices(void);

int sdlx_audio_init(void)
{
    INFO("initializing\n");

    // initialize SDL audio
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        ERROR("SDL_Init AUDIO failed, %s\n", SDL_GetError());
        return -1;
    }

    // initializing SDL_mixer
    if (!MIX_Init()) {
        ERROR("MIX_Init failed, %s\n", SDL_GetError());
        return -1;
    }

    // print list of audio devices
    audio_print_list_of_devices();

    // success
    return 0;
}

void sdlx_audio_quit(void)
{
    INFO("quitting\n");

    // stop audio
    sdlx_audio_stop();

    // quit mixer
    MIX_Quit();

    // quit SDL audio subsystem
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

static void audio_print_list_of_devices(void)
{
    SDL_AudioDeviceID *devid;
    int i, count;
    const char *name;

    devid = SDL_GetAudioPlaybackDevices(&count);
    INFO("num playback devices = %d\n", count);
    for (i = 0; i < count; i++) {
        name = SDL_GetAudioDeviceName(devid[i]);
        INFO("  playback dev %d = %s\n", devid[i], name);
    }
    SDL_free(devid);

    devid = SDL_GetAudioRecordingDevices(&count);
    INFO("num recording devices = %d\n", count);
    for (i = 0; i < count; i++) {
        name = SDL_GetAudioDeviceName(devid[i]);
        INFO("  recording dev %d = %s\n", devid[i], name);
    }
    SDL_free(devid);
}

// -----------------  CONTROL & STATE  ----------------------------

void sdlx_audio_get_state(sdlx_audio_state_t *x)
{
    *x = state;
}

int sdlx_audio_stop(void)
{
    // if audio is idle then return success
    if (state.state == AUDIO_STATE_IDLE) {
        return 0;
    }

    // set audio state to stopping, 
    // this will stop in progress: record_mic, record_dev, or play_buff
    state.stopping = true;

    // if SDL Mixer is playing a track then stop it,
    // this will stop in progress: play_file
    if (mixer_track) {
        int fade_out_frames = 0;
        MIX_StopTrack(mixer_track, fade_out_frames);
    }

    // wait for in progress audio to stop
    int ms = 0;
    while (state.state != AUDIO_STATE_IDLE) {
        usleep(TWO_MS);
        ms += 2;

        if (ms > 5000) {
            ERROR("failed to stop\n");
            state.stopping = false;
            return -1;
        }
    }

    // return success
    return 0;
}

void sdlx_audio_pause(void)
{
    // if audio is idle, or is already paused, then return
    if (state.state == AUDIO_STATE_IDLE || state.paused) {
        return;
    }

    // if play_file track is running then pause the track
    if (mixer_track) {
        MIX_PauseTrack(mixer_track);
    }

    // pause the audio_stream, except if recording from mic;
    // when recording from mic the stream is left running so that 
    // the mic volume and sampling will continue
    if (Audio_stream) {
        if (state.state != AUDIO_STATE_RECORD_FROM_MIC) {
            SDL_PauseAudioStreamDevice(Audio_stream);  
        }
    }

    // set state to paused, and volume to 0;
    state.paused = true;
    if (state.state != AUDIO_STATE_RECORD_FROM_MIC && 
        state.state != AUDIO_STATE_RECORD_FROM_DEVICE) 
    {
        state.volume = 0;
    }
}

void sdlx_audio_resume(void)
{
    if (!state.paused) {
        return;
    }

    if (mixer_track) {
        MIX_ResumeTrack(mixer_track);
    } else if (Audio_stream) {
        SDL_ResumeAudioStreamDevice(Audio_stream);  
    }

    state.paused = false;
}

// Setting audio state to idle is done from the threads.
// To reduce chance of a race condition between the threads setting
// the state to idle, and the main thread observing that it is idle,
// the following steps are used:
// - the state structure is first cleared, except for the state.state field,
//   which is not changed
// - sync_synchronize is called to ensure memory write ordering
// - finally state.state is set to AUDIO_STATE_IDLE
static void set_state_idle(void)
{
    sdlx_audio_state_t tmp_state;

    memset(&tmp_state, 0, sizeof(tmp_state));
    tmp_state.state = state.state;
    state = tmp_state;

    __sync_synchronize();

    state.state = AUDIO_STATE_IDLE;
}

// -----------------  PARAMS  -------------------------------------

void sdlx_audio_set_params(sdlx_audio_params_t *ap)
{
    audio_params = *ap;
}   

void sdlx_audio_get_params(sdlx_audio_params_t *ap)
{   
    *ap = audio_params;
}

// -----------------  SAVE / GET AUDIO SAMPLES  ----------

#define MAX_AUDIO_SAMPLES 65536
typedef struct {
    unsigned long total;
    float         left[MAX_AUDIO_SAMPLES];
    float         right[MAX_AUDIO_SAMPLES];
    float         mono[MAX_AUDIO_SAMPLES];
} sdlx_audio_samples_t;

sdlx_audio_samples_t audio_samples;

void save_audio_samples(float *samples, int num_samples, int num_channels)
{
    float *l, *r, *m, *l_end;
    int   idx, i;

    // init
    idx = (audio_samples.total % MAX_AUDIO_SAMPLES);
    l = &audio_samples.left[idx];
    r = &audio_samples.right[idx];
    m = &audio_samples.mono[idx];
    l_end = audio_samples.left + MAX_AUDIO_SAMPLES;

    // processing differs based on num_channels
    if (num_channels == 1) {
        // mono case: set left, right, and mono values all the same
        for (i = 0; i < num_samples; i++) {
            *l++ = *r++ = *m++ = samples[i];
            if (l == l_end) {
                l = audio_samples.left;
                r = audio_samples.right;
                m = audio_samples.mono;
            }
        }
    } else if (num_channels == 2) {
        // stereo case: set left and right from the interleaved input;
        // set mono value to the average of left and right
        for (i = 0; i < num_samples; i += 2) {
            *l++ = samples[i];
            *r++ = samples[i+1];
            *m++ = (samples[i] + samples[i+1]) / 2;
            if (l == l_end) {
                l = audio_samples.left;
                r = audio_samples.right;
                m = audio_samples.mono;
            }
        }
    } else { 
        ERROR("invalid num_channels %d\n", num_channels);
        return;
    }

    // update the total number of samples saved
    __sync_synchronize();
    audio_samples.total += (num_samples / num_channels);
}

void sdlx_get_audio_samples(int num_ret_samples, int num_downsample, int which_channel, float *ret_samples)
{
    int   idx;
    float *x;

    // determine the starting idx of the saved samples to retrieve
    idx = audio_samples.total - 1 - ((num_ret_samples-1) * num_downsample);
    idx = ((idx + MAX_AUDIO_SAMPLES) % MAX_AUDIO_SAMPLES);

    // set 'x' pointer to the samples array that the caller has chosen
    if (which_channel == GET_SAMPLES_LEFT_CHANNEL) {
        x = audio_samples.left;
    } else if (which_channel == GET_SAMPLES_RIGHT_CHANNEL) {
        x = audio_samples.right;
    } else {
        x = audio_samples.mono;
    }

    // copy and downsamle the saved samples to the caller supplied ret_samples array
    for (int i = 0; i < num_ret_samples; i++) {
        ret_samples[i] = x[idx];
        idx += num_downsample;
        if (idx >= MAX_AUDIO_SAMPLES) {
            idx -= MAX_AUDIO_SAMPLES;
        }
    }
}
            
// -----------------  UTILS  -----------------------------

#define VOLUME_SCALE_FACTOR 1.5
static double calc_volume(float *samples, int n)
{   
    double sum_squares = 0;
    double volume;

    // calculate volume using RMS value of samples 
    for (int i = 0; i < n; i++) {
        sum_squares += (samples[i] * samples[i]);
    }
    volume = sqrt(sum_squares / n) * VOLUME_SCALE_FACTOR;

    // limit volume to max value 1
    if (volume > 1) volume = 1;

    // return volume
    return volume;
}

static char *concat_dir_and_filename(char *dir, char *fn, char *path)
{
    if (dir && fn) {
        sprintf(path, "%s/%s", dir, fn);
    } else if (dir) {
        strcpy(path, dir);
    } else if (fn) {
        strcpy(path, fn);
    } else {
        path[0] = '\0';
    }

    return path;
}

void sdlx_audio_main_thread_periodic(void)
{
    static time_t t_idle_start;
    time_t        t_idle_duration;

    // This routine is called periodically on the main thread.
    // If audio resources are allocated, and audio state has been idle for 30 seconds,
    // then the audio resources are destroyed.
    // If the audio resources are not destroyed then java threads SDLAudioPnn or SDLAudioCnn
    // would persist, and these threads use significant CPU resources; which would discharge
    // the device battery.

    // if nothing is allocated then return
    if (audio_stream_rec_mono == NULL &&
        audio_stream_play_mono == NULL &&
        audio_stream_play_stereo == NULL &&
        mixer_audio == NULL &&
        mixer_track == NULL &&
        mixer_dev == NULL)
    {
        t_idle_start = 0;
        return;
    }

    // determine duration of the audio idle state
    if (state.state == AUDIO_STATE_IDLE) {
        if (t_idle_start == 0) {
            t_idle_start = time(NULL);
        }
        t_idle_duration = time(NULL) - t_idle_start;
    } else {
        t_idle_start = 0;
        t_idle_duration = 0;
    }

    // destroy allocations if audio state has been idle for 30 seconds
    if (t_idle_duration > 30) {
        if (audio_stream_rec_mono) {
            SDL_DestroyAudioStream(audio_stream_rec_mono);
            audio_stream_rec_mono = NULL;
        }
        if (audio_stream_play_mono) {
            SDL_DestroyAudioStream(audio_stream_play_mono);
            audio_stream_play_mono = NULL;
        }
        if (audio_stream_play_stereo) {
            SDL_DestroyAudioStream(audio_stream_play_stereo);
            audio_stream_play_stereo = NULL;
        }

        if (mixer_audio) {
            MIX_DestroyAudio(mixer_audio); 
            mixer_audio = NULL; 
        }
        if (mixer_track) {
            MIX_DestroyTrack(mixer_track);
            mixer_track = NULL; 
        }
        if (mixer_dev) {
            MIX_DestroyMixer(mixer_dev);
            mixer_dev = NULL;
        }
    }
}

// -----------------  CREATE MP3 FILE  -------------------

// MP3 file is created using the LAMS MP3 Encoder.
//
// "LAME ("LAME Ain't an MP3 Encoder") is a widely used, open-source audio 
// encoding library and command-line tool designed to convert audio files 
// into high-quality MP3 format. As a free software released in 1998, it is 
// considered the industry standard for creating high-fidelity MP3s, offering
// better quality and speed than most other encoders."
//
// Lame is initialized in sdlx_audio_init.
// - num_channels   = 2
// - frames_per_sec = 48000
// - bit rate       = 128 kbps
// - mode           = joint stereo
// - quality        = 2  (high)

#define MAX_SAMPLES_MP3_FILE_WRITE 8192
#define MAX_MP3_BUF (MAX_SAMPLES_MP3_FILE_WRITE / 2 + 7200)

typedef struct {
    int                fd;
    lame_global_flags *gfp;
    unsigned char      mp3buf[MAX_MP3_BUF];
} mp3_file_cx_t;

static void *mp3_file_open(char *dir, char *filename, int num_channels, bool append)
{
    char           path[200];
    int            fd, len;
    mp3_file_cx_t *cx;

    // only num_channels equal 2 is supported
    if (num_channels != 2) {
        ERROR("num_channels=%d, must be 2\n", num_channels);
        return NULL;
    }

    // create pathname, verify suffix is ".mp3"
    concat_dir_and_filename(dir, filename, path);
    len = strlen(path);
    if (len < 5 || strcmp(path+len-4, ".mp3") != 0) {
        ERROR("mp3 extension required\n");
        return NULL;
    }

    // open mp3 file, set open flags to truncate file when not appending
    if (!append) {
        fd = open(path, O_RDWR|O_CREAT|O_TRUNC, 0666);
        if (fd < 0) {
            ERROR("failed to create '%s', %s\n", path, strerror(errno));
            return NULL;
        }
    } else {
        fd = open(path, O_RDWR|O_CREAT, 0666);
        if (fd < 0) {
            ERROR("failed to open for append '%s', %s\n", path, strerror(errno));
            return NULL;
        }
        lseek(fd, 0, SEEK_END);
    }

    // initialize lame
    lame_global_flags *gfp;
    gfp = lame_init();
    if (gfp == NULL) {
        ERROR("lame_init failed\n");
        close(fd);
        unlink(path);
        return NULL;
    }

    lame_set_num_channels(gfp,2);
    lame_set_in_samplerate(gfp,FRAMES_PER_SEC);
    lame_set_brate(gfp,128);
    lame_set_mode(gfp,MP3_LAME_MODE_JOINT_STEREO);
    lame_set_quality(gfp,2);   // 2=high  5 = medium  7=low
    if (lame_init_params(gfp) == -1) {
        ERROR("lame_init_params failed\n");
        close(fd);
        unlink(path);
        lame_close(gfp);
        return NULL;
    }

    // allocate and init cx
    cx = calloc(1, sizeof(mp3_file_cx_t));
    cx->fd = fd;
    cx->gfp = gfp;

    // return cx
    return cx;
}

// args:
// - samples are interleaved left, right channel
// - there are 2 samples per frame (STEREO)
// - num_samples must be multiple of 2
static void mp3_file_write(void *cx_arg, float *samples, int num_samples)
{
    int len;
    int process_samples;
    mp3_file_cx_t *cx = cx_arg;
    short samples_s16[MAX_SAMPLES_MP3_FILE_WRITE];

    // if mp3 file is not open then just return
    if (cx_arg == NULL) {
        return;
    }

    // num_samples must be a mulitple of 2
    if (num_samples & 1) {
        ERROR("num_samples %d, must be multiple of 2\n", num_samples);
        return;
    }

    // loop until all samples have been processed
    while (num_samples) {
        process_samples = (num_samples > MAX_SAMPLES_MP3_FILE_WRITE ? MAX_SAMPLES_MP3_FILE_WRITE : num_samples);

        // libmp3lame requires s16 format
        for (int i = 0; i < process_samples; i++) {
            samples_s16[i] = samples[i] * 32767;
        }

        // note: 3rd arg is number of samples per channel, thus the divide by 2
        len = lame_encode_buffer_interleaved(cx->gfp, samples_s16, process_samples/2, cx->mp3buf, MAX_MP3_BUF);
        if (len < 0) {
            ERROR("lame_encode_buffer failed, rc=%d\n", len);
            return;
        }

        if (len > 0) {
            write(cx->fd, cx->mp3buf, len);
        }

        samples += process_samples;
        num_samples -= process_samples;
    }
}

static void mp3_file_close(void *cx_arg)
{
    int len;
    mp3_file_cx_t *cx = cx_arg;

    // if mp3 file is not open then just return
    if (cx_arg == NULL) {
        return;
    }

    while (true) {
        // flush lame internal buffers
        len = lame_encode_flush(cx->gfp, cx->mp3buf, MAX_MP3_BUF);
        if (len < 0) {
            ERROR("lame_encode_flush failed, rc=%d\n", len);
            break;
        }

        // if no remaining data then break
        if (len == 0) {
            break;
        }

        // write mp3 data
        if (len > 0) {
            write(cx->fd, cx->mp3buf, len);
        }
    }

    // cleanup
    close(cx->fd);
    lame_close(cx->gfp);
    free(cx);
}

// -----------------  RECORD MICROPHONE  ------------------

typedef struct {
    char dir[100];
    char filename[100];
    bool append;
    int  auto_stop_secs;
} record_mic_cx_t;

static int record_mic_thread(void *cx_arg);

int sdlx_audio_record_from_mic(char *dir, char *filename, int auto_stop_secs, bool append, bool start_paused)
{
    record_mic_cx_t     *cx = NULL;
    const SDL_AudioSpec  record_spec = { SDL_AUDIO_F32, 1, FRAMES_PER_SEC };

    // stop audio 
    if (sdlx_audio_stop() != 0) {
        ERROR("failed to stop audio\n");
        return -1;
    }

    // if filename provided it must have mp3 extension
    if (filename && strstr(filename, ".mp3") == NULL) {
        ERROR("filename '%s' must have mp3 ext\n", filename);
        return -1;
    }

    // open sdl audio to record
    if (audio_stream_rec_mono == NULL) {
        audio_stream_rec_mono = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &record_spec, NULL, NULL);
        if (audio_stream_rec_mono == NULL) {
            ERROR("SDL_OpenAudioDeviceStream failed for record\n");
            return -1;
        }
    }
    Audio_stream = audio_stream_rec_mono;

    // init state
    memset(&state, 0, sizeof(state));
    state.state       = AUDIO_STATE_RECORD_FROM_MIC;
    state.record_secs = 0;
    state.volume      = 0;
    state.paused      = start_paused;

    // create thread to xfer microphone data to mp3 file
    cx = calloc(1, sizeof(record_mic_cx_t));
    if (dir && filename) {
        strcpy(cx->dir, dir);
        strcpy(cx->filename, filename);
        cx->append = append;
    }
    cx->auto_stop_secs = auto_stop_secs;
    sdlx_create_detached_thread(record_mic_thread, "record_mic", cx);

    // success
    return 0;
}

static int record_mic_thread(void *cx_arg)
{
    record_mic_cx_t *cx = (record_mic_cx_t*)cx_arg;
    float        mono_buff[4096];
    float        stereo_buff[8192];
    int          bytes;
    int          mono_buff_samples, stereo_buff_samples;
    double       silence_secs = 0;
    double       record_secs = 0;
    void        *mp3_cx = NULL;

    // start audio stream;
    // the audio stream is running even when state is paused, to provide saved audio samples
    SDL_ResumeAudioStreamDevice(Audio_stream);  

    while (true) {
        // if in stopping state then done
        if (state.stopping) {
            break;
        }

        // get audio data
        // note: last arg is buffer length, in bytes
        bytes = SDL_GetAudioStreamData(Audio_stream, mono_buff, sizeof(mono_buff));  
        if (bytes == -1) {
            ERROR("SDL_GetAudioStreamData failed, %s\n", SDL_GetError());
            break;
        }
        if (bytes == 0) {
            usleep(TWO_MS);
            continue;
        }
        mono_buff_samples = bytes / sizeof(float);
        stereo_buff_samples = 2 * mono_buff_samples;

        // scale record data, and create stereo_buff
        for (int i = 0; i < mono_buff_samples; i++) {
            mono_buff[i] = mono_buff[i] * audio_params.record_gain;
            stereo_buff[2*i] = stereo_buff[2*i+1] = mono_buff[i];
        }

        // save audio samples, these samples can be obtained and used by an app,
        // for example to compute fft
        save_audio_samples(mono_buff, mono_buff_samples, 1);

        // calculate volume of the samples just obtained
        state.volume = calc_volume(mono_buff, mono_buff_samples);

        // if not paused then perform recording  
        if (!state.paused && cx->dir[0] != '\0' && cx->filename[0] != '\0') {
            // open mp3 file, if not already opened
            if (mp3_cx == NULL) {
                mp3_cx = mp3_file_open(cx->dir, cx->filename, 2, cx->append);
                if (mp3_cx == NULL) {
                    ERROR("failed to open %s/%s append=%d\n", cx->dir, cx->filename, cx->append);
                    break;
                }
            }

            // write the data to the mp3 file
            mp3_file_write(mp3_cx, stereo_buff, stereo_buff_samples);

            // keep track of how long the recording has been in progress
            record_secs += ((double)mono_buff_samples / FRAMES_PER_SEC);
            state.record_secs = record_secs;

            // if auto_stop is enabled then if silent for n secs stop recording
            if (cx->auto_stop_secs > 0) {
                //INFO("VOL %f  SILENCE %f  SECS %f\n", 
                //       state.volume,  audio_params.record_silence, silence_secs);
                if (state.volume < audio_params.record_silence) {
                    silence_secs += ((double)mono_buff_samples / FRAMES_PER_SEC);
                } else {
                    silence_secs = 0;
                }
                if (silence_secs > cx->auto_stop_secs) {
                    INFO("auto stopping, %d secs of silence detected\n", cx->auto_stop_secs);
                    break;
                }
            }

            // limit recording time
            if (state.record_secs > MAX_RECORD_SECS) {
                break;
            }
        }

        // short sleep
        usleep(TWO_MS);
    }

    // pause and clear the record stream
    SDL_PauseAudioStreamDevice(Audio_stream);  
    SDL_ClearAudioStream(Audio_stream);

    // add short tone to end of file
    static float *tone;
    #define AMPLITUDE      0.15
    #define HZ             500
    #define NUM_SIN_WAVES  (HZ / 4)  // 1/4 sec
    #define N_ONE_SIN_WAVE (FRAMES_PER_SEC / HZ)   // samples in 1 sine wave
    #define N_TOTAL        (NUM_SIN_WAVES * N_ONE_SIN_WAVE)  // total samples
    if (tone == NULL) {
        tone = malloc(2 * N_TOTAL * sizeof(float));
        for (int j = 0; j < N_TOTAL; j++) {
            tone[2*j] = tone[2*j+1] = AMPLITUDE * sin((2*M_PI) * ((double)j / N_ONE_SIN_WAVE));
        }
    }
    if (mp3_cx) {
        mp3_file_write(mp3_cx, tone, 2*N_TOTAL);
    }

    // cleanup and return
    if (mp3_cx) {
        mp3_file_close(mp3_cx);
    }
    Audio_stream = NULL;
    free(cx);
    set_state_idle();
    return 0;
}

// -----------------  RECORD ANDROID DEVICE AUDIO TO STEREO MP3 FILE  -------------

typedef struct {
    char dir[100];
    char filename[100];
    bool append;
} record_dev_cx_t;

static int record_dev_thread(void *cx);

int sdlx_audio_record_from_device(char *dir, char *filename, bool append, bool start_paused)
{
    record_dev_cx_t *cx = NULL;
    int              rc;

    // stop audio 
    if (sdlx_audio_stop() != 0) {
        ERROR("sdlx_audio_stop failed\n");
        return -1;
    }

    // if filename provided it must have mp3 extension
    if (filename && strstr(filename, ".mp3") == NULL) {
        ERROR("filename '%s' must have mp3 ext\n", filename);
        return -1;
    }

    // call java to start playback capture
    rc = util_start_playbackcapture();
    if (rc != 0) {
        ERROR("util_start_playbackcapture failed\n");
        return -1;
    }

    // init state
    memset(&state, 0, sizeof(state));
    state.state       = AUDIO_STATE_RECORD_FROM_DEVICE;
    state.record_secs = 0;
    state.volume      = 0;
    state.paused      = start_paused;

    // if requested, initialize to paused state
    if (start_paused) {
        sdlx_audio_pause();
    }

    // create record_dev_thread
    cx = calloc(1, sizeof(record_dev_cx_t));
    if (dir && filename) {
        strcpy(cx->dir, dir);
        strcpy(cx->filename, filename);
        cx->append = append;
    }
    sdlx_create_detached_thread(record_dev_thread, "record_dev", cx);

    // return success
    return 0;
}

static int record_dev_thread(void *cx_arg)
{
    #define MAX_SAMPLES 2048

    record_dev_cx_t *cx = cx_arg;
    float            samples[MAX_SAMPLES];
    int              rc;
    double           record_secs = 0;
    void            *mp3_cx = NULL;

    while (true) {
        // if in stopping state then goto done;
        if (state.stopping) {
            break;
        }

        // get playback capture audio samples,
        // the samples are interleaved left/right channel
        rc = util_get_playbackcapture_audio(samples, MAX_SAMPLES);
        if (rc != 0) {
            ERROR("util_get_playbackcapture_audio failed\n");
            break;
        }

        // save audio samples, for optional use by an app, such as to compute fft
        save_audio_samples(samples, MAX_SAMPLES, 2);

        // calculate volume
        state.volume = calc_volume(samples, MAX_SAMPLES);

        // if not paused then perform recording  
        if (!state.paused && cx->dir[0] != '\0' && cx->filename[0] != '\0') {
            // open mp3 file, if not already opened
            if (mp3_cx == NULL) {
                mp3_cx = mp3_file_open(cx->dir, cx->filename, 2, cx->append);
                if (mp3_cx == NULL) {
                    ERROR("failed to open %s/%s append=%d\n", cx->dir, cx->filename, cx->append);
                    break;
                }
            }

            // write samples to the mp3 file
            mp3_file_write(mp3_cx, samples, MAX_SAMPLES);

            // keep track of how long the recording has been in progress
            record_secs += ((double)MAX_SAMPLES / 2) / FRAMES_PER_SEC;
            state.record_secs = record_secs;

            // limit recording time
            if (state.record_secs > MAX_RECORD_SECS) {
                break;
            }
        }
    }

    // stop playback capture
    // close mp3 file,
    // return success
    util_stop_playbackcapture();
    if (mp3_cx) {
        mp3_file_close(mp3_cx);
    }
    free(cx);
    set_state_idle();
    return 0;
}

// -----------------  PLAY SEQUENCE OF TONES  -------------

// defines
#define MIN_TONE_FREQ  25   // inclusive range
#define MAX_TONE_FREQ  6000 

// typedefs
typedef struct {
    int n;
    float data[0];
} sine_wave_t;

typedef struct {
    int num_tones;
    sdlx_tone_t tones[0];
} play_tones_cx_t;

// variables
static sine_wave_t *sine_waves[MAX_TONE_FREQ+1];

// prototypes
static int tones_thread(void *cx_arg);
static void smooth(float *buff, int len);
static void play_buff(float *samples, int num_samples, int num_channels, int *total_queued_samples);

int sdlx_audio_play_tones(sdlx_tone_t *tones)
{
    int                  num_tones, duration_ms, i;
    play_tones_cx_t     *cx;
    const SDL_AudioSpec  playback_spec = { SDL_AUDIO_F32, 1, FRAMES_PER_SEC };

    // stop audio 
    if (sdlx_audio_stop() != 0) {
        ERROR("failed to stop audio\n");
        return -1;
    }

    // open sdl audio for playback
    if (audio_stream_play_mono == NULL) {
        audio_stream_play_mono = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &playback_spec, NULL, NULL);
        if (audio_stream_play_mono == NULL) {
            ERROR("SDL_OpenAudioDeviceStream failed for playback\n");
            return -1;
        }
    }
    Audio_stream = audio_stream_play_mono;

    // loop over tones to determine total duration and num_tones
    num_tones = 0;
    duration_ms = 0;
    for (i = 0; tones[i].intvl_ms > 0; i++) {
        sdlx_tone_t *t = &tones[i];
        duration_ms += t->intvl_ms;
        num_tones++;
    }

    // init state for playing tones
    memset(&state, 0, sizeof(state));
    state.state             = AUDIO_STATE_PLAY_TONES_SEQUENCE; 
    state.play_current_secs = 0;
    state.play_total_secs   = duration_ms / 1000;
    state.volume            = 0;

    // create thread to play the tones
    cx = malloc(sizeof(play_tones_cx_t) + num_tones * sizeof(sdlx_tone_t));
    cx->num_tones = num_tones;
    memcpy(cx->tones, tones, num_tones * sizeof(sdlx_tone_t));
    sdlx_create_detached_thread(tones_thread, "play_tones", cx);

    // success
    return 0;
}

static int tones_thread(void *cx_arg)
{
    play_tones_cx_t *cx = (play_tones_cx_t*)cx_arg;
    float           *samples;
    int              max_samples;
    int              total_queued_samples = 0;

    static int seqnum;

    // allocate samples to handle a tone or gap of up to 30 secs
    max_samples = 30 * FRAMES_PER_SEC;
    samples = malloc(max_samples * sizeof(float));
    if (samples == NULL) {
        ERROR("malloc failed\n");
        goto done;
    }

    // pre calculate the sine waves for the frequency(s) requested
    for (int i = 0; i < cx->num_tones; i++) {
        sdlx_tone_t *t = &cx->tones[i];
        int n, j;
        sine_wave_t *sw;

        if (t->freq != 0 && t->freq < MIN_TONE_FREQ) {
            t->freq = MIN_TONE_FREQ;
        }
        if (t->freq > MAX_TONE_FREQ) {
            t->freq = MAX_TONE_FREQ;
        }

        if (t->freq && sine_waves[t->freq] == NULL) {
            n = nearbyint(FRAMES_PER_SEC / t->freq);
            sw = malloc(sizeof(int) + n * sizeof(float));
            sw->n = n;
            for (j = 0; j < n; j++) {
                sw->data[j] = sin((2*M_PI) * ((double)j / n));
            }
            sine_waves[t->freq] = sw;
        }
    }

    // start playing tones
    SDL_ResumeAudioStreamDevice(Audio_stream);  

    // loop over the tones
    for (int i = 0; i < cx->num_tones; i++) {
        sdlx_tone_t *t = &cx->tones[i];
        int n;  // number of samples

        //INFO("tone[%d] freq=%d millisecs=%d\n", i, t->freq, t->intvl_ms);

        // update audio state fields:
        // - play_tones_freq: the frequency of the tone being played; or 0 for silent gap
        // - play_tones_seqnum: incremented for each tone/gap playedA;
        //     an app can poll the seqnum to determine if a new tone is being played
        seqnum++;
        state.play_tones_freq = t->freq;
        state.play_tones_seqnum = seqnum;

        // construct samples for either:
        // - gap  (when t->freq == 0), or
        // - tone
        if (t->freq == 0) {
            n = FRAMES_PER_SEC * t->intvl_ms / 1000;
            if (n > max_samples) {
                n = max_samples;
            }
            memset(samples, 0, n*sizeof(float));
        } else {
            sine_wave_t *sw = sine_waves[t->freq];
            int num_sine_waves = t->intvl_ms * t->freq / 1000;
            float *ptr = samples;

            if (num_sine_waves * sw->n > max_samples) {
                num_sine_waves = max_samples / sw->n;
            }

            for (int j = 0; j < num_sine_waves; j++) {
                memcpy(ptr, sw->data, sw->n * sizeof(float));
                ptr += sw->n;
            }
            n = ptr - samples;

            // prevent popping sound by ramping up/down the begining/end of samples
            smooth(samples, n);
        }

        // play the tone, or gap
        play_buff(samples, n, 1, &total_queued_samples);
        if (state.stopping) {
            break;
        }
    }

    // wait for all queued audio to be played
    while (!state.paused && SDL_GetAudioStreamQueued(Audio_stream) > 0) {  
        usleep(TWO_MS);
    }
    SDL_ClearAudioStream(Audio_stream);

done:
    // cleanup and return
    Audio_stream = NULL;
    free(cx);
    free(samples);
    set_state_idle();
    return 0;
}

static void smooth(float *samples, int num_samples)
{
    #define MAX_SMOOTHER  (FRAMES_PER_SEC / 200)   // number of frames in 5 ms

    // apply 'S' curve to the begining and end of samples;
    // this eliminates a popping sound by providing a smooth transition
    //
    // google searches:
    //   "equation for an s curve to smootly ramp up or down audio data"
    //   "plot 3x^2 - 2x^3"

    static float *smoother;

    if (num_samples < 3 * MAX_SMOOTHER) {
        return;
    }

    if (smoother == NULL) {
        smoother = calloc(MAX_SMOOTHER, sizeof(float));
        for (int i = 0; i < MAX_SMOOTHER; i++) {
            float x = (float)i / MAX_SMOOTHER;
            float x_squared = x * x;
            float x_cubed   = x_squared * x;
            smoother[i] = 3 * x_squared - 2 * x_cubed;
        }
    }

    for (int i = 0; i < MAX_SMOOTHER; i++) {
        samples[i] *= smoother[i];
        samples[num_samples-1-i] *= smoother[i];
    }
}

// returns with 40 ms or less, still being played
static void play_buff(float *samples, int num_samples, int num_channels, int *total_queued_samples)
{
    int num_xfer_samples, num_remaining_samples;
    int queued_ms;

    //INFO("num_samples, channels=%d %d\n", num_samples, num_channels);

    num_remaining_samples = num_samples;
    while (num_remaining_samples) {
        // If stopping and there is more than 1 sec of samples remaining
        // then stop immedeately, otherwise let the remaining samples finish.
        // Letting remaining samples finish avoids audio click or pop sound
        // that occurs when the audio is stopped abrubptly.
        if ((state.stopping) && 
            (state.paused || num_remaining_samples > FRAMES_PER_SEC*num_channels)) 
        {
            break;
        }

        // if paused then short sleep and contnue
        if (state.paused) {
            usleep(TWO_MS);
            continue;
        }

        // queue up to 960 frames of audio, 960 frames is 20 ms
        num_xfer_samples = (num_remaining_samples > 960*num_channels 
                            ? 960*num_channels : 
                            num_remaining_samples);
        SDL_PutAudioStreamData(Audio_stream, samples, num_xfer_samples*sizeof(float));

        // publish duration played
        *total_queued_samples += num_xfer_samples;
        state.play_current_secs = (*total_queued_samples / num_channels) / FRAMES_PER_SEC;

        // calculate volume for the samples just queued
        state.volume = calc_volume(samples, num_xfer_samples);

        // save audio samples
        save_audio_samples(samples, num_xfer_samples, num_channels);

        // sleep while there is more than 40 ms queued;
        // break out of this sleep loop if audio state has become stopping or paused
        do {
            queued_ms = (SDL_GetAudioStreamQueued(Audio_stream) / (sizeof(float) * num_channels)) / FRAMES_PER_MS;
            usleep(TWO_MS);
            if (state.stopping || state.paused) {
                break;
            }
        } while (queued_ms > 40);

        // advance samples
        samples += num_xfer_samples;
        num_remaining_samples -= num_xfer_samples;
    }
}

// -----------------  PLAY BUFFER  ------------------------

typedef struct {
    float *samples;
    int    num_samples;
    int    num_channels;
    int    loops;
    bool   free_samples_when_done;
} play_buff_cx_t;

static int play_buff_thread(void *cx_arg);

int sdlx_audio_play_buff(float *samples, int num_samples, int num_channels, int loops, bool free_samples_when_done)
{
    const SDL_AudioSpec playback_spec = { SDL_AUDIO_F32, num_channels, FRAMES_PER_SEC };
    int                 duration_ms;
    play_buff_cx_t     *cx;

    // stop audio 
    if (sdlx_audio_stop() != 0) {
        ERROR("failed to stop audio\n");
        return -1;
    }

    // sanity check num_channels_arg
    if (num_channels != 1 && num_channels != 2) {
        ERROR("num_channelse = %d\n", num_channels);
        return -1;
    }

    // open sdl audio for playback
    if (num_channels == 1) {
        if (audio_stream_play_mono == NULL) {
            audio_stream_play_mono = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &playback_spec, NULL, NULL);
            if (audio_stream_play_mono == NULL) {
                ERROR("SDL_OpenAudioDeviceStream failed for playback\n");
                return -1;
            }
        }
        Audio_stream = audio_stream_play_mono;
    } else {
        if (audio_stream_play_stereo == NULL) {
            audio_stream_play_stereo = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &playback_spec, NULL, NULL);
            if (audio_stream_play_stereo == NULL) {
                ERROR("SDL_OpenAudioDeviceStream failed for playback\n");
                return -1;
            }
        }
        Audio_stream = audio_stream_play_stereo;
    }

    // calculate duration
    duration_ms = (num_samples / num_channels) / FRAMES_PER_MS * loops;

    // init state for playing the caller supplied raw data
    memset(&state, 0, sizeof(state));
    state.state             = AUDIO_STATE_PLAY_BUFF; 
    state.play_current_secs = 0;
    state.play_total_secs   = duration_ms / 1000;
    state.volume            = 0;

    // init cx and create play_buff_thread
    cx = malloc(sizeof(play_buff_cx_t));
    cx->samples                = samples;
    cx->num_samples            = num_samples;
    cx->num_channels           = num_channels;
    cx->loops                  = loops;
    cx->free_samples_when_done = free_samples_when_done;
    sdlx_create_detached_thread(play_buff_thread, "play_buff", cx);

    // success
    return 0;
}

static int play_buff_thread(void *cx_arg)
{
    play_buff_cx_t *cx = cx_arg;
    int total_queued_samples = 0;

    SDL_ResumeAudioStreamDevice(Audio_stream);  

    // call play_buff for the specified number of loops
    for (int i = 0; cx->loops == 0 || i < cx->loops; i++) {
        play_buff(cx->samples, cx->num_samples, cx->num_channels, &total_queued_samples);
        if (state.stopping) {
            break;
        }
    }

    // wait for all queued audio to be played
    while (!state.paused && SDL_GetAudioStreamQueued(Audio_stream) > 0) {
        usleep(TWO_MS);
    }
    SDL_ClearAudioStream(Audio_stream);

    // cleanup and return
    Audio_stream = NULL;
    if (cx->free_samples_when_done) {
        free(cx->samples);
    }
    free(cx);
    set_state_idle();
    return 0;
}

// -----------------  PLAY FILE  --------------------------

// variables
static SDL_AudioSpec play_file_spec;

// prototypes
static void mixer_track_raw_callback(void *userdata, MIX_Track *track, const SDL_AudioSpec *spec, float *pcm, int samples);
static void mixer_track_stopped_callback(void *userdata, MIX_Track *track);
static char *audio_fmt_str(int fmt);

int sdlx_audio_play_file(char *dir, char *filename)
{
    char path[200];
    bool succ;
    long duration_frames;
    long duration_ms;

    const SDL_AudioSpec playback_request_spec = { SDL_AUDIO_F32, 2, FRAMES_PER_SEC };
    SDL_AudioSpec       playback_actual_spec;

    // stop audio 
    if (sdlx_audio_stop() != 0) {
        ERROR("failed to stop audio\n");
        return -1;
    }

    // create pathname from caller supplied dir and filename
    concat_dir_and_filename(dir, filename, path);
    if (!util_file_exists(path, NULL)) {
        ERROR("file %s doesnt exist\n", path);
        return -1;
    }

    // if mixer_audio,track, or dev still allocated then destroy them
    if (mixer_audio) {
        MIX_DestroyAudio(mixer_audio); 
        mixer_audio = NULL; 
    }
    if (mixer_track) {
        MIX_DestroyTrack(mixer_track);
        mixer_track = NULL; 
    }
    if (mixer_dev) {
        MIX_DestroyMixer(mixer_dev);
        mixer_dev = NULL;
    }

    // create mixer device,
    mixer_dev = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &playback_request_spec);
    if (mixer_dev == NULL) {
        ERROR("MIX_CreateMixerDevice failed, %s\n", SDL_GetError());
        return -1;
    }

    // get the actual playback spec, and 
    // verify the actual playback spec values match the playback_request_spec
    MIX_GetMixerFormat(mixer_dev, &playback_actual_spec);
    INFO("actual_spec: format=%s channels=%d freq=%d\n", 
         audio_fmt_str(playback_actual_spec.format), 
         playback_actual_spec.channels, 
         playback_actual_spec.freq);
    if (playback_actual_spec.format != playback_request_spec.format) {
        ERROR("actual/request format: %s %s\n",
            audio_fmt_str(playback_actual_spec.format), audio_fmt_str(playback_request_spec.format));
    }
    if (playback_actual_spec.channels != playback_request_spec.channels) {
        ERROR("actual/request channels: %d %d\n",
            playback_actual_spec.channels, playback_request_spec.channels);
    }
    if (playback_actual_spec.freq != playback_request_spec.freq) {
        ERROR("actual/request freq: %d %d\n",
            playback_actual_spec.freq, playback_request_spec.freq);
    }

    // create mixer track
    mixer_track = MIX_CreateTrack(mixer_dev);
    if (mixer_track == NULL) {
        ERROR("MIX_CreateTrack failed, %s\n", SDL_GetError());
        MIX_DestroyMixer(mixer_dev); mixer_dev = NULL;
        return -1;
    }

    // set track callbacks
    MIX_SetTrackRawCallback(mixer_track, mixer_track_raw_callback, NULL);
    MIX_SetTrackStoppedCallback(mixer_track, mixer_track_stopped_callback, NULL);

    // load the file, set predecode arg to false
    mixer_audio = MIX_LoadAudio(mixer_dev, path, false);
    if (mixer_audio == NULL) {
        ERROR("MIX_LoadAudio, %s\n", SDL_GetError());
        MIX_DestroyTrack(mixer_track); mixer_track = NULL;
        MIX_DestroyMixer(mixer_dev); mixer_dev = NULL;
        return -1;
    }

    // get audio format
    MIX_GetAudioFormat(mixer_audio, &play_file_spec);
    INFO("file %s: format = %s 0x%x  channels=%d  freq = %d\n", 
         path,
         audio_fmt_str(play_file_spec.format), play_file_spec.format, 
         play_file_spec.channels, 
         play_file_spec.freq);
    if (play_file_spec.channels != 2 || play_file_spec.freq != FRAMES_PER_SEC) {
        ERROR("expected channels=2 and freq=%d, continuing\n", FRAMES_PER_SEC);
    }

    // get file duration
    duration_frames = MIX_GetAudioDuration(mixer_audio);
    if (duration_frames < 0) {
        ERROR("failed to get duration of %s, duration_frames=%ld\n", path, duration_frames);
        duration_frames = 0;
    }
    duration_ms = MIX_AudioFramesToMS(mixer_audio, duration_frames);

    // set track audio, and play track
    succ = MIX_SetTrackAudio(mixer_track, mixer_audio);
    if (!succ) {
        ERROR("MIX_SetTrackAudio failed, %s\n", SDL_GetError());
        MIX_DestroyAudio(mixer_audio); mixer_audio = NULL;
        MIX_DestroyTrack(mixer_track); mixer_track = NULL;
        MIX_DestroyMixer(mixer_dev); mixer_dev = NULL;
        return -1;
    }
    succ = MIX_PlayTrack(mixer_track, 0);
    if (!succ) {
        ERROR("MIX_PlayTrack failed, %s\n", SDL_GetError());
        MIX_DestroyAudio(mixer_audio); mixer_audio = NULL;
        MIX_DestroyTrack(mixer_track); mixer_track = NULL;
        MIX_DestroyMixer(mixer_dev); mixer_dev = NULL;
        return -1;
    }

    // init state
    memset(&state, 0, sizeof(state));
    state.state             = AUDIO_STATE_PLAY_FILE;
    state.play_current_secs = 0;
    state.play_total_secs   = duration_ms / 1000;
    state.volume            = 0;

    // return success
    return 0;
}

void sdlx_audio_set_play_file_time(int secs)
{
    bool succ;
    long frames;

    INFO("setting play file time to %d secs\n", secs);

    // verify file play is in progress
    if (state.state != AUDIO_STATE_PLAY_FILE || mixer_track == NULL) {
        ERROR("state is not AUDIO_STATE_PLAY_FILE or track is NULL\n");
        return;
    }

    // set track playback position
    frames = MIX_TrackMSToFrames(mixer_track, secs*1000L);
    succ = MIX_SetTrackPlaybackPosition(mixer_track, frames);
    if (!succ) {
        ERROR("MIX_SetTrackPlaybackPosition failed, %s\n", SDL_GetError());
        return;
    }

    // update state.play_current_secs;
    frames = MIX_GetTrackPlaybackPosition(mixer_track);
    state.play_current_secs = MIX_TrackFramesToMS(mixer_track, frames) / 1000;
    INFO("readback of play file time = %d\n", state.play_current_secs);
}

static void mixer_track_raw_callback(void *userdata, MIX_Track *track, const SDL_AudioSpec *spec,
                                     float *samples, int num_samples)
{
    // update state.play_current_secs_
    long frames = MIX_GetTrackPlaybackPosition(track);
    state.play_current_secs = MIX_TrackFramesToMS(track, frames) / 1000;

    // update state.volume
    state.volume = calc_volume(samples, num_samples);

    //INFO("fmt=%s  num_samples=%d  volume=%d  play_current=%d secs\n", 
    //     audio_fmt_str(play_file_spec.format),
    //     num_samples, state.volume, state.play_current_secs);

    // save audio samples
    save_audio_samples(samples, num_samples, play_file_spec.channels);
}

static void mixer_track_stopped_callback(void *userdata, MIX_Track *track)
{
    // sanity check
    if (track != mixer_track) {
        ERROR("track != mixer_track\n");
    }

    // set state to idle
    state.state = AUDIO_STATE_IDLE;
    memset(&state, 0, sizeof(state));
}

static char *audio_fmt_str(int fmt)
{
    if (fmt == SDL_AUDIO_UNKNOWN) return "SDL_AUDIO_UNKNOWN";
    if (fmt == SDL_AUDIO_U8)      return "SDL_AUDIO_U8";
    if (fmt == SDL_AUDIO_S8)      return "SDL_AUDIO_S8";
    if (fmt == SDL_AUDIO_S16LE)   return "SDL_AUDIO_S16LE";
    if (fmt == SDL_AUDIO_S16BE)   return "SDL_AUDIO_S16BE";
    if (fmt == SDL_AUDIO_S32LE)   return "SDL_AUDIO_S32LE";
    if (fmt == SDL_AUDIO_S32BE)   return "SDL_AUDIO_S32BE";
    if (fmt == SDL_AUDIO_F32LE)   return "SDL_AUDIO_F32LE";
    if (fmt == SDL_AUDIO_F32BE)   return "SDL_AUDIO_F32BE";

    return "SDL_AUDIO_INVALID_FMT";
}

// -----------------  CREATE TEST MP3 FILE  -------------------

void sdlx_create_test_file(char *dir, char *filename, int freq1, int freq2, int duration_secs)
{
    #define MAX_DUR_SECS 10
    #define TWO_PI       (2 * M_PI)

    int    num_samples;
    int    num_frames;
    float *samples;
    void  *cx;

    // ensure duration is in range
    if (duration_secs < 1 || duration_secs > MAX_DUR_SECS) {
        ERROR("invalid duration %d secs, must be <= %d secs\n", duration_secs, MAX_DUR_SECS);
        return;
    }

    // allocate memory for samples
    num_frames = FRAMES_PER_SEC * duration_secs;
    num_samples = 2 * num_frames;
    samples = calloc(num_samples, sizeof(float));

    // init buffer
    INFO("creating %s/%s - %d sec freq sweep %d-%d Hz\n", dir, filename, duration_secs, freq1, freq2);
    if (freq1 < 20 || freq1 > 10000 || freq2 < 20 || freq2 > 10000 || freq1 > freq2) {
        ERROR("invalid freq range %d - %d\n", freq1, freq2);
        free(samples);
        return;
    }
    double f=0, phase=0;
    int    k = 0;
    for (int i = 0; i < num_frames; i++) {
        f = freq1 + ((double)i / num_frames) * (freq2 - freq1);
        phase += (TWO_PI / FRAMES_PER_SEC) * f;
        samples[k] = samples[k+1] = sin(phase);
        k+=2;
    }

    // create mp3 file from stereo samples
    cx = mp3_file_open(dir, filename, 2, false);
    mp3_file_write(cx, samples, num_samples);
    mp3_file_close(cx);

    // cleanup
    free(samples);
}
