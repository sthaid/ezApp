#include <std_hdrs.h>
#include <sys/queue.h>

#include <sdlx.h>
#include <utils.h>
#include <private.h>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

//
// defines
// 

#define FONT_FILE_PATH  "fonts/FreeMonoBold.ttf"

#define ONE_MS 1000
#define TEN_MS 10000

#define MIN_FONT_PTSIZE  10
#define MAX_FONT_PTSIZE  400

#define LOGICAL_WIN_WIDTH   1000
#define LOGICAL_WIN_HEIGHT  2150

//
// typedefs
//

typedef struct {
    sdlx_loc_t loc;
    int       event_id;
} event_t;

typedef struct {
    TTF_Font *font;
    int       chw;
    int       chh;
} font_t;

//
// global variables
//

int sdlx_win_width;
int sdlx_win_height;
int sdlx_char_width_dflt;
int sdlx_char_height_dflt;

//
// variables
//

SDL_Window          *window;  // needed by sdlx_misc.c

static SDL_Renderer *renderer;
static font_t        font[MAX_FONT_PTSIZE];
sdlx_texture_t      *texture_display;
int                  orientation;
int                  real_win_width, real_win_height;
int                  logical_win_width, logical_win_height;
int                  logical_win_width_portrait, logical_win_height_portrait;
int                  logical_win_width_landscape, logical_win_height_landscape;
double               scale_events_x;
double               scale_events_y;

//
// prototypes
//

static void set_render_draw_color(sdlx_color_t color);

//
// inline routines
//

// this routine is used, instead of type punning, 
// to prevent strict-aliasing warnings
static inline SDL_Color sdlx_color(sdlx_color_t color)
{
    SDL_Color val;
    memcpy(&val, &color, sizeof(color));
    return val;
}

// ----------------- INIT / EXIT --------------------------

static bool event_watcher(void* userdata, SDL_Event* event);

int sdlx_video_init(void)
{
    int w, h;

    INFO("initializing\n");

#if 0
    // commented out because this is not that an important feature, and
    // enabling this will reduce battery life 
    //
    // disable block on pause;
    // this allows the ColrOrgn app to continue audio playback when ezApp is backgrounded
    bool succ = SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
    if (!succ) {
        ERROR("disable SDL_HINT_ANDROID_BLOCK_ON_PAUSE failed\n");
    }
#endif

    // initialize SDL video
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        ERROR("SDL_Init VIDEO failed, %s\n", SDL_GetError());
        return -1;
    }

    // display list of video drivers
    int num, i;
    num = SDL_GetNumVideoDrivers();
    INFO("Current Video Drivers: %s", SDL_GetCurrentVideoDriver());
    INFO("Available Video Drivers: ");
    for (i = 0; i < num; i++) {
        INFO("   %s\n",  SDL_GetVideoDriver(i));
    }

#ifdef ANDROID
    // Android code to create SDL Window and Renderer

    // AI query:
    //   in SDL3 when SDL_CreateWindowAndRenderer is called with flags=0 on recent 
    //   android device, the bottom area of the display is obstructed
    // AI answer:
    //   The problem you are experiencing occurs because Android 15 (API 35+) enforces 
    //   "Edge-to-Edge" rendering by default for all apps targeting the modern SDK.
    //   When you pass flags = 0 to SDL_CreateWindowAndRenderer, SDL3 creates a standard 
    //   window that spans the device's entire hardware dimensions. Because of the modern 
    //   Android operating system behavior, your game or application content will render 
    //   directly underneath the translucent OS navigation gesture bar or 3-button layout 
    //   at the bottom of the screen.

    // My devices
    //   DEVNAME    API     ANDROID     USE_FLAGS
    //   -------    ---     -------     ---------
    //   samsung    36        16        SDL_WINDOW_FULLSCREEN
    //   motorola   30        11        0

    int sdk_version = SDL_GetAndroidSDKVersion();
    int flags;

    INFO("sdk_version = %d\n", sdk_version);
    if (sdk_version >= 35) {
        INFO("flags = SDL_WINDOW_FULLSCREEN\n");
        flags = SDL_WINDOW_FULLSCREEN;
    } else {
        INFO("flags = 0\n");
        flags = 0;
    }

    if (!SDL_CreateWindowAndRenderer("ezApp", 0, 0, flags, &window, &renderer)) {
        ERROR("SDL_CreateWindowAndRenderer failed\n");
        return -1;
    }
#else
    // Linux code to create SDL Window and Renderer

    // use Window with aspect ratio 2.1666  (19.5:9)
    if (!SDL_CreateWindowAndRenderer("ezApp", 450, 975, 0, &window, &renderer)) {
        ERROR("SDL_CreateWindowAndRenderer failed\n");
        return -1;
    }
