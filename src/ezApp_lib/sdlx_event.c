#include <std_hdrs.h>
    
#include <sdlx.h>
#include <utils.h>
#include <private.h>

#include <SDL3/SDL.h>

//
// defines
//

#define MAX_EVENT 200
#define ONE_MS    1000

//
// typedefs
//

typedef struct {
    sdlx_loc_t loc;
    int       event_id;
} event_t;

//
// variables
//

//xxx delete these here; do these need to be public
//extern SDL_Window *window;
//extern SDL_Renderer *renderer;


static event_t event_tbl[MAX_EVENT];
static int     max_event;
static bool    evid_motion_registered;
static bool    evid_keybd_registered;
static int     event_quit_rcvd;
static bool    event_box_enable;

//
// prototypes
//

static void process_sdlx_event(SDL_Event *ev, sdlx_event_t *event);
static char *event_type_to_str(enum SDL_EventType evtype) ATTRIBUTE_UNUSED;

// -----------------  REGISTER EVENTS  --------------------

void sdlx_register_event(sdlx_loc_t *loc, int event_id)
{
    sdlx_loc_t loc2;

    // if event_tbl is full print error msg and return
    if (max_event == MAX_EVENT) {
        ERROR("event_tbl is full\n");
        return;
    }

    // set registration flags when mouse motion, or keyboard events are registered
    if (event_id == EVID_MOTION) {
        evid_motion_registered = true;
        return;
    }
    if (event_id == EVID_KEYBD) {
        evid_keybd_registered = true;
        return;
    }

    // sanity check loc arg
    if (loc == NULL || loc->w == 0 || loc->h == 0) {
        ERROR("invalid loc, event_id=%d\n", event_id);
        return;
    }

    // enforce minimum w,h
    loc2 = *loc;
    if (loc2.w < 150) {
        int delta = 150 - loc2.w;
        loc2.w += delta;
        loc2.x -= delta/2;
    }
    if (loc2.h < 150) {
        int delta = 150 - loc2.h;
        loc2.h += delta;
        loc2.y -= delta/2;
    }

    // event box aids development;
    // when enabled a rectangle is drawn around the event display area
    if (event_box_enable) {
        sdlx_render_rect(loc2.x, loc2.y, loc2.w, loc2.h, 3, COLOR_WHITE);
    }

    // if the display orientation is landscape then the caller supplied 
    // landscape event location is converted to a portrait location;
    // the portrait location is what is stored the event_tbl
    if (orientation == LANDSCAPE) { 
        int x,y,w,h;
        x = logical_win_width_portrait - loc2.y - loc2.h;
        y = loc2.x;
        w = loc2.h;
        h = loc2.w;

        loc2.x = x;
        loc2.y = y;
        loc2.w = w;
        loc2.h = h;
    }

    // save the event location in the event_tbl
    event_tbl[max_event].loc = loc2;
    event_tbl[max_event].event_id  = event_id; 
    max_event++;
}

