#include <std_hdrs.h>

#include <sdlx.h>
#include <utils.h>
#include <svcs.h>
#include <private.h>

#ifdef ANDROID
#include <SDL3/SDL.h>
#endif

#include "version.h"

//
// defines
//

#ifdef ANDROID
#define MAIN SDL_main
#else
#define MAIN main
#endif

#define DEFAULT_DEVEL_PORT     9000   // IANA registered port range 1024 - 49151
#define DEFAULT_DEVEL_PASSWORD "change_me"
#define MIN_DEVEL_PASSWORD_LEN 4

#define LAST_PAGE ((max_apps - 1) / 18)

#define BG_COLOR COLOR_PURPLE

#define EVID_PAGE_DECREMENT  900
#define EVID_PAGE_INCREMENT  901
#define EVID_MINIMIZE        902

#define MS  1000
#define SEC 1000000

#define CREATE_FILES_INIT                1
#define CREATE_FILES_RESET_APPS_AND_SVCS 2

//#define PRINT_TYPE_SIZES

//
// typedefs
//

typedef struct {
    bool   devel_mode;
    int    devel_port;
    char   devel_password[50];
    bool   foreground_enabled;
    double record_gain;
    double record_silence;
    bool   event_box_enable;
    double aspect_ratio;
} params_t;

typedef struct {
    bool post_notifications;
    bool access_coarse_location;
    bool access_fine_location;
    bool activity_recognition;
    bool record_audio;
    bool camera; //xxx dont call camera routine if no perm
} perm_granted_t;

//
// variables
//

static char          *storage_path;
static params_t       params;
static pthread_t      server_tid;
static perm_granted_t perm_granted;

//
// prototypes
//

static void processing(void);
static int devel_mode_server_thread(void *cx);

// -----------------  MAIN  ------------------------------------------

static int init(void);
static void cleanup(void);
static void sigusr2_hndlr(int signum);
static void print_type_sizes(void) __attribute__ ((unused));
static void create_files(int action);

int MAIN(int argc, char **argv)
{
    int rc;

    rc = init();
    if (rc != 0) {
        ERROR("init failed, terminating\n");
        return 1;
    }

    processing();

    cleanup();
    return 0;
}

static int init(void)
{
    int rc;

    // get storage_path, and
    // set current working directory to storage_path
    storage_path = sdlx_get_storage_path();
    chdir(storage_path);

    // init logging
    rc = log_init();
    if (rc != 0) {
        ERROR("log_init failed\n");
        return -1;
    }

    // print startup message
    INFO("======================================================\n");
    INFO("========== STARTING: %s %s  ==========\n", VERSION, BUILD_DATE);
    INFO("======================================================\n");
    INFO("storage_path = %s\n", storage_path);
#ifdef ANDROID
    INFO("sdk_version  = %d\n", SDL_GetAndroidSDKVersion());
#endif

#ifdef PRINT_TYPE_SIZES
    // print type sizes
    print_type_sizes();
#endif

    // get permissions when running on Android;
    // this is noop when running on Linux
    #define GET_PERMISSION(perm,granted) \
        do { \
            if (sdlx_get_permission("android.permission." perm) != 0) { \
                ERROR("failed to get permission %s\n", perm); \
                granted = false; \
            } else { \
                INFO("permission granted %s\n", perm); \
                granted = true; \
            } \
        } while (0)
    GET_PERMISSION("POST_NOTIFICATIONS", perm_granted.post_notifications);
    GET_PERMISSION("ACCESS_COARSE_LOCATION", perm_granted.access_coarse_location);
    GET_PERMISSION("ACCESS_FINE_LOCATION", perm_granted.access_fine_location);
    GET_PERMISSION("ACTIVITY_RECOGNITION", perm_granted.activity_recognition);
    GET_PERMISSION("RECORD_AUDIO", perm_granted.record_audio);
    GET_PERMISSION("CAMERA", perm_granted.camera);  // xxx test without permission

    // init android utils, which provide support for:
    // - text to speech
    // - location
    // - flashlight
    // this is noop when running on Linux
    util_android_utils_init();

    // get params, if they don't exist, set to default value
    params.devel_mode = util_get_numeric_param(".", "devel_mode", false);
    params.devel_port = util_get_numeric_param(".", "devel_port", DEFAULT_DEVEL_PORT);
    char *s = util_get_str_param(".", "devel_password", DEFAULT_DEVEL_PASSWORD);
    strcpy(params.devel_password, s);
    free(s);
    params.foreground_enabled = util_get_numeric_param(".", "foreground_enabled", 1);
    params.record_gain = util_get_numeric_param(".", "record_gain", DEFAULT_RECORD_GAIN);
    params.record_silence = util_get_numeric_param(".", "record_silence", DEFAULT_RECORD_SILENCE);
    params.event_box_enable = util_get_numeric_param(".", "event_box_enable", false);
    params.aspect_ratio = util_get_numeric_param(".", "aspect_ratio", DEFAULT_ASPECT_RATIO);

    // provide params to other modules, when needed
    sdlx_event_box_ctrl(params.event_box_enable);
    sdlx_audio_params_t ap = { params.record_gain, params.record_silence };
    sdlx_audio_set_params(&ap);

    // extract files.tar, if needed
    create_files(CREATE_FILES_INIT);

    // allocate SIGUSR2, this signal is sent to the devel_mode_server_thread
    // when developer mode is disabled or developer mode port is changed
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sigusr2_hndlr;
    sigaction(SIGUSR2, &action, NULL);

    // create devel mode server thread
    sdlx_create_detached_thread(devel_mode_server_thread, "devel_server", NULL);

    // init sdl
    rc = sdlx_init(SUBSYS_VIDEO | SUBSYS_AUDIO | SUBSYS_SENSOR, params.aspect_ratio);
    if (rc != 0) {
        ERROR("sdlx_init failed\n");
        return -1;
    }
    INFO("sdlx_win_width,height = %d %d  sdlx_char_width,height=%d %d\n",
         sdlx_win_width, sdlx_win_height, sdlx_char_width_dflt, sdlx_char_height_dflt);

    // start/stop foreground mode based on the foreground_enabled param
    if (params.foreground_enabled) {
        if (perm_granted.access_coarse_location) {
            util_start_foreground();
        } else {
            ERROR("permission has not been granted to access_coarse_location\n");
        }
    } else {
        util_stop_foreground();
    }

    // start all services, that do not have a 'stopped' file in their dir
    svcs_start_all();

    // success
    INFO("init success\n");
    return 0;
}

