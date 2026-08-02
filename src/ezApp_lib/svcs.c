#include <std_hdrs.h>

#include <sdlx.h>
#include <utils.h>
#include <svcs.h>
#include <private.h>

//
// defines
//

#define MAX_SVCS     20
#define MAX_SVC_NAME 30

// --- the following defines relate to svc state ---

#define SVC_STOP_TIMEOUT_SECS  5

#define SERVICE_STATE_DELETED           0     // blue  (should not be seen)
#define SERVICE_STATE_STOPPED           1     // white
#define SERVICE_STATE_RUNNING           2     // green
#define SERVICE_STATE_STOPPING          3     // yellow
#define SERVICE_STATE_DELETING          4     // yellow
#define SERVICE_STATE_STOPPED_BY_ERROR  5     // red

#define SERVICE_STATE_STR(state) \
    ((state) == SERVICE_STATE_DELETED           ? "DELETED"          : \
     (state) == SERVICE_STATE_STOPPED           ? "STOPPED"          : \
     (state) == SERVICE_STATE_RUNNING           ? "RUNNING"          : \
     (state) == SERVICE_STATE_STOPPING          ? "STOPPING"         : \
     (state) == SERVICE_STATE_DELETING          ? "DELETING"         : \
     (state) == SERVICE_STATE_STOPPED_BY_ERROR  ? "STOPPED_BY_ERROR" : \
                                                  "????")

#define SERVICE_STATE_TO_COLOR(state) \
    ((state) == SERVICE_STATE_DELETED           ? COLOR_BLUE   : \
     (state) == SERVICE_STATE_STOPPED           ? COLOR_WHITE  : \
     (state) == SERVICE_STATE_RUNNING           ? COLOR_GREEN  : \
     (state) == SERVICE_STATE_STOPPING          ? COLOR_YELLOW : \
     (state) == SERVICE_STATE_DELETING          ? COLOR_YELLOW : \
                                                  COLOR_RED)

#define SERVICE_IS_STOPPED(state)  ((state) == SERVICE_STATE_STOPPED || \
                                    (state) == SERVICE_STATE_STOPPED_BY_ERROR)

// --- the following defines relate to makeing requests to a svc ---

#define REQ_STATE_IDLE         0
#define REQ_STATE_PENDING      1
#define REQ_STATE_IN_PROGRESS  2
#define REQ_STATE_COMPLETED    3

//
// typedefs
//

typedef struct {
    char            name[MAX_SVC_NAME];
    int             svc_state;
    int             req_state;
    svc_req_t       req;
    pthread_mutex_t req_mutex1;
    pthread_mutex_t req_mutex2;
    pthread_cond_t  req_cond;
} svc_t;

//
// variables
//

static svc_t  svcs[MAX_SVCS];
static int    max_svcs;

static bool   initialized;

//
// prototypes
//

static void update_svcs_tbl(void);
static int svc_start(char *name);  
static int svc_stop(char *name);  
static void run_svc(char *svc_name);
static int svc_thread(void *cx);

static int svc_name_to_id(char *svc_name);
static void remove_trailing_newline(char *s);
static int num_svcs(void);

// -----------------  SVCS ROUTINES USED BY MAIN.C  ---------------

// start all services, except if tagged stopped;
// a svc is tagged stopped if it has a 'stopped' file in its dir
void svcs_start_all(void)
{
    INFO("starting all services\n");

    // perform initialization on first call
    if (!initialized) {
        // init pthread mutexs, and cond
        for (int id = 0; id < MAX_SVCS; id++) {
            svc_t *x = svcs+id;
            pthread_mutex_init(&x->req_mutex1, NULL);
            pthread_mutex_init(&x->req_mutex2, NULL);
            pthread_cond_init(&x->req_cond, NULL);
        }

        // find all svc names, these are the svcs/* directories, and
        // populates the svcs tbl based on the names found
        update_svcs_tbl();

        // set initialized flag
        initialized = true;
    }


    // start all svcs that are not tagged with 'stopped' file 
    // and are in SERVICE_STATE_STOPPED
    for (int id = 0; id < max_svcs; id++) {
        svc_t *x = &svcs[id];
        char dir[100];
        bool stopped;
        char tmp_name[MAX_SVC_NAME];

        // determine if svc is marked 'stopped'
        // note: use of tmp_name is to workaround compiler warning about sprintf truncation
        strcpy(tmp_name, x->name);
        snprintf(dir, sizeof(dir), "svcs/%s", tmp_name);
        stopped = util_file_exists(dir, "stopped");

        // if svc is not marked 'stopped', and its state is SERVICE_STATE_STOPPED, then run the svc
        if (!stopped && SERVICE_IS_STOPPED(x->svc_state)) {
            svc_start(x->name);
        }
    }
}