void sdlx_register_control_events(int evid1, char *evstr1, 
                                  int evid2, char *evstr2, 
                                  int evid3, char *evstr3)
{
    sdlx_loc_t *loc;
    int i, x, y;
    char *evstr[3];
    int  evid[3];

    #define FG_COLOR   COLOR_WHITE
    #define BG_COLOR   COLOR_TEAL

    evstr[0] = evstr1;
    evstr[1] = evstr2;
    evstr[2] = evstr3;

    evid[0] = evid1;
    evid[1] = evid2;
    evid[2] = evid3;

    // fill entire control events area with background color
    if (orientation == PORTRAIT) {
        y = logical_win_height - CONTROL_AREA_SIZE;
        sdlx_render_fill_rect(0, y, logical_win_width, logical_win_height-y, BG_COLOR);
    } else {
        sdlx_render_fill_rect(logical_win_width - CONTROL_AREA_SIZE, 0,  // x,y
                              CONTROL_AREA_SIZE, logical_win_height,      // w,h
                              BG_COLOR);
    }

    // display the 3 control events at the display bottom
    for (i = 0; i < 3; i++) {
        int chw = sdlx_char_width(FONT_NORMAL);

        if (evstr[i] == NULL) {
            continue;
        }

        if (orientation == PORTRAIT) {
            x = (logical_win_width/3/2) + i * (logical_win_width/3);
            if (i == 0 && x < strlen(evstr[0]) * chw / 2) {
                x = strlen(evstr[0]) * chw / 2;
            }
            if (i == 2 && x > logical_win_width - (strlen(evstr[2]) * chw / 2)) {
                x = logical_win_width - (strlen(evstr[2]) * chw / 2);
            }
            y = logical_win_height - (CONTROL_AREA_SIZE / 2);
            loc = sdlx_render_printf_ex2(x, y, FONT_NORMAL, FG_COLOR, FLAG_XY_CTR, "%s", evstr[i]);
        } else {
            y = (logical_win_height/3/2) + i * (logical_win_height/3);
            if (i == 0 && y < strlen(evstr[0]) * chw / 2) {
                y = strlen(evstr[0]) * chw / 2;
            }
            if (i == 2 && y > logical_win_height - (strlen(evstr[2]) * chw / 2)) {
                y = logical_win_height - (strlen(evstr[2]) * chw / 2);
            }
            x = logical_win_width - (CONTROL_AREA_SIZE / 2);
            loc = sdlx_render_printf_ex2(
                        x, logical_win_height - y,
                        FONT_NORMAL, FG_COLOR, FLAG_XY_CTR|FLAG_ROT_CTR_270, "%s", evstr[i]);
        }

        sdlx_register_event(loc, evid[i]);
    }
}

void sdlx_reset_events(void)
{
    max_event = 0;
    evid_motion_registered = false;
    evid_keybd_registered = false;
}

void sdlx_event_box_ctrl(bool enable)
{
    event_box_enable = enable;
}

// -----------------  GET AN EVENT  -----------------------

// arg timeout_us:
//   -1:     wait forever
//    0:     don't wait
//    usecs: timeout
void sdlx_get_event(long timeout_us, sdlx_event_t *event)
{
    SDL_Event ev;
    long waited = 0;
    bool got_event;

    // if SDL_QUIT has been received then
    // repeat returning EVID_QUIT
    if (event_quit_rcvd > 0) {
        event_quit_rcvd--;
        memset(event, 0, sizeof(*event));
        event->event_id = EVID_QUIT;
        return;
    }

try_again:
    // preset return sdlx event 
    memset(event, 0, sizeof(*event));
    event->event_id = -1;

    // get SDL event
    got_event = SDL_PollEvent(&ev);

    // no event available, either return error or try again to get event
    if (!got_event) {
        if (timeout_us == 0) {
            // dont wait
            return;
        } else if (timeout_us < 0 || waited < timeout_us) {
            // either wait forever or time waited is less than timeout_us
            usleep(10*ONE_MS);
            waited += 10*ONE_MS;
            goto try_again;
        } else {
            // time waited exceeds timeout_us
            return;
        }
    }

    // process the sdlx_event; this may or may not return an event
    process_sdlx_event(&ev, event);
    if (event->event_id == -1) {
        goto try_again;
    }

    // an event was returned from process_sdlx_event
    return;
}