static void cleanup(void)
{
    // note that this routine may not free all allocations;
    // but that shouldn't matter, because the program is terminating

    INFO("cleanup starting\n");

    INFO("stopping services\n");
    svcs_stop_all();

    INFO("destroying android utils\n");
    util_android_utils_destroy();

    INFO("quitting SDL subsystems\n");
    sdlx_quit(SUBSYS_VIDEO | SUBSYS_AUDIO | SUBSYS_SENSOR);

    INFO("cleanup completed, program terminating\n");
}

static void create_files(int action)
{
#ifndef ANDROID
    INFO("this routine is only executed when running on Android\n");
    return;
#endif

    // If apps or svcs dirs don't exist or if there is a new files.tar
    // then extract files.tar.
    // Existing files, such as params or mp3, are not removed.
    // This is done when ezApp initializes.
    if (action == CREATE_FILES_INIT) {
        bool extract_needed = false;
        int  rc;
        do {
            if (!util_file_exists(".", "apps")) { 
                INFO("extracting because no apps dir\n");
                extract_needed = true;
                break;
            }

            if (!util_file_exists(".", "files.tar")) { 
                INFO("extracting because no files.tar\n");
                extract_needed = true;
                break;
            }

            sdlx_copy_asset_file("files.tar.sig", ".");
            rc = system("cmp --quiet files.tar.sig files.tar.sig.save");
            rc = WEXITSTATUS(rc);
            util_delete_file(".", "files.tar.sig");
            if (rc != 0) {
                INFO("extracting because files.tar.sig different\n");
                extract_needed = true;
                break;
            }
        } while (0);

        if (extract_needed) {
            sdlx_copy_asset_file("files.tar", ".");
            sdlx_copy_asset_file("files.tar.sig", ".");
            system("tar -xvf files.tar");
            util_rename_file(".", "files.tar.sig", ".", "files.tar.sig.save");
        } else {
            INFO("not extracting\n");
        }

        system("rm -rf tmp/*");
            
    // remove existing apps and svcs, and recreate
    } else if (action ==  CREATE_FILES_RESET_APPS_AND_SVCS) {
        svcs_stop_all();
        system("rm -rf apps svcs tmp");
        system("tar -xvf files.tar apps svcs tmp");
        svcs_start_all();
    } else {
        ERROR("invalid arg, action %d\n", action);
    }
}

static void sigusr2_hndlr(int signum)
{
    // nothing needed here
}

static void print_type_sizes(void)
{
    INFO("type sizes ...\n");
    INFO("  sizoef(char)   = %zd", sizeof(char));
    INFO("  sizoef(short)  = %zd", sizeof(short));
    INFO("  sizoef(int)    = %zd", sizeof(int));
    INFO("  sizoef(long)   = %zd", sizeof(long));
    INFO("  sizoef(size_t) = %zd", sizeof(size_t));
    INFO("  sizoef(off_t)  = %zd", sizeof(off_t));
    INFO("  sizoef(time_t) = %zd", sizeof(time_t));
    INFO("  sizoef(clock_t)= %zd", sizeof(clock_t));
    INFO("  sizoef(float)  = %zd", sizeof(float));
    INFO("  sizoef(double) = %zd", sizeof(double));
    INFO("  sizeof(1)      = %zd", sizeof(1));
    INFO("  sizeof(1L)     = %zd", sizeof(1L));
}

// -----------------  PROCESSING  ------------------------------------

#define MAX_APPS (20 * 18)  // max of 20 pages

static char *apps[MAX_APPS];
static int   max_apps;
static int   page;

static void display_menu(void);
static void get_list_of_apps(void);
static void settings(void);

