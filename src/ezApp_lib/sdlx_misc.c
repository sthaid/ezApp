#include <std_hdrs.h>

#include <sdlx.h>
#include <utils.h>
#include <private.h>

#include <SDL3/SDL.h>

//
// defines
//

#define TEN_MS 10000

//
// variables
//

extern SDL_Window *window;

static int video_init_count;
static int audio_init_count;
static int sensor_init_count;

// -----------------  SDLX INIT / QUIT  -----------------------

// picoc can call sdlx_init and sdlx_quit,
// but these calls are intended only for use by eztest

int sdlx_init(int subsys, double aspect_ratio)
{
    int rc;

    if (subsys & SUBSYS_VIDEO) {
        if (video_init_count == 0) {
            rc = sdlx_video_init(aspect_ratio);
            if (rc != 0) {
                ERROR("failed to init video\n");
                SDL_Quit();
                return -1;
            }
        }
        video_init_count++;
    }

    if (subsys & SUBSYS_AUDIO) {
        if (audio_init_count == 0) {
            rc = sdlx_audio_init();
            if (rc != 0) {
                ERROR("failed to init audio\n");
                SDL_Quit();
                return -1;
            }
        }
        audio_init_count++;
    }

    if (subsys & SUBSYS_SENSOR) {
        if (sensor_init_count == 0) {
            rc = sdlx_sensor_init();
            if (rc != 0) {
                ERROR("failed to init sensor\n");
                SDL_Quit();
                return -1;
            }
        }
        sensor_init_count++;
    }

    return 0;
}

void sdlx_quit(int subsys)
{
    if (subsys & SUBSYS_VIDEO) {
        if (--video_init_count == 0) {
            sdlx_video_quit();
        }
    }

    if (subsys & SUBSYS_AUDIO) {
        if (--audio_init_count == 0) {
            sdlx_audio_quit();
        }
    }

    if (subsys & SUBSYS_SENSOR) {
        if (--sensor_init_count == 0) {
            sdlx_sensor_quit();
        }
    }

    if (video_init_count <= 0 && 
        audio_init_count <= 0 &&
        sensor_init_count <= 0)
    {
        SDL_Quit();
    }
}

// -----------------  GET INPUT STR  ----------------------

char *sdlx_get_input_str(char *prompt, bool numeric_keybd, char *dflt_input_str)
{
    static char        input[100];
    int                max_input, row;
    sdlx_loc_t        *loc;
    sdlx_event_t       event;

    // init
    memset(input, 0, sizeof(input));
    if (dflt_input_str) {
        strcpy(input, dflt_input_str);
    }
    max_input = strlen(input);

    // init text input properties
#ifdef ANDROID
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetBooleanProperty(props, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN, false);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER, SDL_CAPITALIZE_NONE);
    if (numeric_keybd == false) {
        // use full keyboard; enable SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_VISIBLE to
        // prevent Android from modifying the keyboard input
        SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_VISIBLE);
    } else {
        // use numeric keypad: notes - 
        //   2    = TYPE_CLASS_NUMBER
        //   8192 = TYPE_NUMBER_FLAG_DECIMAL
        //   4096 = TYPE_NUMBER_FLAG_SIGNED  (not working well in conjunction with TYPE_NUMBER_FLAG_DECIMAL)
        SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_ANDROID_INPUTTYPE_NUMBER, 2 | 8192);
    }
#else  // Linux
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, SDL_PROP_TEXTINPUT_TYPE_NUMBER, SDL_TEXTINPUT_TYPE_TEXT);
#endif

    // start text input, with properties initialized above
    SDL_StartTextInputWithProperties(window, props);

    // loop, adding chars to the input string, until newline char rcvd or cancelled
    while (true) {
        // clear backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // display prompt line(s), the prompt arg str can contain newline chars
        row = 0;
        if (prompt && prompt[0] != '\0') {
            row += 1;
            loc = sdlx_render_printf_ex2(0, ROW2Y(1), FONT_NORMAL, COLOR_WHITE, 0, "%s", prompt);
            row += nearbyint((double)loc->h / sdlx_char_height_dflt);
        }

        // display input value string
        row += 1;
        loc = sdlx_render_printf_ex2(0, ROW2Y(row), FONT_NORMAL, COLOR_WHITE, 0, "? %s", input);
        if ((util_microsec_timer() / 500000) & 1) {
            sdlx_render_printf_ex2(loc->x+loc->w, loc->y, FONT_NORMAL, COLOR_WHITE, 0, "%s", "_");
        }

        // register cancel event;
        // this event is needed to deal with the keybd being dismissed
        row += 2;
        loc = sdlx_render_printf_ex2(0, ROW2Y(row), FONT_NORMAL, COLOR_LIGHT_BLUE, 0, "Cancel");
        sdlx_register_event(loc, EVID_QUIT);

        // register for keyboard events
        sdlx_register_event(NULL, EVID_KEYBD);

        // present display
        sdlx_display_present();

        // wait for event
        sdlx_get_event(100000, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process sdlx events EVID_KEYBD and EVID_QUIT
        if (event.event_id == EVID_KEYBD) {
            int ch = event.u.private_keybd.keycode;

            if (ch >= 0x20 && ch < 0x7f) {
                // add the printable char to the input array
                if (max_input < sizeof(input)) {
                    input[max_input++] = ch;
                    input[max_input] = '\0';
                }
            } else if (ch == '\b') {
                // backspace char: remove last char in input array
                if (max_input > 0) {
                    input[--max_input] = '\0';
                }
            } if (ch == '\r') {
                // <cr> char: done with input, break out of loop
                break;
            }
        } else if (event.event_id == EVID_QUIT) {
            input[0] = '\0';
            break;
        }
    }

    // cleanup
    SDL_StopTextInput(window);
    SDL_DestroyProperties(props);

    // return input string
    return input;
}