#endif

    // Aspect ratio ...
    // * Modern android devices use taller aspect ratios, such as:
    //   - 19.5:9   2.1666   (most common, my device)
    //   - 20:9     2.2222   (most common)
    //   - 20.5:9   2.2777
    // * ezApp was developed on an Android device with 
    //   display size of WxH = 1080 x 2340   (aspect_ratio = 2.1666).
    // * For best results ezApp should be run on a device with
    //   one of these taller aspect ratios.
    // * ezApp scales the display size to WxH = 1000x2150; aspect ratio 2.15
    // * 150 pixels at the bottom of the display are reserved space for the master controls.
    // * miniApps that run within ezApp should assume a fixed logical
    //   display size of 1000 x 2000. This size excludes the master control area.
    //
    // When landscape mode is selected the logical display area becomes 2000 x 1000.
    // The orientation is selected by apps when calling sdlx_display_init().
    // Since apps call sdlx_display_init periodically, the apps can dynamically change
    // screen orientation.
    //
    // These global variables should be used for display size.
    // These varaibles are initialized by the sdlx_display_init routine, to 
    // the following values:
    //                   PORTRAIT   LANDSCAPE
    // sdlx_win_width  =  1000        2000
    // sdlx_win_height =  2000        1000

    // get real windows size and aspect ratio
    SDL_GetWindowSize(window, &real_win_width, &real_win_height);
    INFO("real    win_width x height = %d %d  aspect = %f\n", 
         real_win_width, real_win_height, 
         (double)real_win_height / real_win_width);

    // sanity check
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
    if (real_win_width != w || real_win_height != h) {
        ERROR("real_win_width/height = %d %d differs from GetCurrentRenderOutputSize %d %d\n",
              real_win_width, real_win_height, w, h);
    }

    // init the logical window size for portrait and landscape orientations
    logical_win_width_portrait   = LOGICAL_WIN_WIDTH;
    logical_win_height_portrait  = LOGICAL_WIN_HEIGHT;
    logical_win_width_landscape  = logical_win_height_portrait;
    logical_win_height_landscape = logical_win_width_portrait;

    // start in PORTRAIT mode
    orientation = PORTRAIT;
    logical_win_width  = logical_win_width_portrait;
    logical_win_height = logical_win_height_portrait;
    sdlx_win_width     = logical_win_width;
    sdlx_win_height    = logical_win_height - CONTROL_AREA_SIZE;
    INFO("logical win_width x height = %d %d  aspect = %f\n", 
         logical_win_width, logical_win_height, 
         (double)logical_win_height / logical_win_width);

    // init scale factors used by sdlx_event.c
    scale_events_x = (double)real_win_width / logical_win_width;
    scale_events_y = (double)real_win_height / logical_win_height;
    INFO("scale_events x,y = %f %f\n", scale_events_x, scale_events_y);

    // add the event watcher, which is currently only used for debug purpose
    SDL_AddEventWatch(event_watcher, NULL);

    // initialize True Type Font
    if (!TTF_Init()) {
        ERROR("TTF_Init failed\n");
        return -1;
    }

    // init default fontsize, where value of FONT_NORMAL is num chars across display;
    // and validate expected character size and columns
    sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);
    INFO("sdlx_print_set(%d) char_width=%d char_height=%d\n", FONT_NORMAL, sdlx_char_width_dflt, sdlx_char_height_dflt);
    if (sdlx_char_width_dflt != 50 || sdlx_char_height_dflt != 83) {
        ERROR("chw,chh, expected = 50,83  actual = %d,%d\n", sdlx_char_width_dflt, sdlx_char_height_dflt);
    }

    // enable alpha blending
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // workaround: needed so that the first actual display present works
    sdlx_display_init(COLOR_BLACK, PORTRAIT);
    sdlx_display_present();
    usleep(50000);
    sdlx_event_t event;
    sdlx_get_event(0, &event);

    // return success
    INFO("success\n");
    return 0;
}