static void processing(void)
{
    sdlx_event_t event;

    while (true) {
        // clear the display, and set the font to default
        sdlx_display_init(BG_COLOR, PORTRAIT);
        sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);

        // display menu, and register for events
        display_menu();

        // register for screen bottom control events
        if (LAST_PAGE > 0) {
            sdlx_register_control_events(EVID_PAGE_DECREMENT, "<",
                                         EVID_PAGE_INCREMENT, ">",
                                         EVID_MINIMIZE, "X");
        } else {
            sdlx_register_control_events(0, NULL, 
                                         0, NULL, 
                                         EVID_MINIMIZE, "X");
        }

        // display message when devel mode is enabled
        if (params.devel_mode) {
            sdlx_render_printf_ex2(sdlx_win_width/2, sdlx_win_height-1.5*sdlx_char_height_dflt,
                                   FONT_SMALL, COLOR_WHITE, FLAG_X_CTR, 
                                   "developer mode");
        }

        // update the display
        sdlx_display_present();

        // wait for an event, 1 sec timeout
        sdlx_get_event(1*SEC, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process the event
        INFO("proc event_id %d\n", event.event_id);
        if (event.event_id == EVID_QUIT) {
            INFO("got EVID_QUIT\n");
            break;
        } else if (event.event_id == EVID_MINIMIZE) {
            INFO("got EVID_MINIMIZE\n");
#ifdef ANDROID
            INFO("calling sdlx_minimize_window on Android\n");
            sdlx_minimize_window();
#else
            INFO("end program on Linux\n");
            return;
#endif
        } else if (event.event_id == EVID_PAGE_DECREMENT) {
            if (--page < 0) {
                page = LAST_PAGE;
            }
        } else if (event.event_id == EVID_PAGE_INCREMENT) {
            if (++page > LAST_PAGE) {
                page = 0;
            }
        } else if (event.event_id >= 0 && event.event_id <= max_apps-1) {
            int id = event.event_id;
            sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);
            if (apps[id] == NULL) {
                ERROR("apps[%d] is NULL\n", id);
            } else if (strcmp(apps[id], "Settings") == 0) {
                settings();
            } else {
                run(apps[id], false);
            }
        }
    }
}

// -----------------  DISPLAY MENU  -------------------------------

static sdlx_texture_t *circle;
static sdlx_texture_t *create_filled_circle_texture(int radius, sdlx_color_t color);
static bool app_name_exists(char *name);
static void sanitize_input(char *s);