// -----------------  MISC ROUTINES NOT MADE AVAILABLE IN PICOC  ---------------------- 

// - - - - - - - - - sdlx_get_storage_path  - - - - - - - - - - 

char *sdlx_get_storage_path(void)
{
#ifdef ANDROID
    return (char*)SDL_GetAndroidInternalStoragePath();
#else  // not Android
    static char storage_path_buff[200];

    if (storage_path_buff[0] == '\0') {
        getcwd(storage_path_buff, sizeof(storage_path_buff));
    }

    return storage_path_buff;
#endif
}

// - - - - - - - - - sdlx_copy_asset_file - - - - - - - - - - - 

void sdlx_copy_asset_file(char *asset_filename, char *dest_dir)
{
    int rc;
    char dest_path[200];

    sprintf(dest_path, "%s/%s", dest_dir, asset_filename);

#ifdef ANDROID
    void  *ptr;
    size_t len;

    // remove dest file, because it may already exist
    unlink(dest_path);

    // read the asset using SDL_LoadFile;
    //
    // Note SDL_LoadFile calls SDL_IOFromFile, which attempts to read
    // the file as follows:
    // - if filename begins with '/' then use fopen
    //   else if filename begins with "content://" then use Android_JNI_OpenFileDescriptor
    //   else fopen of file in SDL_GetAndroidInternalStoragePath
    //   endif
    // - if above failed then try to read the file from assets, using Android_JNI_FileOpen
    ptr = SDL_LoadFile(asset_filename, &len);
    if (ptr == NULL ) {
        ERROR("failed to read apps.tar");
        return;
    }

    // write the asset file to dest_dir
    rc = util_write_file(dest_dir, asset_filename, ptr, len);
    SDL_free(ptr);
    if (rc != 0) {
        ERROR("failed to create %s/%s\n", dest_dir, asset_filename);
        return;
    }
#else  // not Android
    char cmd[250];

    sprintf(cmd, "cp ../assets/%s %s", asset_filename, dest_path);
    rc = system(cmd);
    if (rc != 0) {
        ERROR("cmd '%s' failed\n", cmd);
    }
#endif
}

// - - - - - - - - - sdlx_get_permission  - - - - - - - - - - - 

#ifdef ANDROID
#define PERM_NO_RESULT    0
#define PERM_GRANTED      1
#define PERM_NOT_GRANTED  2
static void get_permission_cb(void *userdata, const char *permission, bool granted)
{
    int *perm_result = (int*)userdata;

    INFO("permission=%s  granted=%d\n", permission, granted);
    *perm_result = (granted ? PERM_GRANTED : PERM_NOT_GRANTED);
}
#endif

int sdlx_get_permission(char *name)
{
#ifndef ANDROID
    // when not running on Android return success
    return 0;
#else
    bool succ;
    int perm_result;

    // API < 33 does not support or require POST_NOTIFICATION permission
    if ((strcmp(name, "android.permission.POST_NOTIFICATIONS") == 0) &&
        (SDL_GetAndroidSDKVersion() < 33))
    {
        INFO("ignoring %s at API %d\n", name, SDL_GetAndroidSDKVersion());
        return 0;
    }

    INFO("get_permission %s\n", name);

    // request permission
    perm_result = PERM_NO_RESULT;
    succ = SDL_RequestAndroidPermission(name, get_permission_cb, &perm_result);
    if (!succ) {
        ERROR("SDL_RequestAndroidPermission failed, %s\n", SDL_GetError());
        return -1;
    }

    // wait for permission request to be either granted or not-granted
    while (perm_result == PERM_NO_RESULT) {
        usleep(TEN_MS);
    }

    // if not granted then return error
    if (perm_result != PERM_GRANTED) {
        ERROR("%s not granted\n", name);
        return -1;
    }

    // return success
    return 0;
#endif
}

// - - - - - - - - - sdlx_create_detached_thread_private  - - - - - - - 

// from SDL doc ...
//
// If you want to use threads in your SDL app, it's strongly recommended that you
// do so by creating them using SDL functions. This way, the required attach/detach
// handling is managed by SDL automagically. If you have threads created by other
// means and they make calls to SDL functions, make sure that you call
// Android_JNI_SetupThread() before doing anything else otherwise SDL will attach
// your thread automatically anyway (when you make an SDL call), but it'll never
// detach it.

int sdlx_create_detached_thread(int (*thread_fn)(void*), char *thread_name, void *cx)
{
    SDL_Thread *x;

    x = SDL_CreateThread(thread_fn, thread_name, cx);
    if (x == NULL) {
        return -1;
    }

    SDL_DetachThread(x);
    return 0;
}