void sdlx_video_quit(void)
{
    int i;

    INFO("quitting\n");

    // close fonts
    for (i = 0; i < MAX_FONT_PTSIZE; i++) {
        if (font[i].font != NULL) {
            TTF_CloseFont(font[i].font);
        }
    }
    TTF_Quit();

    // destroy the renderer and window
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    // quit SDL video
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void sdlx_minimize_window(void)
{   
    SDL_MinimizeWindow(window);
}

static bool event_watcher(void* userdata, SDL_Event* event)
{
    switch (event->type) {
    case SDL_EVENT_WILL_ENTER_BACKGROUND:
        // pause here, if needed
        INFO("about to be backgrounded\n");
        break;
    case SDL_EVENT_DID_ENTER_BACKGROUND:
        INFO("now in the background\n");
        break;
    case SDL_EVENT_WILL_ENTER_FOREGROUND:
        INFO("about to be foregrounded\n");
        break;
    case SDL_EVENT_DID_ENTER_FOREGROUND:
        // resume here, if needed
        INFO("now in the foreground\n");
        break;
    default:
        break;
    }
    return 0;
}

// ----------------- DISPLAY INIT / PRESENT ---------------

void sdlx_display_init(sdlx_color_t color, int orientation_arg)
{
    static int texture_orientation = -1;

    // sdlx_audio needs to be called periodically from the main thread
    sdlx_audio_main_thread_periodic();

    // clear existing event registrations from last cycle
    sdlx_reset_events();

    // set global display orientation variable
    orientation = orientation_arg;

    // if the display rendering texture has not yet been allocated, or
    // the display orientation has changed, then ...
    if (!texture_display || (texture_orientation != orientation)) {
        // destroy the current display rendering texture
        sdlx_destroy_texture(texture_display);
        texture_display = NULL;

        // based on new display orientation:
        // - set the logical width/height variables
        // - create new display rendering texture with size equal to the 
        //   logical_win_width/height
        // - init the sdlx_win_width/height; this is the same as the 
        //   logical_win_width/height, except for not including the CONTROL_AREA_SIZE
        if (orientation == PORTRAIT) {
            logical_win_width = logical_win_width_portrait;
            logical_win_height = logical_win_height_portrait;
            texture_display = sdlx_create_texture(logical_win_width, logical_win_height);
            texture_orientation = PORTRAIT;
            sdlx_win_width  = logical_win_width;
            sdlx_win_height = logical_win_height - CONTROL_AREA_SIZE;
        } else {
            logical_win_width = logical_win_width_landscape;
            logical_win_height = logical_win_height_landscape;
            texture_display = sdlx_create_texture(logical_win_width, logical_win_height);
            texture_orientation = LANDSCAPE;
            sdlx_win_width  = logical_win_width - CONTROL_AREA_SIZE;
            sdlx_win_height = logical_win_height;
        }
    }

    // set texture_display as the rendering target
    SDL_SetRenderTarget(renderer, (SDL_Texture*)texture_display);

    // clear the texture_display to the caller supplied color
    sdlx_clear_texture(texture_display, color);
}

void sdlx_display_present(void)
{
    // set rendering target to the display
    SDL_SetRenderTarget(renderer, NULL);

    // render the texture_display to the display;
    // when orientation is landscpe, the texture_display is rotated by 90 degrees
    if (orientation == PORTRAIT) {
        sdlx_render_texture(texture_display, NULL, NULL);
    } else {
        sdlx_loc_t dest = {0, 0, real_win_height, real_win_width};
        sdlx_point_t center = {real_win_width/2, real_win_width/2};
        sdlx_render_texture_rotated(texture_display, NULL, &dest, 90, &center, FLIP_NONE);
    }

    // present the display
    SDL_RenderPresent(renderer);
}

// -----------------  COLORS  -----------------------------

sdlx_color_t sdlx_create_color(int r, int g, int b, int a)
{
    return (r << 0) | (g << 8) | (b << 16) | (a << 24);
}

sdlx_color_t sdlx_scale_color(sdlx_color_t color, double inten)
{
    unsigned int r = (color >> 0) & 0xff;
    unsigned int g = (color >> 8) & 0xff;
    unsigned int b = (color >> 16) & 0xff;
    unsigned int a = (color >> 24) & 0xff;

    if (inten < 0) inten = 0;
    if (inten > 1) inten = 1;

    r *= inten;
    g *= inten;
    b *= inten;

    return (r << 0) | (g << 8) | (b << 16) | (a << 24);
}

sdlx_color_t sdlx_set_color_alpha(sdlx_color_t color, int alpha)
{
    return (color & 0x00ffffff) | ((alpha & 0xff) << 24);
}

// ported from http://www.noah.org/wiki/Wavelength_to_RGB_in_Python
sdlx_color_t sdlx_wavelength_to_color(int wavelength_arg)
{
    double wavelength = wavelength_arg;
    double attenuation;
    double gamma = 0.8;
    double R,G,B;

    if (wavelength >= 380 && wavelength <= 440) {
        double attenuation = 0.3 + 0.7 * (wavelength - 380) / (440 - 380);
        R = pow((-(wavelength - 440) / (440 - 380)) * attenuation, gamma);
        G = 0.0;
        B = pow(1.0 * attenuation, gamma);
    } else if (wavelength >= 440 && wavelength <= 490) {
        R = 0.0;
        G = pow((wavelength - 440) / (490 - 440), gamma);
        B = 1.0;
    } else if (wavelength >= 490 && wavelength <= 510) {
        R = 0.0;
        G = 1.0;
        B = pow(-(wavelength - 510) / (510 - 490), gamma);
    } else if (wavelength >= 510 && wavelength <= 580) {
        R = pow((wavelength - 510) / (580 - 510), gamma);
        G = 1.0;
        B = 0.0;
    } else if (wavelength >= 580 && wavelength <= 645) {
        R = 1.0;
        G = pow(-(wavelength - 645) / (645 - 580), gamma);
        B = 0.0;
    } else if (wavelength >= 645 && wavelength <= 750) {
        attenuation = 0.3 + 0.7 * (750 - wavelength) / (750 - 645);
        R = pow(1.0 * attenuation, gamma);
        G = 0.0;
        B = 0.0;
    } else {
        R = 0.0;
        G = 0.0;
        B = 0.0;
    }

    if (R < 0) R = 0; else if (R > 1) R = 1;
    if (G < 0) G = 0; else if (G > 1) G = 1;
    if (B < 0) B = 0; else if (B > 1) B = 1;

    return sdlx_create_color(R*255, G*255, B*255, 255);
}

static void set_render_draw_color(sdlx_color_t color)
{
    unsigned int r = (color >> 0) & 0xff;
    unsigned int g = (color >> 8) & 0xff;
    unsigned int b = (color >> 16) & 0xff;
    unsigned int a = (color >> 24) & 0xff;

    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

// -----------------  RENDER TEXT  ------------------------

// - - - - - - - - - font create - - - - - - - - - - -

static void font_create(int ptsize)
{
    font_t *f;
    TTF_Font *fnt;

    // validate ptsize
    if (ptsize < MIN_FONT_PTSIZE || ptsize >= MAX_FONT_PTSIZE) {
        ERROR("ptsize %d out of range\n", ptsize);
        return;
    }

    // if font already exists then return
    f = &font[ptsize];
    if (f->font != NULL) {
        return;
    }

    // open TTF font
    fnt = TTF_OpenFont(FONT_FILE_PATH, ptsize);
    if (fnt == NULL) {
        ERROR("TTF_OpenFont failed, ptsize=%d, %s\n", ptsize, SDL_GetError());
        return;
    }

    // initialize the font structure
    f->font = fnt;
    f->chh  = nearbyint(ptsize);
    f->chw  = nearbyint(f->chh * 0.6);
}

// - - - - - - - - - set print default - - - - - - - - - - - 

static struct {
    int          fontid;
    sdlx_color_t color;
} print_dflt;

void sdlx_print_set_default(int fontid, sdlx_color_t color)
{
    int ptsize;

    // save print default settings
    print_dflt.fontid = fontid;
    print_dflt.color  = color;

    // set global variables containing the char width,height of the default font
    ptsize = (logical_win_width_portrait / fontid) / 0.6;
    sdlx_char_height_dflt = nearbyint(ptsize);
    sdlx_char_width_dflt  = nearbyint(sdlx_char_height_dflt * 0.6);
}

// - - - - - - - - - get char width/height for fontid - - - -

int sdlx_char_width(int fontid)
{
    int ptsize, chh, chw;

    // return char width, based on fontid
    ptsize = (logical_win_width_portrait / fontid) / 0.6;
    chh = nearbyint(ptsize);
    chw  = nearbyint(chh * 0.6);
    return chw;
}

int sdlx_char_height(int fontid)
{
    int ptsize, chh;

    // return char height, based on fontid
    ptsize = (logical_win_width_portrait / fontid) / 0.6;
    chh = nearbyint(ptsize);
    return chh;
}

// - - - - - - - - - render text - - - - - - - - - - - - - 

#define MAX_HASH_LIST   293  // using a prime number is recomended
#define MAX_HASH_ENTRY  200

static TAILQ_HEAD(age_list, ht_entry_s)   age_list_head;
static TAILQ_HEAD(hash_list, ht_entry_s)  hash_list_head[MAX_HASH_LIST];

typedef struct ht_entry_s {
    char                   *key;
    SDL_Texture            *texture;
    int                     surface_w;
    int                     surface_h;
    TAILQ_ENTRY(ht_entry_s) age_list_entry;
    TAILQ_ENTRY(ht_entry_s) hash_list_entry;
} ht_entry_t;

static void render_text_texture(SDL_Texture *t, SDL_FRect *pos, unsigned int flags)
{
    #define SWAP_FLOAT(a,b) \
        do { float tmp = (a); (a) = (b); (b) = tmp; } while (0)

    if (flags & FLAG_ROT_CTR_90) {
        SDL_RenderTextureRotated(renderer,
                                 t, 
                                 NULL,      // source, NULL means the entire texture
                                 pos,       // dest rectangle
                                 90,        // rotation angle
                                 NULL,      // point around which dest will be rotated
                                 SDL_FLIP_NONE);
        pos->x = pos->x + pos->w/2 - pos->h/2;
        pos->y = pos->y + pos->h/2 - pos->w/2;
        SWAP_FLOAT(pos->w, pos->h);
    } else if (flags & FLAG_ROT_CTR_180) {
        SDL_RenderTextureRotated(renderer,
                                 t, 
                                 NULL,      // source, NULL means the entire texture
                                 pos,       // dest rectangle
                                 180,       // rotation angle
                                 NULL,      // point around which dest will be rotated
                                 SDL_FLIP_NONE);
    } else if (flags & FLAG_ROT_CTR_270) {
        SDL_RenderTextureRotated(renderer,
                                 t, 
                                 NULL,      // source, NULL means the entire texture
                                 pos,       // dest rectangle
                                 270,       // rotation angle
                                 NULL,      // point around which dest will be rotated
                                 SDL_FLIP_NONE);
        pos->x = pos->x + pos->w/2 - pos->h/2;
        pos->y = pos->y + pos->h/2 - pos->w/2;
        SWAP_FLOAT(pos->w, pos->h);
    } else {
        SDL_RenderTexture(renderer, t, NULL, pos);
    }
}

static unsigned int calc_hash_idx(char *key)
{
    unsigned int hash_value = 0;

    for (int i = 0; key[i] != '\0'; i++) {
        hash_value = hash_value * 31 + key[i];
    }

    return (hash_value % MAX_HASH_LIST);
}

static sdlx_loc_t *render_text(int x, int y, int fontid, sdlx_color_t color, unsigned int flags, char *str)
{
    char         key[1000];
    ht_entry_t  *entry;
    SDL_FRect    pos;
    bool         found;
    int          hash_idx, ptsize;
    SDL_Surface *surface = NULL;
    SDL_Texture *texture = NULL;

    static sdlx_loc_t loc;
    static int        num_allocated = 0;

    // if font has not been created then do so
    ptsize = (logical_win_width_portrait / fontid) / 0.6;
    if (font[ptsize].font == NULL) {
        font_create(ptsize);
        if (font[ptsize].font == NULL) {
            ERROR("failed to create font ptsize %d for fontid %d\n", ptsize, fontid);
            loc.x = loc.y = loc.w = loc.h = 0;
            return &loc;
        } else {
            INFO("created font ptsize %d for fontid %d\n", ptsize, fontid);
        }
    }

    // if zero length string, which will fail to print, then
    // set the str arg to single space char
    if (str[0] == '\0') {
        str = " ";
    }

    // calculate hash key and idx
    snprintf(key, sizeof(key), "%x-%d-%x-%s", color, ptsize, flags, str);
    hash_idx = calc_hash_idx(key);

    // search the hash_list_head[hash_idx] for entry matching hash key
    found = false;
    TAILQ_FOREACH(entry, &hash_list_head[hash_idx], hash_list_entry) {
        if (strcmp(entry->key, key) == 0) {
            found = true;
            break;
        }
    }

    // matching entry found, render it, and return
    if (found) {
        pos.x = x;
        pos.y = y;
        pos.w = entry->surface_w;
        pos.h = entry->surface_h;
        if (flags & FLAG_X_CTR) pos.x -= pos.w / 2;
        if (flags & FLAG_Y_CTR) pos.y -= pos.h / 2;
        render_text_texture(entry->texture, &pos, flags);

        TAILQ_REMOVE(&age_list_head, entry, age_list_entry);
        TAILQ_INSERT_HEAD(&age_list_head, entry, age_list_entry);

        goto return_loc;
    }

    // hash table entry not found ...

    // if number of textures allocated is max then
    //   discard the least recently used texture
    // endif
    if (num_allocated >= MAX_HASH_ENTRY) {
        entry = TAILQ_LAST(&age_list_head, age_list);
        TAILQ_REMOVE(&age_list_head, entry, age_list_entry);
        TAILQ_REMOVE(&hash_list_head[hash_idx], entry, hash_list_entry);
        SDL_DestroyTexture(entry->texture);
        free(entry->key);
        free(entry);
        num_allocated--;
    }

    // create surface containing the rendered text
    if (flags & FLAG_BG_BLACK) {
        surface = TTF_RenderText_Shaded_Wrapped(
                        font[ptsize].font, str, 0,
                        sdlx_color(color), sdlx_color(COLOR_BLACK),
                        flags & FLAG_WRAP_MASK);
    } else if (flags & FLAG_BG_WHITE) {
        surface = TTF_RenderText_Shaded_Wrapped(
                        font[ptsize].font, str, 0,
                        sdlx_color(color), sdlx_color(COLOR_WHITE),
                        flags & FLAG_WRAP_MASK);
    } else {
        surface = TTF_RenderText_Solid_Wrapped(
                        font[ptsize].font, str, 0,
                        sdlx_color(color),
                        flags & FLAG_WRAP_MASK);
    }
    if (surface == NULL) {
        ERROR("TTF_RenderTextSolid failed, %s\n", SDL_GetError());
        goto return_error;
    }

    // create texture from the surface
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture == NULL) {
        ERROR("SDL_CreateTextureFromSurface failed, %s\n", SDL_GetError());
        goto return_error;
    }

    // allocate and init new hash table entry
    entry = malloc(sizeof(ht_entry_t));
    entry->key = strdup(key);
    entry->texture = texture;
    entry->surface_w = surface->w;
    entry->surface_h = surface->h;
    TAILQ_INSERT_HEAD(&age_list_head, entry, age_list_entry);
    TAILQ_INSERT_HEAD(&hash_list_head[hash_idx], entry, hash_list_entry);
    num_allocated++;

    // render the texture
    pos.x = x;
    pos.y = y;
    pos.w = surface->w;
    pos.h = surface->h;
    if (flags & FLAG_X_CTR) pos.x -= pos.w / 2;
    if (flags & FLAG_Y_CTR) pos.y -= pos.h / 2;
    render_text_texture(texture, &pos, flags);

    // the surface is no longer needed, destroy the surface
    SDL_DestroySurface(surface);
    surface = NULL;

    // return the display location where the text was rendered;
return_loc:
    loc.x = pos.x;
    loc.y = pos.y;
    loc.w = pos.w;
    loc.h = pos.h;
    return &loc;

    // error return path
return_error:
    if (surface) {
        SDL_DestroySurface(surface);
        surface = NULL;
    }
    loc.x = 0;
    loc.y = 0;
    loc.w = 0;
    loc.h = 0;
    return &loc;
}

// - - - - - - - - -  print api routines - - - - - - - - - 

sdlx_loc_t *sdlx_render_printf(int x, int y, char * fmt, ...)
{
    char str[1000];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);

    return render_text(x, y, print_dflt.fontid, print_dflt.color, 0, str);
}