static void process_sdlx_event(SDL_Event *ev, sdlx_event_t *event)
{
    #define AT_LOC(X,Y,loc) (((X) >= (loc).x)            && \
                             ((X) <  (loc).x + (loc).w)  && \
                             ((Y) >= (loc).y)            && \
                             ((Y) <  (loc).y + (loc).h))

    int i;

    switch (ev->type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        static int last_pressed_x = -1;
        static int last_pressed_y = -1;
        int x, y;

        //INFO("MOUSE_BUTTON button=%s state=%s x=%d y=%d\n",
        //        (ev->button.button == SDL_BUTTON_LEFT   ? "LEFT" :
        //         ev->button.button == SDL_BUTTON_MIDDLE ? "MIDDLE" :
        //         ev->button.button == SDL_BUTTON_RIGHT  ? "RIGHT" : "???"),
        //        (ev->button.down ? "DOWN" : "UP"),
        //        ev->button.x,
        //        ev->button.y);
        x = ev->button.x / scale_events_x;
        y = ev->button.y / scale_events_y;

        if (ev->button.down) {
            last_pressed_x = x;
            last_pressed_y = y;
        } else {
            for (i = max_event-1; i >= 0; i--) {
                if (AT_LOC(x, y, event_tbl[i].loc)) {
                    break;
                }
            }
            if (i >= 0 && AT_LOC(last_pressed_x, last_pressed_y, event_tbl[i].loc)) {
                event->event_id = event_tbl[i].event_id;
            }
        }
        break; }

    case SDL_EVENT_MOUSE_MOTION: {
        if ((ev->motion.state & SDL_BUTTON_LMASK) && evid_motion_registered) {
            //INFO("MOUSE_MOTION x=%f y=%f xrel=%f yrel=%f\n",
            //    ev->motion.x,
            //    ev->motion.y,
            //    ev->motion.xrel,
            //    ev->motion.yrel);

            // consolidate possible additional MOUSE_MOTION events into this event
            while (true) {
                SDL_Event tmp_ev;
                int rc = SDL_PeepEvents(
                            &tmp_ev, 1, SDL_GETEVENT,
                            SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_MOTION);
                if (rc != 1) break;
                if ((tmp_ev.motion.state & SDL_BUTTON_LMASK) == 0) break;

                ev->motion.x    += tmp_ev.motion.x;
                ev->motion.y    += tmp_ev.motion.y;
                ev->motion.xrel += tmp_ev.motion.xrel;
                ev->motion.yrel += tmp_ev.motion.yrel;
            }

            event->event_id = EVID_MOTION;
            if (orientation == PORTRAIT) {
                event->u.motion.x = ev->motion.x / scale_events_x;
                event->u.motion.y = ev->motion.y / scale_events_y;
                event->u.motion.xrel = ev->motion.xrel / scale_events_x;
                event->u.motion.yrel = ev->motion.yrel / scale_events_y;
            } else {
                event->u.motion.y = logical_win_height - ev->motion.x / scale_events_x;
                event->u.motion.x = ev->motion.y / scale_events_y;
                event->u.motion.yrel = -ev->motion.xrel / scale_events_x;
                event->u.motion.xrel = ev->motion.yrel / scale_events_y;
            }
        }
        break; }

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        SDL_KeyboardEvent *x = &ev->key;
        SDL_Keycode keycode;

        if (!evid_keybd_registered || x->down) {
            break;
        }

        keycode = SDL_GetKeyFromScancode(x->scancode, x->mod, false);
        //bool shift = (x->mod & SDL_KMOD_SHIFT) != 0;
        //INFO("GOT keycode 0x%x  shift=%d\n", keycode, shift);
        event->event_id = EVID_KEYBD;
        event->u.data.bytes[0] = keycode;
        break; }

    case SDL_EVENT_QUIT: {
        // the event_quit_rcvd variable is set so that 
        // this routine will repeat returning EVID_QUIT, so that
        // a running app will first process the EVID_QUIT, and 
        // finally main will process EVID_QUIT
        event_quit_rcvd = 10;
        event->event_id = EVID_QUIT;
        break; }

    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION:
        // SDL_TouchFingerEvent
        break;

    case SDL_EVENT_PINCH_BEGIN:
    case SDL_EVENT_PINCH_UPDATE:
    case SDL_EVENT_PINCH_END: {
        SDL_PinchFingerEvent *x = &ev->pinch;

        INFO("%s: scale=%f span=%f %f focus=%f %f\n",
             (ev->type == SDL_EVENT_PINCH_BEGIN  ? "PINCH_BEGIN" :
              ev->type == SDL_EVENT_PINCH_UPDATE ? "PINCH_BEGIN" :
                                                   "PINCH_END"),
            x->scale, x->span_x, x->span_y, x->focus_x, x->focus_y);
        break; }

    case SDL_EVENT_SENSOR_UPDATE:
        // these occur frequently
        break;

#if 0  //xxx
    case SDL_EVENT_RENDER_DEVICE_RESET:
        printf("XXXXXXXXXXXXXXXXXX GOT SDL_EVENT_RENDER_DEVICE_RESET\n");
        break;

    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        printf("XXXXXXXXXXXXXXXXXX GOT SDL_EVENT_WINDOW_FOCUS_GAINED\n");
        break;
#endif

    default: {
        // debug print the events that are not supported
        //INFO("unsupported sdl event %s\n", event_type_to_str(ev->type));
        break; }
    }
}