void svcs_stop_all(void)
{
    INFO("stopping all services\n");

    for (int id = 0; id < max_svcs; id++) {
        svc_t *x = &svcs[id];
        if (x->svc_state == SERVICE_STATE_RUNNING) {
            svc_req_t stop_req = {SVC_REQ_ID_STOP};
            x->svc_state = SERVICE_STATE_STOPPING;
            svc_make_req(x->name, &stop_req, 5);
        }
    }
}

void svcs_display(int bg_color)
{
    sdlx_event_t event;
    int         id, k;
    bool        done = false;
    sdlx_loc_t  *loc;
    int         y_top, y_bottom, y2;
    double      y;

    #define EVID_SVC_START    100
    #define EVID_SVC_STOP     200

    sdlx_print_set_default(FONT_NORMAL, COLOR_WHITE);

    // init variables which define the vertical region of the display
    // being used for the filename list
    y_top    = ROW2Y(3);
    y_bottom = sdlx_win_height;
    y        = y_top;

    // handle the setting display
    while (true) {
        // find all svc names, these are the svcs/* directories, and populates
        // the svcs tbl based on the names found
        update_svcs_tbl();

        // init display and display title line
        sdlx_display_init(bg_color, PORTRAIT);
        sdlx_render_printf_ex2(sdlx_win_width/2, ROW2Y(1), 
                               FONT_NORMAL, COLOR_WHITE, FLAG_X_CTR, 
                               "Services");

        // display name and controls for each service
        for (k=0, id=0; id < max_svcs; id++) {
            svc_t *x = &svcs[id];

            // if svc has been deleted then continue so that it is not displayed
            if (x->svc_state == SERVICE_STATE_DELETED) {
                continue;
            }

            // disable scroling if there are 12 or fewer services
            if (num_svcs() <= 12) {
                y = y_top;
            }

            // if y display location is above the top of the display region,
            // or below the bottom of the display region, then either continue or break
            y2 = y + 2 * k++ * sdlx_char_height_dflt;
            if (y2 < y_top - sdlx_char_height_dflt/2) continue;
            if (y2 > y_bottom) break;

            // display svc name
            sdlx_render_printf_ex1(0, y2, FONT_NORMAL, SERVICE_STATE_TO_COLOR(x->svc_state), "%-s", x->name);

            // display state info, or control for this svc
            if (x->svc_state == SERVICE_STATE_DELETING) {
                sdlx_render_printf_ex1(COL2X(10), y2, FONT_NORMAL, COLOR_WHITE, "deleting");
            } else if (x->svc_state == SERVICE_STATE_STOPPING) {
                sdlx_render_printf_ex1(COL2X(10), y2, FONT_NORMAL, COLOR_WHITE, "stopping");
            } else if (SERVICE_IS_STOPPED(x->svc_state)) {
                loc = sdlx_render_printf_ex1(COL2X(10), y2, FONT_NORMAL, COLOR_LIGHT_BLUE, "start");
                sdlx_register_event(loc, EVID_SVC_START+id);
            } else if (x->svc_state == SERVICE_STATE_RUNNING) {
                loc = sdlx_render_printf_ex1(COL2X(10), y2, FONT_NORMAL, COLOR_LIGHT_BLUE, "stop");
                sdlx_register_event(loc, EVID_SVC_STOP+id);
            }
        }

        // register motion and control events
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(0, NULL, 
                                     0, NULL, 
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for an event, with 100 ms timeout;
        // if no event received then re-display
        sdlx_get_event(100000, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process the event
        switch (event.event_id) {
        case EVID_SVC_START ... EVID_SVC_START + MAX_SVCS - 1:
            id = event.event_id - EVID_SVC_START;
            svc_start(svcs[id].name);
            break;
        case EVID_SVC_STOP ... EVID_SVC_STOP + MAX_SVCS - 1:
            id = event.event_id - EVID_SVC_STOP;
            svc_stop(svcs[id].name);
            break;
        case EVID_MOTION:
            y += event.u.motion.yrel;
            if (y >= y_top) {
                y = y_top;
            }
            break;
        case EVID_QUIT:
            done = true;
            break;
        }

        if (done) {
            break;
        }
    }
}

int num_svcs_running(void)
{
    int num = 0;

    for (int id = 0; id < max_svcs; id++) {
        svc_t *x = svcs+id;
        if (x->svc_state == SERVICE_STATE_RUNNING) {
            num++;
        }
    }

    return num;
}

static void update_svcs_tbl(void)
{
    FILE *fp;
    char s[100];
    char new[MAX_SVCS][MAX_SVC_NAME];
    int  max_new = 0;
    long new_mtime;

    static long mtime;

    // if the svcs dir has not been modifies since last call then return
    new_mtime = util_file_mtime("svcs", NULL);
    if (new_mtime == mtime) {
        return;
    }
    mtime = new_mtime;

    // get list of svcs/* directories, store in 'new' tbl
    fp = popen("cd svcs; find * -maxdepth 0 -type d", "r");
    while (fgets(s, sizeof(s), fp)) {
        remove_trailing_newline(s);
        strcpy(new[max_new++], s);
        if (max_new == MAX_SVCS) {
            break;
        }
    }
    pclose(fp);

    // for all existing svc names that are not in the new svc names, delete the svc
    for (int id = 0; id < max_svcs; id++) {
        svc_t *x = &svcs[id];

        // if this svc is already deleted or deleting then continue
        if (x->svc_state == SERVICE_STATE_DELETED || x->svc_state == SERVICE_STATE_DELETING) {
            continue;
        }

        // determine if svc name is contained in the new names list
        bool found_in_new_names = false;
        for (int i = 0; i < max_new; i++) {
            if (strcmp(x->name, new[i]) == 0) {
                found_in_new_names = true;
                break;
            }
        }

        // if svc name is not contained in the new names list then delete the svc
        if (!found_in_new_names) {
            INFO("deleting svc %s\n", x->name);

            // if svc is running then send it stop request
            if (x->svc_state == SERVICE_STATE_RUNNING) {
                svc_req_t stop_req = {SVC_REQ_ID_STOP};
                x->svc_state = SERVICE_STATE_STOPPING;
                svc_make_req(x->name, &stop_req, 5);
            }

            // wait for service to be in stopped state
            int ms = 0;
            while (ms < (SVC_STOP_TIMEOUT_SECS*1000) && !SERVICE_IS_STOPPED(x->svc_state)) {
                usleep(100000);
                ms += 100;
            }

            // the svc should be stopped now
            //
            // if it is stopped then
            //   remove the svc
            // else
            //   set svc state to SERVICE_STATE_DELETING
            // endif
            if (SERVICE_IS_STOPPED(x->svc_state)) {
                x->name[0] = '\0';
                x->svc_state = SERVICE_STATE_DELETED;
                x->req_state = REQ_STATE_IDLE;
                memset(&x->req, 0, sizeof(x->req));
            } else {
                x->svc_state = SERVICE_STATE_DELETING;
            }
        }
    }

    // for all new svc names that don't currently exist in svcs tbl add entry to svcs tbl
    for (int i = 0; i < max_new; i++) {
        int id;

        // if new name is already in svcs tbl then continue
        if (svc_name_to_id(new[i]) != -1) {
            continue;
        }

        // find first free entry in svcs tbl;
        // if not found then add new entry to the end of svcs tbl
        for (id = 0; id < max_svcs; id++) {
            if (svcs[id].svc_state == SERVICE_STATE_DELETED) {
                break;
            }
        }
        if (id == max_svcs) {
            if (max_svcs == MAX_SVCS) {
                ERROR("svcs tbl is full\n");
                break;
            }
            max_svcs++;
        }

        // init the new svcs tbl entry for this the new svc
        svc_t *x = svcs + id;
        strcpy(x->name, new[i]);
        x->svc_state = SERVICE_STATE_STOPPED;
        x->req_state = REQ_STATE_IDLE;
        memset(&x->req, 0, sizeof(x->req));

        // except on first call, the new svc is tagged 'stopped'
        if (initialized) {
            char dir[100], tmp_name[MAX_SVC_NAME];
            strcpy(tmp_name, x->name);
            sprintf(dir, "svcs/%s", tmp_name);
            util_write_file(dir, "stopped", NULL, 0);
        }
    }

    // debug print new svc_tbl
    INFO("Services List ...\n");
    for (int id = 0; id < max_svcs; id++) {
        svc_t *x = &svcs[id];
        if (x->svc_state != SERVICE_STATE_DELETED) {
            INFO("%15s %s\n", x->name, SERVICE_STATE_STR(x->svc_state));
        }
    }
}

static int svc_start(char *name)
{
    int id;
    char dir[100];

    // lookup id from name
    id = svc_name_to_id(name);
    if (id == -1) {
        ERROR("svc %s not found\n", name);
        return -1;
    }
    svc_t *x = &svcs[id];

    // if the service is not stopped then return error
    if (!SERVICE_IS_STOPPED(x->svc_state)) {
        ERROR("svc %s is not stopped, state=%s\n", name, SERVICE_STATE_STR(x->svc_state));
        return -1;
    }
    
    // delete the 'stopped' file; 
    // when this file exists this svc is not started by svcs_start_all
    sprintf(dir, "svcs/%s", x->name);
    util_delete_file(dir, "stopped");

    // reset svc_t fields
    x->svc_state = SERVICE_STATE_RUNNING;
    x->req_state = REQ_STATE_IDLE;
    memset(&x->req, 0, sizeof(x->req));

    // run the service
    run_svc(x->name);

    // success
    return 0;
}

static int svc_stop(char *name)
{
    int id;
    char dir[100];

    // lookup id from name
    id = svc_name_to_id(name);
    if (id == -1) {
        ERROR("svc %s not found\n", name);
        return -1;
    }
    svc_t *x = &svcs[id];

    // if the service is not running then return error
    if (x->svc_state != SERVICE_STATE_RUNNING) {
        ERROR("svc %s is not running, state=%s\n", name, SERVICE_STATE_STR(x->svc_state));
        return -1;
    }
    
    // create the 'stopped' file; 
    // when this file exists this svc is not started by svcs_start_all
    sprintf(dir, "svcs/%s", x->name);
    util_write_file(dir, "stopped", NULL, 0);

    // send req to the service to request that it stop
    svc_req_t stop_req = {SVC_REQ_ID_STOP};
    x->svc_state = SERVICE_STATE_STOPPING;
    svc_make_req(x->name, &stop_req, 5);

    // success
    return 0;
}

static void run_svc(char *svc_name_arg)
{
    char thread_name[100];
    char *svc_name = strdup(svc_name_arg);

    sprintf(thread_name, "svc_%s", svc_name);
    sdlx_create_detached_thread(svc_thread, thread_name, svc_name);
}

static int svc_thread(void *cx)
{
    #define IS_SVC true

    char *svc_name = cx;
    int   rc, id;

    rc = run(svc_name, IS_SVC);

    id = svc_name_to_id(svc_name);
    if (id != -1) {
        svc_t *x = &svcs[id];
        x->svc_state = (rc == 0 ? SERVICE_STATE_STOPPED : SERVICE_STATE_STOPPED_BY_ERROR);
    }

    free(svc_name);
    return 0;
}

// -----------------  MAKE SVC REQUEST SUPPORT  -------------------

// ---- routines called by apps ----

#define POLL_FOR(cond,mtx,tout_secs) \
    do { \
        long start_us = util_microsec_timer(); \
        while (true) { \
            if (cond) break; \
            if (util_microsec_timer() - start_us > ((tout_secs) * 1000000)) break; \
            if (mtx) pthread_mutex_unlock(mtx); \
            usleep(10000); \
            if (mtx) pthread_mutex_lock(mtx); \
        } \
    } while (0)

// svc_make_req return values:
// - ret >= 0: req comp_status, zero should usually be used to indicate success
// - ret < 0:  one of the SVC_REQ_ERROR codes

int svc_make_req(char *svc_name, svc_req_t *req_arg, int timeout_secs)
{
    int id;
    svc_t *x;
    bool okay;

    // if not initialized then this code is being run by eztest
    if (!initialized) {
        ERROR("eztest support stub\n");
        return SVC_REQ_ERROR_FAILURE;
    }

    // check if req_arg is NULL
    if (req_arg == NULL) {
        ERROR("req is NULL\n");
        return SVC_REQ_ERROR_FAILURE;
    }

    // get svc id for the svc_name
    id = svc_name_to_id(svc_name);
    if (id == -1) {
        ERROR("service %s not found\n", svc_name);
        return SVC_REQ_ERROR_SVC_NOT_FOUND;
    }
    x = &svcs[id];

    // verify svc state is okay to make a request
    okay = ((x->svc_state == SERVICE_STATE_RUNNING) ||
            (req_arg->id == SVC_REQ_ID_STOP && x->svc_state == SERVICE_STATE_STOPPING));
    if (!okay) {
        ERROR("service %s state %s invalid to make req\n", x->name, SERVICE_STATE_STR(x->svc_state));
        return SVC_REQ_ERROR_SVC_NOT_RUNNING;
    }

    // lock mutex1 and mutex2
    // - mutex1 prevents concurrent execution of this routine
    // - mutex2 prevents concurrent access to req and req_state, and is
    //   required by pthread_cond_timedwait
    pthread_mutex_lock(&x->req_mutex1);
    pthread_mutex_lock(&x->req_mutex2);

    // wait for req_state to be okay to proceed
    if (x->req_state == REQ_STATE_IDLE) {
        // req_state is IDLE; okay to proceed
    } else {
        // a prior req is still in progress, this is not normal
        // wait for the prior req to be COMPLETED
        POLL_FOR(x->req_state == REQ_STATE_COMPLETED, &x->req_mutex2, 30);
        if (x->req_state != REQ_STATE_COMPLETED) {
            ERROR("service %s state failed to become COMPLETED, state=%s\n",
                  x->name, SERVICE_STATE_STR(x->svc_state));
            pthread_mutex_unlock(&x->req_mutex2);
            pthread_mutex_unlock(&x->req_mutex1);
            return SVC_REQ_ERROR_TIMEDOUT;
        }
    }

    // copy the caller's req_arg to x->req;
    // set req_state to PENDING;
    // signal the cond to wake the svc which is waiting in svc_wait_for_req call to pthread_cond_wait
    x->req = *req_arg;
    x->req_state = REQ_STATE_PENDING;
    pthread_cond_signal(&x->req_cond);

    // poll for req to be COMPLETED, timeout is caller supplied timeout_secs arg
    POLL_FOR(x->req_state == REQ_STATE_COMPLETED, &x->req_mutex2, timeout_secs);
    if (x->req_state != REQ_STATE_COMPLETED) {
        ERROR("service %s timedout waiting for REQ_STATE_COMPLETED\n", svc_name);
        pthread_mutex_unlock(&x->req_mutex2);
        pthread_mutex_unlock(&x->req_mutex1);
        return SVC_REQ_ERROR_TIMEDOUT;
    }

    // copy req back to caller's buffer;
    // set req_state to IDLE;
    // unlock mutex;
    // return the req comp_status, usually 0 should indicate success
    *req_arg = x->req;
    x->req_state = REQ_STATE_IDLE;
    pthread_mutex_unlock(&x->req_mutex2);
    pthread_mutex_unlock(&x->req_mutex1);
    return req_arg->comp_status;
}

char *svc_make_req_status_to_str(int rc)
{
    switch (rc) {
    case SVC_REQ_ERROR_FAILURE: 
        return "FAILURE";
    case SVC_REQ_ERROR_SVC_NOT_FOUND: 
        return "SVC_NOT_FOUND";
    case SVC_REQ_ERROR_SVC_NOT_RUNNING: 
        return "SVC_NOT_RUNNING";
    case SVC_REQ_ERROR_TIMEDOUT: 
        return "TIMEDOUT";
    default:
        return "FAILURE";
    }
}

// ---- routines called by svcs ----

int svc_wait_for_req(char *svc_name, svc_req_t **req_arg, int timeout_secs)
{
    struct timespec abstime;
    int             id, ret;

    // if not initialized then this code is being run by eztest
    if (!initialized) {
        ERROR("eztest support stub\n");
        sleep(timeout_secs);
        return SVC_REQ_ERROR_TIMEDOUT;
    }

    // get svc id for the svc_name
    id = svc_name_to_id(svc_name);
    if (id == -1) {
        ERROR("service %s not found\n", svc_name);
        return SVC_REQ_ERROR_SVC_NOT_FOUND;
    }
    svc_t *x = &svcs[id];

    // determine abstime of timeout for call to pthread_cond_timedwait
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += timeout_secs;

    // acquire mutex
    pthread_mutex_lock(&x->req_mutex2);

    // wait, with timeout, for a request to be available 
    while (true) {
        // if req is pending then return copy of the req
        if (x->req_state == REQ_STATE_PENDING) {
            x->req_state = REQ_STATE_IN_PROGRESS;
            *req_arg = &x->req;
            pthread_mutex_unlock(&x->req_mutex2);
            return 0;
        }

        // wait for cond to be set, timeout at abstime
        ret = pthread_cond_timedwait(&x->req_cond, &x->req_mutex2, &abstime);
        if (ret != 0) {
            pthread_mutex_unlock(&x->req_mutex2);
            return SVC_REQ_ERROR_TIMEDOUT;
        }
    }
}

void svc_req_completed(char *svc_name, svc_req_t *req, int comp_status)
{
    int id;

    // get svc id for the svc_name
    id = svc_name_to_id(svc_name);
    if (id == -1) {
        ERROR("service %s not found\n", svc_name);
        return;
    }
    svc_t *x = &svcs[id];

    // req and x->req are expected to be the same
    if (req != &x->req) {
        ERROR("req not equal x->req\n");
    }

    // set req comp_status, and
    // set req_state to completed
    pthread_mutex_lock(&x->req_mutex2);
    x->req.comp_status = comp_status;
    x->req_state = REQ_STATE_COMPLETED;
    pthread_mutex_unlock(&x->req_mutex2);
}

// -----------------  UTILS  ----------------------------------------

// return -1 if svc_name not foumd, else return the id for svc_name
static int svc_name_to_id(char *svc_name)
{
    int id;

    for (id = 0; id < max_svcs; id++) {
        if (strcmp(svcs[id].name, svc_name) == 0) {
            return id;
        }
    }

    return -1;
}

static void remove_trailing_newline(char *s)
{
    int len = strlen(s);

    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
    }
}

// returns the number of services that are not in state SERVICE_STATE_DELETED
static int num_svcs(void)
{
    int num = 0;

    for (int id = 0; id < max_svcs; id++) {
        svc_t *x = svcs+id;
        if (x->svc_state != SERVICE_STATE_DELETED) {
            num++;
        }
    }

    return num;
}