sdlx_loc_t *sdlx_render_printf_ex1(int x, int y, int fontid, sdlx_color_t color, char * fmt, ...)
{
    char str[1000];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);

    return render_text(x, y, fontid, color, 0, str);
}

sdlx_loc_t *sdlx_render_printf_ex2(int x, int y, int fontid, sdlx_color_t color, unsigned int flags, char *fmt, ...)
{
    char str[1000];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);

    return render_text(x, y, fontid, color, flags, str);
}

// each line may have embedded newline chars
void sdlx_render_multiline_text(int x, int y, int y_top, int y_bottom, int fontid, 
                                char **lines, unsigned int *colors, int num_lines)
{
    int   y2 = y;
    int   n = 0, k = 0, len;
    char *ptr;
    char  str[1000];
    int   chh = sdlx_char_height(fontid);

    while (n < num_lines) {
        // if y pos of line is below the bottom of the
        // display region then break
        if (y2 > y_bottom - chh) {
            break;
        }

        // extract str from the line currently being processed
        ptr = strchr(&lines[n][k], '\n');
        if (ptr) {
            len = ptr - &lines[n][k];
            memcpy(str, &lines[n][k], len);
            str[len] = '\0';
        } else {
            strcpy(str, &lines[n][k]);
            len = strlen(str);
        }

        // if y location of line (y2) is at or below the begining of the display
        // region then render the line
        if (y2 >= y_top) {
            sdlx_color_t color = (colors ? colors[n] : print_dflt.color);
            render_text(x, y2, fontid, color, 0, str);
        }

        // advance k and n
        k += len;
        if (lines[n][k] == '\n') {
            k++;
        }
        if (lines[n][k] == '\0') {
            k = 0;
            n++;
        }

        // advance y for the next line
        y2 += chh;
    }
}