static char *event_type_to_str(enum SDL_EventType evtype)
{
    #define CASE(et) case et: return #et
    static char str[20];

    switch (evtype) {
    CASE(SDL_EVENT_WILL_ENTER_BACKGROUND);    // these must be handled in a callback
    CASE(SDL_EVENT_DID_ENTER_BACKGROUND);     // set with SDL_AddEventWatch
    CASE(SDL_EVENT_WILL_ENTER_FOREGROUND);
    CASE(SDL_EVENT_DID_ENTER_FOREGROUND);

    CASE(SDL_EVENT_WINDOW_SHOWN);
    CASE(SDL_EVENT_WINDOW_HIDDEN);
    CASE(SDL_EVENT_WINDOW_EXPOSED);
    CASE(SDL_EVENT_WINDOW_MOVED);
    CASE(SDL_EVENT_WINDOW_RESIZED);
    CASE(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
    CASE(SDL_EVENT_WINDOW_METAL_VIEW_RESIZED);
    CASE(SDL_EVENT_WINDOW_MINIMIZED);
    CASE(SDL_EVENT_WINDOW_MAXIMIZED);
    CASE(SDL_EVENT_WINDOW_RESTORED);
    CASE(SDL_EVENT_WINDOW_MOUSE_ENTER);
    CASE(SDL_EVENT_WINDOW_MOUSE_LEAVE);
    CASE(SDL_EVENT_WINDOW_FOCUS_GAINED);
    CASE(SDL_EVENT_WINDOW_FOCUS_LOST);
    CASE(SDL_EVENT_WINDOW_CLOSE_REQUESTED);
    CASE(SDL_EVENT_WINDOW_HIT_TEST);
    CASE(SDL_EVENT_WINDOW_ICCPROF_CHANGED);
    CASE(SDL_EVENT_WINDOW_DISPLAY_CHANGED);
    CASE(SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED);
    CASE(SDL_EVENT_WINDOW_SAFE_AREA_CHANGED);
    CASE(SDL_EVENT_WINDOW_OCCLUDED);
    CASE(SDL_EVENT_WINDOW_ENTER_FULLSCREEN);
    CASE(SDL_EVENT_WINDOW_LEAVE_FULLSCREEN);
    CASE(SDL_EVENT_WINDOW_DESTROYED);
    CASE(SDL_EVENT_WINDOW_HDR_STATE_CHANGED);

    CASE(SDL_EVENT_MOUSE_MOTION);
    CASE(SDL_EVENT_MOUSE_BUTTON_DOWN);
    CASE(SDL_EVENT_MOUSE_BUTTON_UP);
    CASE(SDL_EVENT_MOUSE_WHEEL);
    CASE(SDL_EVENT_MOUSE_ADDED);
    CASE(SDL_EVENT_MOUSE_REMOVED);

    CASE(SDL_EVENT_FINGER_DOWN);
    CASE(SDL_EVENT_FINGER_UP);
    CASE(SDL_EVENT_FINGER_MOTION);

    CASE(SDL_EVENT_KEY_DOWN);
    CASE(SDL_EVENT_KEY_UP);
    CASE(SDL_EVENT_TEXT_EDITING);
    CASE(SDL_EVENT_TEXT_INPUT);
    CASE(SDL_EVENT_KEYMAP_CHANGED);
    CASE(SDL_EVENT_KEYBOARD_ADDED);
    CASE(SDL_EVENT_KEYBOARD_REMOVED);
    CASE(SDL_EVENT_TEXT_EDITING_CANDIDATES);
    CASE(SDL_EVENT_SCREEN_KEYBOARD_SHOWN);
    CASE(SDL_EVENT_SCREEN_KEYBOARD_HIDDEN);

    CASE(SDL_EVENT_AUDIO_DEVICE_ADDED);
    CASE(SDL_EVENT_AUDIO_DEVICE_REMOVED);
    CASE(SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED);

    CASE(SDL_EVENT_CLIPBOARD_UPDATE);

    CASE(SDL_EVENT_SENSOR_UPDATE);

    CASE(SDL_EVENT_QUIT);

    default:
        sprintf(str, "0x%04x", evtype);
        return str;
    }
}