static void display_menu(void)
{
    int first, last;

    #define RADIUS 100

    // allocate circle texture, which is used when displaying menu items
    if (circle == NULL) {
        circle = create_filled_circle_texture(RADIUS, COLOR_BLUE);
    }

    // get the list of apps: 
    // - this initializes the apps[] array of  app names
    // - the dir names must be the same as the app names
    // - the apps array is indexed by the location on the display, for
    //   example idx=0 is top left, and idx=17 is bottom right
    get_list_of_apps();

    first = page * 18;
    last  = first + 17;

    for (int i = first; i <= last; i++) {
        char     *name = apps[i];
        char      s1[10], s2[10];
        int       len, l1, l2, lmax, x, y, fontid;
        double    chw, chh;
        sdlx_loc_t loc;

        // if no app at this menu loction then continue
        if (name == NULL) {
            continue;
        }

        // break app name into one or two strings, depending the name length
        len  = strlen(name);
        if (len > 8) len = 8;
        if (len <= 4) {
            l1 = len;
            l2 = 0;
            strcpy(s1, name);
            s2[0] = '\0';
            lmax = l1;
        } else {
            l1 = len / 2;
            l2 = len - l1;
            strncpy(s1, name, l1);
            strncpy(s2, name+l1, l2);
            s1[l1] = '\0';
            s2[l2] = '\0';
            lmax = l2;
        }

        // determine the fontid, and font char width/height;
        // which will be used below when displaying the menu item text
        if (s2[0] == '\0') {
            double k = (len == 1 ? 1 : 1.5);
            chw = (k * RADIUS) / lmax;
            fontid = sdlx_win_width / chw;
        } else {
            double k = ((len == 5 || len == 6) ? 1.35 : 1.5);
            chw = (k * RADIUS) / lmax;
            fontid = sdlx_win_width / chw;
        }
        chh = chw / 0.6;

        // determine dispaly location of the center of the menu item
        x = (sdlx_win_width/3/2) + (i%3) * (sdlx_win_width/3);
        y = ((sdlx_win_height-150)/6/2) + ((i-first)/3) * ((sdlx_win_height-150)/6);

        // display the menu item
        // - first render the circle
        // - then render the app name text within the circle
        sdlx_loc_t dest = {x-RADIUS, y-RADIUS, 2*RADIUS, 2*RADIUS};
        sdlx_render_texture(circle, NULL, &dest);
        if (s2[0] == '\0') {
            sdlx_render_printf_ex2(x, y, 
                                   fontid, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%s", s1);
        } else {
            sdlx_render_printf_ex2(x, nearbyint(y-0.5*chh), 
                                   fontid, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%s", s1);
            sdlx_render_printf_ex2(x, nearbyint(y+0.5*chh), 
                                   fontid, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%s", s2);
        }

        // register event
        loc.x = x - RADIUS;
        loc.y = y - RADIUS;
        loc.w = 2 * RADIUS;
        loc.h = 2 * RADIUS;
        sdlx_register_event(&loc, i);
    }
}

static sdlx_texture_t *create_filled_circle_texture(int radius, sdlx_color_t color)
{
    int w, h;
    sdlx_texture_t *t;

    w = h = 2 * radius;

    t = sdlx_create_texture(w, h);
    sdlx_set_render_target(t);
    sdlx_render_fill_circle(w/2, h/2, radius, color);
    sdlx_set_render_target(NULL);

    return t;
}

static void get_list_of_apps(void)
{
    struct stat statbuf;
    bool        layout_file_changed;
    bool        apps_dir_changed;
    int         rc, i, cnt, secs, n, line_num;
    FILE       *fp;
    char        str[200], s[3][100];

    static long layout_file_mtime;
    static long apps_dir_mtime;
    static bool first_call = true;

    // determine if layout file has changed
    rc = stat("apps/layout", &statbuf);
    if (rc != 0) {
        ERROR("stat apps/layout failed, %s\n", strerror(errno));
        return;
    }
    layout_file_changed = (statbuf.st_mtime != layout_file_mtime);
    layout_file_mtime = statbuf.st_mtime;

    // determine if apps dir has changed
    rc = stat("apps", &statbuf);
    if (rc != 0) {
        ERROR("stat apps failed, %s\n", strerror(errno));
        return;
    }
    apps_dir_changed = (statbuf.st_mtime != apps_dir_mtime);
    apps_dir_mtime = statbuf.st_mtime;

    // if neither layout file or apps dir has changed then return
    if (!layout_file_changed && !apps_dir_changed) {
        return;
    }

    // obtain the list of apps from the layout file ...

    // the layut file may currently being updated;
    // wait for the layout file to not have any processes having it open
    if (!first_call) {
        secs = 0;
        while (true) {
            str[0] = '\0';
            fp = popen("lsof apps/layout | wc -l", "r");
            fgets(str, sizeof(str), fp);
            pclose(fp);
            cnt = sscanf(str, "%d", &n);
            if (cnt != 1) {
                ERROR("invalid output from wc, '%s'\n", str);
                break;
            }
            if (n == 0) {
                INFO("layout file not open by any processes, secs=%d\n", secs);
                break;
            }
            if (secs >= 3) {
                ERROR("timedout waiting for layout file\n");
                break;
            }
            sleep(1);
            secs++;
        }
    }
    first_call = false;

    // free the current apps names
    for (i = 0; i < max_apps; i++) {
        free(apps[i]);
        apps[i] = NULL;
    }
    max_apps = 0;

    // read the app names, which are the same as their dir names, from the layout file
    line_num = 0;
    fp = fopen("apps/layout", "r");
    while (fgets(str, sizeof(str), fp)) {
        line_num++;

        // sanitize input string
        // - remove trailing newline
        // - remove comments
        // - remove leading spaces
        // - remove trailing spaces
        sanitize_input(str);

        // if str is empty then continue
        if (str[0] == '\0') {
            continue;
        }

        // read 3 app names from each line of the layout file
        cnt = sscanf(str, "%s %s %s", s[0], s[1], s[2]);
        if (cnt != 3) {
            ERROR("invalid line, line_num = %d\n", line_num);
            break;
        }

        // store the app names, just read, to the apps[] array;
        // ignoring app names that are "-"
        for (i = 0; i < 3; i++) {
            if (max_apps == MAX_APPS) {
                break;
            }
            if (strcmp(s[i], "-") != 0) {
                apps[max_apps] = strdup(s[i]);
            }
            max_apps++;
        }

        // if apps tbl is full then break
        if (max_apps == MAX_APPS) {
            break;
        }
    }
    fclose(fp);

    // if there are additional app dirs that are not contained in the
    // layout file, then add the app dir to the list of apps
    fp = popen("find apps -maxdepth 1 -type d | sort", "r");
    while (fgets(str, sizeof(str), fp)) {
        str[strcspn(str, "\n")] = '\0';
        char *name = basename(str);
        if (strcmp(name, "apps") == 0) {
            continue;
        }
        if (!app_name_exists(name)) {
            if (max_apps == MAX_APPS) {
                break;
            }
            INFO("adding '%s' which is not in layout fille\n", name);
            apps[max_apps++] = strdup(name);
        }
    }
    pclose(fp);

#if 0
    // debug print the list of apps names
    INFO("max_apps = %d\n", max_apps);
    for (i = 0; i < max_apps; i++) {
        if (apps[i] != NULL) {
            INFO("apps[%d] = %s\n", i, apps[i]);
        }
    }
#endif
}

static bool app_name_exists(char *name)
{
    for (int i = 0; i < max_apps; i++) {
        if (apps[i] && strcmp(apps[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static void sanitize_input(char *s)
{
    int len, i;
    char *p;

    // remove trailing newline
    len = strlen(s);
    if (len > 0 && s[len-1] == '\n') s[len-1] = '\0';

    // remove comments
    p = strchr(s, '#');
    if (p) *p = '\0';

    // remove leading spaces
    i = 0;
    while (s[i] == ' ' && s[i] != '\0') {
        i++;
    }
    memmove(s, &s[i], strlen(&s[i]));

    // remove trailing spaces
    i = strlen(s) - 1;
    while (i >= 0) {
        if (s[i] == ' ')
            s[i] = '\0';
        else
            break;
        i--;
    }
}

// -----------------  SETTINGS  -----------------------------------

static void show_file(char *filename, int orientation, int fontid);
static double get_number(char *prompt, double min, double max);

static void settings(void)
{
    // record_test_state values
    #define IDLE      0
    #define RECORDING 1
    #define PLAYBACK  2

    #define RECORD_TEST_FILENAME "record_test.mp3"

    #define EVID_README               1001
    #define EVID_LICENSES             1002
    #define EVID_CREDITS              1003
    #define EVID_DEVEL_MODE           1004
    #define EVID_DEVEL_PORT           1005
    #define EVID_DEVEL_PASSWORD       1006
    #define EVID_SERVICES             1007
    #define EVID_RECORD_GAIN          1008
    #define EVID_RECORD_SILENCE       1009
    #define EVID_RECORD_TEST          1010
    #define EVID_RESET_APPS_AND_SVCS  1011
    #define EVID_FOREGROUND           1012
    #define EVID_EVENT_BOX_ENABLE     1013
    #define EVID_ASPECT_RATIO         1014

    #define GET_Y2 ({ y2 += 2*sdlx_char_height_dflt; \
                      y2 >= y_top - 1.5 * sdlx_char_height_dflt && y2 <= y_bottom; })

    bool                done = false;
    sdlx_event_t        event;
    sdlx_loc_t         *loc;
    char                ipaddr_str[30];
    int                 record_test_state = IDLE;
    sdlx_audio_params_t ap;
    sdlx_audio_state_t  as;
    int                 y_top;
    int                 y_bottom;
    double              y;
    int                 y2;

    // get this device ipaddr
    util_get_ipaddr(ipaddr_str);

    // init variables which define the vertical region of the display
    // being used for the filename list
    y_top    = ROW2Y(4.5);
    y_bottom = sdlx_win_height;
    y        = y_top;

    // handle the setting display
    while (true) {
        // init display and font size/color
        sdlx_display_init(BG_COLOR, PORTRAIT);
        sdlx_print_set_default(FONT_NORMAL, COLOR_LIGHT_BLUE);
        y2 = nearbyint(y - 2*sdlx_char_height_dflt);

        // display Readme
        if (GET_Y2) {
            loc = sdlx_render_printf(0, y2, "Readme");
            sdlx_register_event(loc, EVID_README);
        }

        // display Licenses 
        if (GET_Y2) {
            loc = sdlx_render_printf(0, y2, "Licenses");
            sdlx_register_event(loc, EVID_LICENSES);
        }

        // display credits
        if (GET_Y2) {
            loc = sdlx_render_printf(0, y2, "Credits");
            sdlx_register_event(loc, EVID_CREDITS);
        }

        // display Services
        if (GET_Y2) {
            loc = sdlx_render_printf(0, y2, "Services");
            sdlx_register_event(loc, EVID_SERVICES);
        }

        // display Record_Gain
        if (GET_Y2) {
            sdlx_audio_get_params(&ap);
            loc = sdlx_render_printf(0, y2, "Rec_Gain = %0.2f", ap.record_gain);
            sdlx_register_event(loc, EVID_RECORD_GAIN);
        }

        // display Record_Silence
        if (GET_Y2) {
            sdlx_audio_get_params(&ap);
            loc = sdlx_render_printf(0, y2, "Rec_Silence = %0.2f", ap.record_silence);
            sdlx_register_event(loc, EVID_RECORD_SILENCE);
        }

        // display Record_Test
        if (GET_Y2) {
            sdlx_audio_get_state(&as);
            if (record_test_state == IDLE) {
                loc = sdlx_render_printf(0, y2, "Rec_Test");
                sdlx_register_event(loc, EVID_RECORD_TEST);
            } else if (record_test_state == RECORDING) {
                int bar_value_w =  sdlx_win_width * as.volume;
                int bar_height = sdlx_char_height_dflt;
                sdlx_render_printf(sdlx_win_width-COL2X(4), y2, "%4.2f", as.volume);
                sdlx_render_fill_rect(0, y2, bar_value_w, bar_height, COLOR_RED);
                sdlx_render_rect(0, y2, sdlx_win_width, bar_height, 2, COLOR_WHITE);
            } else if (record_test_state == PLAYBACK) {
                int bar_value_w = (as.play_total_secs ? (sdlx_win_width * as.play_current_secs / as.play_total_secs) : 0);
                int bar_height = sdlx_char_height_dflt;
                sdlx_render_fill_rect(0, y2, bar_value_w, bar_height, COLOR_GREEN);
                sdlx_render_rect(0, y2, sdlx_win_width, bar_height, 2, COLOR_WHITE);
            }
        }

        // display Foreground
        if (GET_Y2) {
            loc = sdlx_render_printf(0, y2, "Foreground = %s", params.foreground_enabled ? "ENABLED" : "DISABLED");
            sdlx_register_event(loc, EVID_FOREGROUND);
        }

        // display Event_Box
        if (GET_Y2) {
            loc = sdlx_render_printf(0, y2, "Event_Box = %s", params.event_box_enable ? "ENABLED" : "DISABLED");
            sdlx_register_event(loc, EVID_EVENT_BOX_ENABLE);
        }

        // display Aspect_Ratio
        if (GET_Y2) {
            loc = sdlx_render_printf(0, y2, "Aspect_Ratio = %0.3f", params.aspect_ratio);
            sdlx_register_event(loc, EVID_ASPECT_RATIO);
        }

        // display Devel_Mode
        if (GET_Y2) {
            loc = sdlx_render_printf(0, y2, "Devel_Mode = %s", params.devel_mode ? "ON" : "OFF");
            sdlx_register_event(loc, EVID_DEVEL_MODE);
        }

        // display Devel_Port
        if (GET_Y2) {
            loc = sdlx_render_printf(0, y2, "Devel_Port = %d", params.devel_port);
            sdlx_register_event(loc, EVID_DEVEL_PORT);
        }

        // display Devel_Password
        if (GET_Y2) {
            loc = sdlx_render_printf(0, y2, "Devel_Password");
            sdlx_register_event(loc, EVID_DEVEL_PASSWORD);
        }

        // display Reset_Apps_And_svcs
        if (GET_Y2) {
            loc = sdlx_render_printf(0, y2, "Reset_Apps_And_Svcs");
            sdlx_register_event(loc, EVID_RESET_APPS_AND_SVCS);
        }

        // change print color back to white
        sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);

        // Record_Test processing
        sdlx_audio_get_state(&as);
        if (record_test_state == RECORDING && (as.record_secs > 6 || as.state == AUDIO_STATE_IDLE)) {
            sdlx_audio_play_file(".", RECORD_TEST_FILENAME);
            record_test_state = PLAYBACK;
        } else if (record_test_state == PLAYBACK && as.state == AUDIO_STATE_IDLE) {
            util_delete_file(".", RECORD_TEST_FILENAME);
            record_test_state = IDLE;
        }

        // display title line, version, and ipaddr
        sdlx_render_fill_rect(0, 0, sdlx_win_width, 4*sdlx_char_height_dflt, BG_COLOR);
        sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(0),
                               FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
                               "%s", "Settings");
        sdlx_render_printf(0, ROW2Y(1), "Version = %s", VERSION);
        sdlx_render_printf(0, ROW2Y(2), "%s", BUILD_DATE);
        sdlx_render_printf(0, ROW2Y(3), "%s:%d", ipaddr_str, params.devel_port);

        // register motion and control events
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(0, NULL, 0, NULL, EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for an event, with 100 ms timeout;
        // if no event received then re-display
        sdlx_get_event(100*MS, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process the event
        switch (event.event_id) {
        case EVID_README:
            show_file("README.md", PORTRAIT, FONT_TINY);
            break;
        case EVID_LICENSES:
            show_file("LICENSE", PORTRAIT, FONT_TINY);
            break;
        case EVID_CREDITS:
            show_file("CREDITS", PORTRAIT, FONT_TINY);
            break;
        case EVID_DEVEL_MODE:
            params.devel_mode = (params.devel_mode ? 0 : 1);
            util_set_numeric_param(".", "devel_mode", params.devel_mode);
            if (!params.devel_mode) {
                INFO("sending SIGUSR2 to devel_mode_server_thread\n");
                pthread_kill(server_tid, SIGUSR2);
            }
            break;
        case EVID_DEVEL_PORT: {
            char *str; 
            int cnt, port;
            str = sdlx_get_input_str("Port\n1024 - 49151", true, NULL);
            cnt = sscanf(str, "%d", &port);
            if (cnt == 1 && (port >= 1024 && port <= 49151)) {
                params.devel_port = port;
                util_set_numeric_param(".", "devel_port", port);
                if (params.devel_mode) {
                    INFO("sending SIGUSR2 to devel_mode_server_thread\n");
                    pthread_kill(server_tid, SIGUSR2);
                }
            }
            break; }
        case EVID_DEVEL_PASSWORD: {
            char *str; 
            str = sdlx_get_input_str("Password\nMin Length 4", false, params.devel_password);
            if (strlen(str) >= MIN_DEVEL_PASSWORD_LEN) {
                strcpy(params.devel_password, str);
                util_set_str_param(".", "devel_password", str);
                sdlx_show_toast("CHANGED");
            } else {
                sdlx_show_toast("TOO SHORT");
            }
            break; }
        case EVID_SERVICES:
            svcs_display(BG_COLOR);
            break;
        case EVID_RECORD_GAIN: {
            double number = get_number("Rec_Gain", 1, 20);
            if (number != INVALID_NUMBER) {
                params.record_gain = number;
                util_set_numeric_param(".", "record_gain", number);
                sdlx_audio_get_params(&ap);
                ap.record_gain = number;
                sdlx_audio_set_params(&ap);
            }
            break; }
        case EVID_RECORD_SILENCE: {
            double number = get_number("Rec_Silence", 0, 20);
            if (number != INVALID_NUMBER) {
                params.record_silence = number;
                util_set_numeric_param(".", "record_silence", number);
                sdlx_audio_get_params(&ap);
                ap.record_silence = number;
                sdlx_audio_set_params(&ap);
            }
            break; }
        case EVID_RECORD_TEST: {
            // auto_stop_secs = 3
            // append         = false
            // start_paused   = false
            sdlx_audio_record_from_mic(".", RECORD_TEST_FILENAME, 3, false, false);
            record_test_state = RECORDING;
            break; }
        case EVID_RESET_APPS_AND_SVCS: {
            char *str; 
            str = sdlx_get_input_str(
                    "WARNING all created\n"
                    "files will be lost\n"
                    "\n"
                    "Reset y/n", 
                    false, NULL);
            if (strcasecmp(str, "y") != 0) {
                INFO("aborting RESET_APPS_AND_SVCS\n");
                break;
            }
            INFO("performing  RESET_APPS_AND_SVCS\n");
            create_files(CREATE_FILES_RESET_APPS_AND_SVCS);
            sdlx_show_toast("RESET DONE");
            break; }
        case EVID_FOREGROUND: {
            params.foreground_enabled = (params.foreground_enabled ? false : true);
            util_set_numeric_param(".", "foreground_enabled", params.foreground_enabled);
            if (params.foreground_enabled) {
                if (perm_granted.access_coarse_location) {
                    util_start_foreground();
                } else {
                    ERROR("permission has not been granted to access_coarse_location\n");
                }
            } else {
                util_stop_foreground();
            }
            break; }
        case EVID_EVENT_BOX_ENABLE: {
            params.event_box_enable = (params.event_box_enable ? false : true);
            util_set_numeric_param(".", "event_box_enable", params.event_box_enable);
            sdlx_event_box_ctrl(params.event_box_enable);
            break; }
        case EVID_ASPECT_RATIO: {
            char *str, dflt_input_str[100];
            int cnt;
            double aspect_ratio;
            sprintf(dflt_input_str, "%0.3f", params.aspect_ratio);
            str = sdlx_get_input_str("Aspect_Ratio\n2.0 - 2.4\nezApp restart needed", true, dflt_input_str);
            cnt = sscanf(str, "%lf", &aspect_ratio);
            if (cnt == 1 && (aspect_ratio >= 2.0 && aspect_ratio <= 2.4)) {
                params.aspect_ratio = aspect_ratio;
                util_set_numeric_param(".", "aspect_ratio", aspect_ratio);
            }
            // xxx show toat on errors
            break; }
        case EVID_MOTION:
            y += event.u.motion.yrel;
            if (y >= y_top) {
                y = y_top;
            }
            break;
        case EVID_QUIT:
            if (record_test_state != IDLE) {
                sdlx_audio_stop();
                record_test_state = IDLE;
            }
            done = true;
            break;
        }

        if (done) {
            break;
        }
    }
}

static double get_number(char *prompt_arg, double min, double max)
{
    char  *input_str; 
    double number;
    char   prompt[100];

    sprintf(prompt, "%s\n%0.1f - %0.1f", prompt_arg, min, max);

    input_str = sdlx_get_input_str(prompt, true, NULL);
    if (sscanf(input_str, "%lf", &number) != 1) {
        return INVALID_NUMBER;
    }
    if (min < max) {
        if (number < min) number = min;
        if (number > max) number = max;
    }
    return number;
}

static void show_file(char *filename, int orientation, int fontid)
{
    double      x, y;
    int         y_top, y_bottom;
    int         len;
    sdlx_event_t event;
    bool        done = false;
    char       *lines[1];

    // read the file
    lines[0] = util_read_file(".", filename, &len);
    if (lines[0] == NULL) {
        ERROR("failed to read %s\n", filename);
        return;
    }

    // select font
    sdlx_print_set_default(fontid, COLOR_BLACK);

    // init vars
    x        = 0;
    y_top    = 0;
    y_bottom = sdlx_win_height;
    y        = y_top;

    // display filename lines
    while (true) {
        // display file lines and register for motion (scrolling) & exit events
        sdlx_display_init(COLOR_WHITE, orientation);
        sdlx_render_multiline_text(x, y, y_top, y_bottom, fontid, lines, NULL, 1);
        sdlx_register_control_events(0, NULL, 0, NULL, EVID_QUIT, "X");
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_display_present();

        sdlx_get_event(-1, &event);
        if (event.event_id == -1) {
            continue;
        }

        switch (event.event_id) {
        case EVID_MOTION: {
            double xrel = event.u.motion.xrel;
            double yrel = event.u.motion.yrel;

            if (fabs(xrel) > fabs(yrel)*1.5) x += xrel;
            if (fabs(yrel) > fabs(xrel)*1.5) y += yrel;

            if (y >= y_top) y = y_top;
            if (x > 0) x = 0;
            break; }
        case EVID_QUIT:
            done = true;
            break;
        }

        if (done) {
            break;
        }
    }

    // free allocation
    free(lines[0]);
}

// ----------------- DEVELOPER MODE SERVER  ----------------

#define MAX_PID_TBL 20

static int process_req_thread(void *cx);

static int devel_mode_server_thread(void *cx)
{
    struct sockaddr_in server_address;
    int                listen_sockfd, ret;

    // save server thread id in global, so that signals can be sent to this thread
    server_tid = pthread_self();

again:
    // wait for developer mode to be enabled
    INFO("waiting for devel_mode enabled\n");
    sleep(1);
    while (params.devel_mode == false) {
        sleep(1);
    }
    INFO("server starting, listening on port %d\n", params.devel_port);

    // create listen socket
    listen_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sockfd == -1) {
        ERROR("socket, %s\n", strerror(errno));
        return 0;
    }

    // set socket options
    int reuseaddr = 1;
    ret = setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseaddr, sizeof(reuseaddr));
    if (ret == -1) {
        ERROR("setsockopt SO_REUSEADDR, %s\n", strerror(errno));
        return 0;
    }

    // bind socket to any ip addr, for specified port
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family      = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port        = htons(params.devel_port);
    ret = bind(listen_sockfd,
               (struct sockaddr *)&server_address,
               sizeof(server_address));
    if (ret == -1) {
        ERROR("bind, %s\n", strerror(errno));
        close(listen_sockfd);
        sleep(1);
        goto again;
    }

    // listen 
    ret = listen(listen_sockfd, 5);
    if (ret == -1) {
        ERROR("listen, %s\n", strerror(errno));
        return 0;
    }

    // accept and process connections
    INFO("accepting connections\n");
    while (1) {
        int                sockfd;
        struct sockaddr_in peer_addr;
        socklen_t          peer_addr_len;

        // accept connection
        peer_addr_len = sizeof(peer_addr);
        sockfd = accept(listen_sockfd, (struct sockaddr *) &peer_addr, &peer_addr_len);
        if (sockfd == -1) {
            ERROR("accept, %s\n", strerror(errno));
            break;
        }

        // create thread to process the client request
        sdlx_create_detached_thread(process_req_thread, "proc_req", (void*)(long)sockfd);
    }

    // close listen socket
    close(listen_sockfd);

    // goto top to reinit devel_mode_server_thread
    goto again;
    return 0;
}

int put_fmt(FILE *fp, char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = vfprintf(fp, fmt, ap);
    va_end(ap);

    if (rc < 0) {
        //ERROR("vfprintf faield, %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

char *get_str(FILE *fp, char *s, int s_len)
{
    char *p;
    int len;

    s[0] = '\0';

    p = fgets(s, s_len, fp);
    if (p == NULL) {
        //ERROR("fget failed, %s\n", strerror(errno));
        return NULL;
    }

    len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }

    return s;
}

static int process_req_thread(void *cx)
{
    int           sockfd = (int)(long)cx;
    FILE         *sockfp;
    ssl_payload_t payload;
    int           rc;

    // get fp for socket, and set unbuffered
    sockfp = fdopen(sockfd, "w+");
    setvbuf(sockfp, NULL, _IONBF, 0);
    
    // read the ssl payload, which contains the encrypted password
    rc = fread(&payload, 1, sizeof(payload), sockfp);
    if (rc != sizeof(payload)) {
        ERROR("fread payload failed, %s\n", strerror(errno));
        goto disconnect;
    }

    // if the ezApp devel_password has not been initialized then 
    // respond to ezsh with error message
    if (strcmp(params.devel_password, DEFAULT_DEVEL_PASSWORD) == 0 ||
        strlen(params.devel_password) < MIN_DEVEL_PASSWORD_LEN) 
    {
        ERROR("ezApp password needs to be set\n");
        put_fmt(sockfp, "%s\n", "ezApp password needs to be set");
        goto disconnect;
    }

    // decrypt the password from the ssl payload
    unsigned char *key;
    char          *plaintext;
    key = ssl_keygen(params.devel_password);
    rc = ssl_decrypt(key, &payload, &plaintext);
    if (rc != 0) {
        ERROR("invalid password\n");
        put_fmt(sockfp, "%s\n", "invalid password");
        goto disconnect;
    }

    // validate password
    if (strcmp(plaintext, params.devel_password) == 0) {
        rc = put_fmt(sockfp, "%s\n", "password okay");
        if (rc != 0) goto disconnect;
    } else {
        ERROR("invalid password 2");
        put_fmt(sockfp, "%s\n", "invalid password 2");
        goto disconnect;
    }

    // put storage_path to socket
    rc = put_fmt(sockfp, "%s\n", storage_path);
    if (rc != 0) goto disconnect;

    // process cmds
    while (true) {
        char str[1000];
        int  status=99;

        // read from socket
        // - if socket has been closed on other end then goto disconnect
        // - else verify 'run' is received
        if (get_str(sockfp, str, sizeof(str)) == NULL) {
            goto disconnect;
        }
        if (strcmp(str, "run") != 0) {
            ERROR("'run' expected\n");
            goto disconnect;
        }

        // read cmdline from socket
        if (get_str(sockfp, str, sizeof(str)) == NULL) {
            goto disconnect;
        }

        // the following cmdline are handled by this code;
        // - put        : create/update file on android device, arg=file_path
        // - get        : get file contents, arg=file_path
        // otherwise the cmdline is passed to popen for the 
        // android shell to process
        //
        // status return is either a negative errno, or a positive exit code
        if (strncmp(str, "put ", 4) == 0) {
            char *data, *p;
            int   data_len, rc, cnt;
            DIR  *dir;
            char  dest_path[200], src_filename[200];

            // extract dest_path and src_filename from str
            cnt = sscanf(str, "put %s %s", dest_path, src_filename);
            if (cnt != 2) {
                ERROR("dest_path and src_filename both required\n");
                goto disconnect;
            }

            // if dest_path is a directory then append src_filename
            dir = opendir(dest_path);
            if (dir != NULL) {
                int len = strlen(dest_path);
                if (len > 0 && dest_path[len-1] != '/') {
                    strcat(dest_path, "/");
                }
                strcat(dest_path, src_filename);
                closedir(dir);
            }

            // read data_len from sockfp
            p = get_str(sockfp, str, sizeof(str));
            if (p == NULL || sscanf(str, "data_len %d", &data_len) != 1) {
                ERROR("failed to get data_len\n");
                goto disconnect;
            }

            // read data from socket
            data = calloc(data_len, 1);             // nmemb=data_len, size=1
            status = fread(data, 1, data_len, sockfp);  // size=1, nmemb=data_len
            if (status != data_len) {
                ERROR("failed to read data from socket\n");
                free(data);
                goto disconnect;
            }

            // write data to android file
            rc = util_write_file(dest_path, NULL, data, data_len);
            status = (rc == 0 ? 0 : errno != 0 ? -errno : -EINVAL);

            // free allocated data
            free(data);
        } else if (strncmp(str, "get ", 4) == 0) {
            char  src_path[200];
            char *data;
            int   data_len, rc;

            // save src_path
            strcpy(src_path, str+4);

            // read android file
            data = util_read_file(src_path, NULL, &data_len);
            if (data == NULL) {
                // failed to read file
                status = (errno != 0 ? -errno : -EINVAL);
                rc = put_fmt(sockfp, "data_len %d\n", 0);
                if (rc != 0) goto disconnect;
            } else {
                // write data_len to socket
                rc = put_fmt(sockfp, "data_len %d\n", data_len);
                if (rc != 0) goto disconnect;

                // write data to socket
                rc = fwrite(data, 1, data_len, sockfp);  // size=1, nmemb=data_len
                if (rc != data_len) {
                    ERROR("failed to write data to socket\n");
                    free(data);
                    goto disconnect;
                }

                // free data, and set success status
                free(data);
                status = 0;
            }
        } else if (strcmp(str, "quiesced") == 0) {
            status = ((!is_app_running() ? 0 : 1) + (num_svcs_running() == 0 ? 0 : 10));
        } else {
            FILE *fp;
            int rc;

            fp = popen(str, "r");
            if (fp == NULL) {
                status = (errno != 0 ? -errno : -EINVAL);
                ERROR("popen '%s' failed, %s\n", str, strerror(errno));
            } else {
                while (get_str(fp, str, sizeof(str)) != NULL) {
                    rc = put_fmt(sockfp, "%s\n", str);
                    if (rc != 0) {
                        pclose(fp);
                        goto disconnect;
                    }
                }
                rc = pclose(fp);
                status = WEXITSTATUS(rc);
            }
        }

        // all cmds termintate with CMD_COMPLETE <status>
        rc = put_fmt(sockfp, "CMD_COMPLETE %d\n", status);
        if (rc != 0) goto disconnect;
    }

disconnect:
    fclose(sockfp);
    return 0;
}