// -----------------  RENDER RECTANGLES, LINES, CIRCLES, POINTS  --------------------

void sdlx_render_rect(int x, int y, int w, int h, int line_width, sdlx_color_t color)
{
    SDL_FRect rect;
    int i;

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

    set_render_draw_color(color);

    for (i = 0; i < line_width; i++) {
        SDL_RenderRect(renderer, &rect);
        if (rect.w < 2 || rect.h < 2) {
            break;
        }
        rect.x += 1;
        rect.y += 1;
        rect.w -= 2;
        rect.h -= 2;
    }
}

void sdlx_render_fill_rect(int x, int y, int w, int h, sdlx_color_t color)
{
    SDL_FRect rect;

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

    set_render_draw_color(color);
    SDL_RenderFillRect(renderer, &rect);
}

void sdlx_render_line(int x1, int y1, int x2, int y2, sdlx_color_t color)
{
    sdlx_point_t points[2] = { {x1,y1}, {x2,y2} };
    sdlx_render_lines(points, 2, color);
}

void sdlx_render_lines(sdlx_point_t *points, int count, sdlx_color_t color)
{
    SDL_FPoint stack_scaled_points[100];
    SDL_FPoint *scaled_points;

    if (count <= 1) {
        return;
    }

    if (count > 100) {
        scaled_points = malloc(count * sizeof(SDL_FPoint));
    } else {
        scaled_points = stack_scaled_points;
    }

    for (int i = 0; i < count; i++) {
        scaled_points[i].x = points[i].x;
        scaled_points[i].y = points[i].y;
    }

    set_render_draw_color(color);
    SDL_RenderLines(renderer, scaled_points, count);

    if (count > 100) {
        free(scaled_points);
    }
}

void sdlx_render_circle(int x_ctr, int y_ctr, int radius, int line_width, sdlx_color_t color)
{
    int count = 0, i, angle, x, y;
    SDL_FPoint points[370];

    static int sin_table[370];
    static int cos_table[370];
    static bool first_call = true;

    // on first call make table of sin and cos indexed by degrees
    if (first_call) {
        for (angle = 0; angle < 362; angle++) {
            sin_table[angle] = sin(angle*(2*M_PI/360)) * (1<<10);
            cos_table[angle] = cos(angle*(2*M_PI/360)) * (1<<10);
        }
        first_call = false;
    }

    // set the color
    set_render_draw_color(color);

    // loop over line_width
    for (i = 0; i < line_width; i++) {
        // draw circle
        for (angle = 0; angle < 362; angle++) {
            x = x_ctr + ((radius * sin_table[angle]) >> 10);
            y = y_ctr + ((radius * cos_table[angle]) >> 10);
            points[count].x = x;
            points[count].y = y;
            count++;
        }
        SDL_RenderLines(renderer, points, count);
        count = 0;

        // reduce radius by 1
        radius--;
        if (radius <= 0) {
            break;
        }
    }
}

// Bresenham’s circle algorithm)
void sdlx_render_fill_circle(int x_ctr, int y_ctr, int radius, sdlx_color_t color)
{
    int x, y, error;

    x     = radius;
    y     = 0;
    error = 1 - radius;

    set_render_draw_color(color);

    #define draw_horizontal_line(y, x1, x2) SDL_RenderLine(renderer, x1, y, x2, y)
    
    while(x >= y) {
        draw_horizontal_line(y+y_ctr, -x+x_ctr, x+x_ctr);
        draw_horizontal_line(x+y_ctr, -y+x_ctr, y+x_ctr);
        draw_horizontal_line(-y+y_ctr, -x+x_ctr, x+x_ctr);
        draw_horizontal_line(-x+y_ctr, -y+x_ctr, y+x_ctr);
        y++;
        if (error<0) {
            error += 2 * y + 1;
        } else {
            x--;
            error += 2 * (y - x) + 1;
        }
    }
}

void sdlx_render_point(int x, int y, sdlx_color_t color, int point_size)
{
    sdlx_point_t point = {x,y};

    sdlx_render_points(&point, 1, color, point_size);
}

void sdlx_render_points(sdlx_point_t *points, int count, sdlx_color_t color, int point_size)
{
    static struct point_extend_s {
        int max;
        struct point_extend_offset_s {
            int x;
            int y;
        } offset[280];
    } point_extend[10] = {
    { 1, {
        {0,0}, 
            } },
    { 5, {
        {-1,0}, 
        {0,-1}, {0,0}, {0,1}, 
        {1,0}, 
            } },
    { 21, {
        {-2,-1}, {-2,0}, {-2,1}, 
        {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, 
        {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, 
        {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, 
        {2,-1}, {2,0}, {2,1}, 
            } },
    { 37, {
        {-3,-1}, {-3,0}, {-3,1}, 
        {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, 
        {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, 
        {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, 
        {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, 
        {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, 
        {3,-1}, {3,0}, {3,1}, 
            } },
    { 61, {
        {-4,-1}, {-4,0}, {-4,1}, 
        {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, 
        {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, 
        {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, 
        {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, 
        {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, 
        {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, 
        {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, 
        {4,-1}, {4,0}, {4,1}, 
            } },
    { 89, {
        {-5,-1}, {-5,0}, {-5,1}, 
        {-4,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-4,3}, 
        {-3,-4}, {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, {-3,4}, 
        {-2,-4}, {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, {-2,4}, 
        {-1,-5}, {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, {-1,5}, 
        {0,-5}, {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, 
        {1,-5}, {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, 
        {2,-4}, {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, {2,4}, 
        {3,-4}, {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, 
        {4,-3}, {4,-2}, {4,-1}, {4,0}, {4,1}, {4,2}, {4,3}, 
        {5,-1}, {5,0}, {5,1}, 
            } },
    { 121, {
        {-6,-1}, {-6,0}, {-6,1}, 
        {-5,-3}, {-5,-2}, {-5,-1}, {-5,0}, {-5,1}, {-5,2}, {-5,3}, 
        {-4,-4}, {-4,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-4,3}, {-4,4}, 
        {-3,-5}, {-3,-4}, {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, {-3,4}, {-3,5}, 
        {-2,-5}, {-2,-4}, {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, {-2,4}, {-2,5}, 
        {-1,-6}, {-1,-5}, {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, {-1,5}, {-1,6}, 
        {0,-6}, {0,-5}, {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, 
        {1,-6}, {1,-5}, {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, 
        {2,-5}, {2,-4}, {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, {2,4}, {2,5}, 
        {3,-5}, {3,-4}, {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, {3,5}, 
        {4,-4}, {4,-3}, {4,-2}, {4,-1}, {4,0}, {4,1}, {4,2}, {4,3}, {4,4}, 
        {5,-3}, {5,-2}, {5,-1}, {5,0}, {5,1}, {5,2}, {5,3}, 
        {6,-1}, {6,0}, {6,1}, 
            } },
    { 177, {
        {-7,-2}, {-7,-1}, {-7,0}, {-7,1}, {-7,2}, 
        {-6,-4}, {-6,-3}, {-6,-2}, {-6,-1}, {-6,0}, {-6,1}, {-6,2}, {-6,3}, {-6,4}, 
        {-5,-5}, {-5,-4}, {-5,-3}, {-5,-2}, {-5,-1}, {-5,0}, {-5,1}, {-5,2}, {-5,3}, {-5,4}, {-5,5}, 
        {-4,-6}, {-4,-5}, {-4,-4}, {-4,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-4,3}, {-4,4}, {-4,5}, {-4,6}, 
        {-3,-6}, {-3,-5}, {-3,-4}, {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, {-3,4}, {-3,5}, {-3,6}, 
        {-2,-7}, {-2,-6}, {-2,-5}, {-2,-4}, {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, {-2,4}, {-2,5}, {-2,6}, {-2,7}, 
        {-1,-7}, {-1,-6}, {-1,-5}, {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, {-1,5}, {-1,6}, {-1,7}, 
        {0,-7}, {0,-6}, {0,-5}, {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,7}, 
        {1,-7}, {1,-6}, {1,-5}, {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, {1,7}, 
        {2,-7}, {2,-6}, {2,-5}, {2,-4}, {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, {2,4}, {2,5}, {2,6}, {2,7}, 
        {3,-6}, {3,-5}, {3,-4}, {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, {3,5}, {3,6}, 
        {4,-6}, {4,-5}, {4,-4}, {4,-3}, {4,-2}, {4,-1}, {4,0}, {4,1}, {4,2}, {4,3}, {4,4}, {4,5}, {4,6}, 
        {5,-5}, {5,-4}, {5,-3}, {5,-2}, {5,-1}, {5,0}, {5,1}, {5,2}, {5,3}, {5,4}, {5,5}, 
        {6,-4}, {6,-3}, {6,-2}, {6,-1}, {6,0}, {6,1}, {6,2}, {6,3}, {6,4}, 
        {7,-2}, {7,-1}, {7,0}, {7,1}, {7,2}, 
            } },
    { 221, {
        {-8,-2}, {-8,-1}, {-8,0}, {-8,1}, {-8,2}, 
        {-7,-4}, {-7,-3}, {-7,-2}, {-7,-1}, {-7,0}, {-7,1}, {-7,2}, {-7,3}, {-7,4}, 
        {-6,-5}, {-6,-4}, {-6,-3}, {-6,-2}, {-6,-1}, {-6,0}, {-6,1}, {-6,2}, {-6,3}, {-6,4}, {-6,5}, 
        {-5,-6}, {-5,-5}, {-5,-4}, {-5,-3}, {-5,-2}, {-5,-1}, {-5,0}, {-5,1}, {-5,2}, {-5,3}, {-5,4}, {-5,5}, {-5,6}, 
        {-4,-7}, {-4,-6}, {-4,-5}, {-4,-4}, {-4,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-4,3}, {-4,4}, {-4,5}, {-4,6}, {-4,7}, 
        {-3,-7}, {-3,-6}, {-3,-5}, {-3,-4}, {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, {-3,4}, {-3,5}, {-3,6}, {-3,7}, 
        {-2,-8}, {-2,-7}, {-2,-6}, {-2,-5}, {-2,-4}, {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, {-2,4}, {-2,5}, {-2,6}, {-2,7}, {-2,8}, 
        {-1,-8}, {-1,-7}, {-1,-6}, {-1,-5}, {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, {-1,5}, {-1,6}, {-1,7}, {-1,8}, 
        {0,-8}, {0,-7}, {0,-6}, {0,-5}, {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,7}, {0,8}, 
        {1,-8}, {1,-7}, {1,-6}, {1,-5}, {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, {1,7}, {1,8}, 
        {2,-8}, {2,-7}, {2,-6}, {2,-5}, {2,-4}, {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, {2,4}, {2,5}, {2,6}, {2,7}, {2,8}, 
        {3,-7}, {3,-6}, {3,-5}, {3,-4}, {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, {3,5}, {3,6}, {3,7}, 
        {4,-7}, {4,-6}, {4,-5}, {4,-4}, {4,-3}, {4,-2}, {4,-1}, {4,0}, {4,1}, {4,2}, {4,3}, {4,4}, {4,5}, {4,6}, {4,7}, 
        {5,-6}, {5,-5}, {5,-4}, {5,-3}, {5,-2}, {5,-1}, {5,0}, {5,1}, {5,2}, {5,3}, {5,4}, {5,5}, {5,6}, 
        {6,-5}, {6,-4}, {6,-3}, {6,-2}, {6,-1}, {6,0}, {6,1}, {6,2}, {6,3}, {6,4}, {6,5}, 
        {7,-4}, {7,-3}, {7,-2}, {7,-1}, {7,0}, {7,1}, {7,2}, {7,3}, {7,4}, 
        {8,-2}, {8,-1}, {8,0}, {8,1}, {8,2}, 
            } },
    { 277, {
        {-9,-2}, {-9,-1}, {-9,0}, {-9,1}, {-9,2}, 
        {-8,-4}, {-8,-3}, {-8,-2}, {-8,-1}, {-8,0}, {-8,1}, {-8,2}, {-8,3}, {-8,4}, 
        {-7,-6}, {-7,-5}, {-7,-4}, {-7,-3}, {-7,-2}, {-7,-1}, {-7,0}, {-7,1}, {-7,2}, {-7,3}, {-7,4}, {-7,5}, {-7,6}, 
        {-6,-7}, {-6,-6}, {-6,-5}, {-6,-4}, {-6,-3}, {-6,-2}, {-6,-1}, {-6,0}, {-6,1}, {-6,2}, {-6,3}, {-6,4}, {-6,5}, {-6,6}, {-6,7}, 
        {-5,-7}, {-5,-6}, {-5,-5}, {-5,-4}, {-5,-3}, {-5,-2}, {-5,-1}, {-5,0}, {-5,1}, {-5,2}, {-5,3}, {-5,4}, {-5,5}, {-5,6}, {-5,7}, 
        {-4,-8}, {-4,-7}, {-4,-6}, {-4,-5}, {-4,-4}, {-4,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-4,3}, {-4,4}, {-4,5}, {-4,6}, {-4,7}, {-4,8}, 
        {-3,-8}, {-3,-7}, {-3,-6}, {-3,-5}, {-3,-4}, {-3,-3}, {-3,-2}, {-3,-1}, {-3,0}, {-3,1}, {-3,2}, {-3,3}, {-3,4}, {-3,5}, {-3,6}, {-3,7}, {-3,8}, 
        {-2,-9}, {-2,-8}, {-2,-7}, {-2,-6}, {-2,-5}, {-2,-4}, {-2,-3}, {-2,-2}, {-2,-1}, {-2,0}, {-2,1}, {-2,2}, {-2,3}, {-2,4}, {-2,5}, {-2,6}, {-2,7}, {-2,8}, {-2,9}, 
        {-1,-9}, {-1,-8}, {-1,-7}, {-1,-6}, {-1,-5}, {-1,-4}, {-1,-3}, {-1,-2}, {-1,-1}, {-1,0}, {-1,1}, {-1,2}, {-1,3}, {-1,4}, {-1,5}, {-1,6}, {-1,7}, {-1,8}, {-1,9}, 
        {0,-9}, {0,-8}, {0,-7}, {0,-6}, {0,-5}, {0,-4}, {0,-3}, {0,-2}, {0,-1}, {0,0}, {0,1}, {0,2}, {0,3}, {0,4}, {0,5}, {0,6}, {0,7}, {0,8}, {0,9}, 
        {1,-9}, {1,-8}, {1,-7}, {1,-6}, {1,-5}, {1,-4}, {1,-3}, {1,-2}, {1,-1}, {1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, {1,7}, {1,8}, {1,9}, 
        {2,-9}, {2,-8}, {2,-7}, {2,-6}, {2,-5}, {2,-4}, {2,-3}, {2,-2}, {2,-1}, {2,0}, {2,1}, {2,2}, {2,3}, {2,4}, {2,5}, {2,6}, {2,7}, {2,8}, {2,9}, 
        {3,-8}, {3,-7}, {3,-6}, {3,-5}, {3,-4}, {3,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {3,3}, {3,4}, {3,5}, {3,6}, {3,7}, {3,8}, 
        {4,-8}, {4,-7}, {4,-6}, {4,-5}, {4,-4}, {4,-3}, {4,-2}, {4,-1}, {4,0}, {4,1}, {4,2}, {4,3}, {4,4}, {4,5}, {4,6}, {4,7}, {4,8}, 
        {5,-7}, {5,-6}, {5,-5}, {5,-4}, {5,-3}, {5,-2}, {5,-1}, {5,0}, {5,1}, {5,2}, {5,3}, {5,4}, {5,5}, {5,6}, {5,7}, 
        {6,-7}, {6,-6}, {6,-5}, {6,-4}, {6,-3}, {6,-2}, {6,-1}, {6,0}, {6,1}, {6,2}, {6,3}, {6,4}, {6,5}, {6,6}, {6,7}, 
        {7,-6}, {7,-5}, {7,-4}, {7,-3}, {7,-2}, {7,-1}, {7,0}, {7,1}, {7,2}, {7,3}, {7,4}, {7,5}, {7,6}, 
        {8,-4}, {8,-3}, {8,-2}, {8,-1}, {8,0}, {8,1}, {8,2}, {8,3}, {8,4}, 
        {9,-2}, {9,-1}, {9,0}, {9,1}, {9,2}, 
            } },
                };

    static_assert(sizeof(point_extend)/sizeof(point_extend[0]) == MAX_POINT_SIZE+1, "static assert failed");

    if (count <= 0) {
        return;
    }
    if (point_size < 0) {
        point_size = 0;
    }
    if (point_size > MAX_POINT_SIZE) {
        point_size = MAX_POINT_SIZE;
    }

    #define MAX_SDL_POINTS 1000
    int i, j, x, y;
    SDL_FPoint sdlx_points[MAX_SDL_POINTS];
    int sdlx_points_count = 0;
    struct point_extend_s * pe = &point_extend[point_size];
    struct point_extend_offset_s * peo = pe->offset;

    set_render_draw_color(color);

    for (i = 0; i < count; i++) {
        for (j = 0; j < pe->max; j++) {
            x = nearbyint(points[i].x + peo[j].x);
            y = nearbyint(points[i].y + peo[j].y);
            sdlx_points[sdlx_points_count].x = x;
            sdlx_points[sdlx_points_count].y = y;
            sdlx_points_count++;

            if (sdlx_points_count == MAX_SDL_POINTS) {
                SDL_RenderPoints(renderer, sdlx_points, sdlx_points_count);
                sdlx_points_count = 0;
            }
        }
    }

    if (sdlx_points_count > 0) {
        SDL_RenderPoints(renderer, sdlx_points, sdlx_points_count);
        sdlx_points_count = 0;
    }
}

// -----------------  RENDER USING TEXTURES  ---------------------------- 

// - - - - - - create/destroy/query/clear/color_mod texture  - - - - - - - 

sdlx_texture_t *sdlx_create_texture(int width, int height)
{
    sdlx_texture_t *texture;

    // create texture
    texture = (sdlx_texture_t*)
              SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ABGR8888,
                                SDL_TEXTUREACCESS_TARGET,  // SDL_TEXTUREACCESS_STREAMING,
                                width, height);
    if (texture == NULL) {
        ERROR("failed to allocate texture\n");
        return NULL;
    }

    // init texture pixels to transparent black (alpha=0)
    sdlx_clear_texture(texture, 0);

    // return the created texture
    return texture;
}

void sdlx_destroy_texture(sdlx_texture_t *texture)
{
    if (texture == NULL) {
        return;
    }

    SDL_DestroyTexture((SDL_Texture *)texture);
}

void sdlx_query_texture(sdlx_texture_t *texture, int * width, int * height)
{
    float w_float, h_float;

    if (texture == NULL) {
        *width = 0;
        *height = 0;
        return;
    }

    SDL_GetTextureSize((SDL_Texture *)texture, &w_float, &h_float);
    *width  = nearbyint(w_float);
    *height = nearbyint(h_float);
}

void sdlx_clear_texture(sdlx_texture_t *t_arg, sdlx_color_t color)
{
    SDL_Texture *initial_render_target = SDL_GetRenderTarget(renderer);
    SDL_Texture *t = (SDL_Texture *)t_arg;

    if (t != initial_render_target) {
        SDL_SetRenderTarget(renderer, t);
        set_render_draw_color(color);
        SDL_RenderClear(renderer);
        SDL_SetRenderTarget(renderer, initial_render_target);
    } else {
        set_render_draw_color(color);
        SDL_RenderClear(renderer);
    }
}

void sdlx_color_mod_texture(sdlx_texture_t *t, float r, float g, float b)
{
    bool succ;

    if (r > 1) r = 1;
    if (g > 1) g = 1;
    if (b > 1) b = 1;

    succ = SDL_SetTextureColorModFloat((SDL_Texture*)t, r, g, b);
    if (!succ) {
        ERROR("failed color mod, %s\n", SDL_GetError());
    }
}

// - - - - - - set/get pixels from texture - - - - - - - - 

void sdlx_set_texture_pixels(sdlx_texture_t *texture, unsigned int *pixels)
{
    int w, h;

    sdlx_query_texture(texture, &w, &h);

    SDL_UpdateTexture((SDL_Texture*)texture, NULL, pixels, w * BYTES_PER_PIXEL);
}

unsigned int *sdlx_get_texture_pixels(sdlx_texture_t *t, int *w_arg, int *h_arg)
{
    SDL_Texture *initial_render_target;
    bool succ;
    SDL_Surface *surface;
    unsigned int *pixels, *p;
    int w, h, row;

    // save the initial_render_target, so it can be restored at then end 
    // of this routine 
    initial_render_target = SDL_GetRenderTarget(renderer);

    // set render target to the caller supplied texture
    succ = SDL_SetRenderTarget(renderer, (SDL_Texture*)t);
    if (!succ) {
        ERROR("SDL_SetRenderTarget failed, %s\n", SDL_GetError());
        return NULL;
    }

    // create surface containing the texture pixels
    surface = SDL_RenderReadPixels(renderer, NULL);
    if (surface == NULL) {
        ERROR("SDL_RenderReadPixels failed, %s\n", SDL_GetError());
        SDL_SetRenderTarget(renderer, initial_render_target);
        return NULL;
    }

    // get the width/height, and
    // allocate memory to hold the pixels
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
    p = pixels = malloc(w * h * BYTES_PER_PIXEL);

    // copy the pixels from the surface to the allocated buffer
    for (row = 0; row < h; row++) {
        memcpy(p, 
               surface->pixels + (row * surface->pitch), 
               w * BYTES_PER_PIXEL);
        p += w;
    }

    // destroy the surface
    SDL_DestroySurface(surface);

    // restore render target
    SDL_SetRenderTarget(renderer, initial_render_target);

    // return w, h, and pixels; caller must free pixels
    if (w_arg) *w_arg = w;
    if (h_arg) *h_arg = h;
    return pixels;
}

// - - - - - - render texture - - - - - 

void sdlx_render_texture(sdlx_texture_t *texture, sdlx_loc_t *srcrect, sdlx_loc_t *dstrect)
{
    SDL_FRect src, *Src=NULL;
    SDL_FRect dst, *Dst=NULL;

    if (texture == NULL) {
        return;
    }
    
    if (srcrect != NULL) {
        src.x = srcrect->x;
        src.y = srcrect->y;
        src.w = srcrect->w;
        src.h = srcrect->h;
        Src = &src;
    }

    if (dstrect != NULL) {
        dst.x = dstrect->x;
        dst.y = dstrect->y;
        dst.w = dstrect->w;
        dst.h = dstrect->h;
        Dst = &dst;
    }

    SDL_RenderTexture(renderer, (SDL_Texture*)texture, Src, Dst);
}

void sdlx_render_texture_rotated(sdlx_texture_t *texture, sdlx_loc_t *srcrect, sdlx_loc_t *dstrect,
                                 double angle, sdlx_point_t *center, int flip) 
{
    SDL_FRect src, *Src=NULL;
    SDL_FRect dst, *Dst=NULL;
    SDL_FPoint ctr, *Ctr=NULL;

    if (texture == NULL) {
        return;
    }
    
    if (srcrect != NULL) {
        src.x = srcrect->x;
        src.y = srcrect->y;
        src.w = srcrect->w;
        src.h = srcrect->h;
        Src = &src;
    }

    if (dstrect != NULL) {
        dst.x = dstrect->x;
        dst.y = dstrect->y;
        dst.w = dstrect->w;
        dst.h = dstrect->h;
        Dst = &dst;
    }

    if (center != NULL) {
        ctr.x = center->x;
        ctr.y = center->y;
        Ctr = &ctr;
    }

    SDL_RenderTextureRotated(renderer, (SDL_Texture*)texture, Src, Dst, angle, Ctr, flip);
}

// - - - - - - set render target - - - - - - - - 

void sdlx_set_render_target(sdlx_texture_t *t)
{
    bool succ;

    if (t == NULL) {
        succ = SDL_SetRenderTarget(renderer, (SDL_Texture*)texture_display);
    } else {
        succ = SDL_SetRenderTarget(renderer, (SDL_Texture*)t);
    }

    if (!succ) {
        ERROR("SDL_SetRenderTarget failed, %s\n", SDL_GetError());
    }
}

// -----------------  MISC  --------------------------------------------- 

void sdlx_show_toast(char *message)
{ 
    INFO("%s\n", message);

#ifdef ANDROID
    https://developer.android.com/reference/android/view/Gravity

    #define DURATION_SHORT  0
    #define DURATION_LONG   1
    #define GRAVITY_CENTER  0x11
    #define GRAVITY_BOTTOM  0x50

    // this is safe to call from any thread
    SDL_ShowAndroidToast(message, DURATION_LONG, GRAVITY_BOTTOM, 0, 0);
#endif
}
