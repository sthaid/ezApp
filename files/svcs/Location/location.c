#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <ctype.h>

#include <sdlx.h>
#include <utils.h>
#include <svcs.h>

#include "svcs/Location/location.h"
#include "svcs/Location/common.h"
#include "lib/lib.h"

// defines
#define CREATE_IF_NEEDED true

#define TEST // xxx del later

// variables
loc_hist_t  *loc_hist;
loc_hist2_t *loc_hist2;
bool         param_enabled;
bool         end_program;

// prototypes
void periodic_processing(void);

void process_req(svc_req_t *req);

void add_entry_to_loc_hist(char *city, char *state);
char *most_recent_loc_hist_city(void);
void clear_loc_history(void);

void add_entry_to_loc_hist2(double latitude, double longitude);

#ifdef TEST
bool test_mode;
void test_init_loc_hist2(void);
void test_get_current_ymd_hms(int *year, int *month, int *day, int *hour, int *minute, int *second);
#endif

// -----------------  MAIN  -----------------------------------------

int main(int argc, char **argv)
{
    svc_req_t *req;
    int        rc;
    int        created;

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // read location data
    rc = read_loc_data();
    if (rc != 0) {
        printf("E %s: failed to read location data\n", progname);
        return 1;
    }

    // map the loc_hist file
    loc_hist = util_map_file(data_dir, LOC_HIST_FILENAME, sizeof(loc_hist_t),
                             CREATE_IF_NEEDED, &created);
    if (loc_hist == NULL) {
        printf("E %s: failed to map %s\n", progname, LOC_HIST_FILENAME);
        return 1;
    }

    // map the loc_hist2 file
    loc_hist2 = util_map_file(data_dir, LOC_HIST2_FILENAME, sizeof(loc_hist2_t),
                             CREATE_IF_NEEDED, &created);
    if (loc_hist2 == NULL) {
        printf("E %s: failed to map %s\n", progname, LOC_HIST2_FILENAME);
        util_unmap_file(loc_hist, sizeof(loc_hist_t));
        return 1;
    }

#ifdef TEST
    if (created) {
        printf("I %s: xxxxxxxxxxxxxxxxxx INIT TEST DATA xxxxxxxxxxxxxxxxxxxxxxxx\n", progname);
        test_init_loc_hist2();
        printf("I %s: xxxxxxxxxxxxxxxxxx INIT TEST DATA DONE  xxxxxxxxxxxxxxxxxx\n", progname);
    }
#endif

    // read parameters
    param_enabled = util_get_numeric_param(data_dir, "enabled", 1);
    printf("I %s: history collection is %s\n", progname, param_enabled ? "enabled" : "disabled");

    // service runtime loop
    while (!end_program) {
        // wait for req or 10 sec timeout
        rc = svc_wait_for_req(progname, &req, 10);

        // if req was received then
        //   process the req
        // else
        //   do periodic processing
        // endif
        if (rc == 0) {
            process_req(req);
        } else {
            periodic_processing();
        }
    }

    // cleanup and end program
    free_loc_data();
    util_unmap_file(loc_hist, sizeof(loc_hist_t));
    util_unmap_file(loc_hist2, sizeof(loc_hist2_t));
    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  PERIODIC PROCESSSING  -------------------------

void periodic_processing(void)
{
    char   city[MAX_NAME];
    char   state[MAX_NAME];
    double latitude, longitude;

#if 0
    // print interval since last call
    static time_t t_last_call;
    time_t t_now = time(NULL);
    if (t_last_call != 0) {
        printf("I %s: periodic interval = %ld secs\n", progname, t_now-t_last_call);
    }
    t_last_call = t_now;
#endif

    // if location history is not enabled then return
    if (!param_enabled) {
        return;
    }

    // get current latitude and longitude
    util_get_location(&latitude, &longitude, NULL, NULL);
    if (latitude == INVALID_NUMBER || longitude == INVALID_NUMBER) {
        return;
    }

    // update loc_hist file ...
    // - find location in database that is closest to current lat/long;
    // - if city is different than most recent entry in loc_file
    //   then add new entry to loc file, 
    find_closest_loc_data(latitude, longitude, city, state, NULL, NULL);
    if (city[0] != '\0' && strcmp(most_recent_loc_hist_city(), city) != 0) {
        add_entry_to_loc_hist(city, state);
    }

    // update loc_hist2 file ...
    // xxx comment
    add_entry_to_loc_hist2(latitude, longitude);
}

// -----------------  PROCESS REQ  ----------------------------------

void process_req(svc_req_t *req)
{
    switch (req->id) {
    case SVC_REQ_ID_STOP:
        svc_req_completed(progname, req, 0);
        end_program = true;
        break;
    case SVC_LOCATION_REQ_GET_LOC_INFO: {
        // Returns location info for the nearest location to the requested 
        // latitude and longitude.
        // The following strings for the nearest location are returned,
        // separated by newline char
        // - city            name of nearest city
        // - state           name of nearest state
        // - latitude        of the nearest city
        // - longitude       of the nearest city
        double req_latitude, req_longitude;
        double actual_latitude, actual_longitude;
        char city[MAX_NAME];
        char state[MAX_NAME];

        req_latitude = *(double*)(&req->data[0]);
        req_longitude = *(double*)(&req->data[8]);
        find_closest_loc_data(req_latitude, req_longitude, city, state, &actual_latitude, &actual_longitude);

        sprintf(req->data, "%s\n%s\n%0.4f\n%0.4f\n", city, state, actual_latitude, actual_longitude);
        svc_req_completed(progname, req, 0);
        break; }
    case SVC_LOCATION_REQ_ADD_COUNTRY_INFO: {
        char *country_code = req->data;

        if (strlen(country_code) != 2) {
            printf("E %s: invalid country code '%s', len must be 2\n", progname, country_code);
            svc_req_completed(progname, req, 99);
            break;
        }

        for (int i = 0; i < strlen(country_code); i++) {
            country_code[i] = tolower(country_code[i]);
        }
            
        download_country_loc_data(country_code);
        read_loc_data();
        svc_req_completed(progname, req, 0);
        break; }
    case SVC_LOCATION_REQ_DEL_COUNTRY_INFO: {
        char filename[1500];

        sprintf(filename, "%s.loc", req->data);
        util_delete_file(data_dir, filename);
        read_loc_data();
        svc_req_completed(progname, req, 0);
        break; }
    case SVC_LOCATION_REQ_LIST_COUNTRY_INFO: {
        FILE *fp;
        char *p, *p2, s[100], cmd[100];

        sprintf(cmd, "cd %s; ls -1 *.loc", data_dir);
        fp = popen(cmd, "r");
        if (fp == NULL) {
            strcpy(req->data, "No Country Info");
            svc_req_completed(progname, req, 0);
            break;
        }
        p = req->data;
        while (fgets(s, sizeof(s), fp) != NULL) {
            p2 = strstr(s, ".loc");
            if (p2) {
                *p2 = '\0';
                p += sprintf(p, "%s\n", s);
            }
        }
        pclose(fp);
        svc_req_completed(progname, req, 0);
        break; }
    case SVC_LOCATION_REQ_CLEAR_HISTORY: {
        clear_loc_history();
        svc_req_completed(progname, req, 0);
        break; }
    case SVC_LOCATION_REQ_QUERY_ENABLED: {
        req->data[0] = param_enabled;
        svc_req_completed(progname, req, 0);
        break; }
    case SVC_LOCATION_REQ_SET_ENABLED: {
        param_enabled = req->data[0];
        util_set_numeric_param(data_dir, "enabled", param_enabled);
        printf("I %s: history collection is now %s\n",
               progname,
               param_enabled ? "enabled" : "disabled");
        svc_req_completed(progname, req, 0);
        break; }
    default:
        printf("E %s: req %d is invalid\n", progname, req->id);
        svc_req_completed(progname, req, 99);
        break;
    }
}

// -----------------  LOC_HIST SUPPORT  -----------------------------

void add_entry_to_loc_hist(char *city, char *state)
{
    // if city is empty string then return
    if (city[0] == '\0') {
        printf("E %s: add_entry_to_loc_hist called with empty city str\n", progname);
        return;
    }

    // if buffer is full then discard the first half (oldest data)
    // xxx maybe make loc_hist a cirular file too
    if (loc_hist->count == MAX_LOC_HIST) {
        memmove(&loc_hist->loc[0], 
                &loc_hist->loc[MAX_LOC_HIST/2], 
                (MAX_LOC_HIST/2)*sizeof(loc_hist->loc[0]));

        memset(&loc_hist->loc[MAX_LOC_HIST/2],
               0,
               (MAX_LOC_HIST/2)*sizeof(loc_hist->loc[0]));

        loc_hist->count = MAX_LOC_HIST/2;
    }

    // get time_str
    struct tm *tm;
    char time_str[50];
    time_t t = time(NULL);
    tm = localtime(&t);
    strftime(time_str, sizeof(time_str), "%b %d %H:%M %Z", tm);

    // combine city and state to single string
    char city_and_state_str[200];
    if (state[0] != '\0') {
        sprintf(city_and_state_str, "%s\n%s", city, state);
    } else {
        sprintf(city_and_state_str, "%s", city);
    }

    // add entry
    snprintf(loc_hist->loc[loc_hist->count].data_str, sizeof(loc_hist->loc[0].data_str),
             "%s\n%s\n\n", city_and_state_str, time_str);
    loc_hist->count++;

    // sync memory mapped buffer to storage
    util_sync_file(loc_hist, sizeof(loc_hist_t));
}

char *most_recent_loc_hist_city(void)
{
    static char city[MAX_NAME];
    char *ptr, *data_str;

    if (loc_hist->count == 0) {
        return "";
    }

    data_str = loc_hist->loc[loc_hist->count-1].data_str;

    ptr = strchr(data_str, '\n');
    if (ptr == NULL) {
        printf("E %s: newline char not found in data_str '%s'\n", progname, data_str);
        return "";
    }

    memcpy(city, data_str, ptr-data_str);
    city[ptr-data_str] = '\0';

    return city;
}

void clear_loc_history(void)
{
    loc_hist->count++;
    memset(loc_hist, 0, sizeof(loc_hist_t));
    util_sync_file(loc_hist, sizeof(loc_hist_t));
}

// -----------------  LOC_HIST2 SUPPORT  -----------------------------

#define SYNC_INTVL_USECS (300 * 1000000000L)  // 5 minutes

void add_entry_to_loc_hist2(double latitude, double longitude)
{
    int                     year, month, day, hour, minute, second;
    long                    t_now;
    struct loc_hist2_day_s *lh2d;

    static long             t_last_sync;

    // get current data
#ifdef TEST
    if (test_mode) {
        test_get_current_ymd_hms(&year, &month, &day, &hour, &minute, &second);
    } else {
        get_current_ymd_hms(&year, &month, &day, &hour, &minute, &second);
    }
#else
    get_current_ymd_hms(&year, &month, &day, &hour, &minute, &second);
#endif
    
    // if current ymd differ from last_day's date then advance last_day
    lh2d = &loc_hist2->day[loc_hist2->last_day % MAX_LH2_DAY];
    if (year != lh2d->year || month != lh2d->month || day != lh2d->day) {
        loc_hist2->last_day++;
        lh2d = &loc_hist2->day[loc_hist2->last_day % MAX_LH2_DAY];
        lh2d->year    = year;
        lh2d->month   = month;
        lh2d->day     = day;
        lh2d->max_loc = 0;
    }

    // add new entry to the last_day data
    if (lh2d->max_loc < MAX_LH2_LOC) {
        lh2d->loc[lh2d->max_loc].secs = 3600*hour + 60*minute + second;
        lh2d->loc[lh2d->max_loc].latitude = latitude;
        lh2d->loc[lh2d->max_loc].longitude = longitude;
        lh2d->max_loc++;
    }

    // sync file periodically
    t_now = util_microsec_timer();
    if ((t_now - t_last_sync) > SYNC_INTVL_USECS) {
        printf("I %s: syncing %s\n", progname, LOC_HIST2_FILENAME);
        util_sync_file(loc_hist2, sizeof(loc_hist2_t));
        t_last_sync = t_now;
    }
}    

#ifdef TEST

#define HOUR 3600
#define TEST_LATITUDE     42.4222
#define TEST_LONGITUDE   -71.6226

#define MILES_PER_DEGREE_LATITUDE       69.0
#define MILES_PER_DEGREE_LONGITUDE(lat) (cosd(lat) * 69.0)

void test_init_loc_hist2()
{
    double lat, lng;
    int day, secs;

    test_mode = true;

    for (day = 0; day < 10; day++) {
        lat = TEST_LATITUDE;
        lng = TEST_LONGITUDE;

        for (secs = 0; secs < 86400; secs += 10) {
            if (secs < 6 * HOUR || secs > 20 * HOUR) {
                lat = TEST_LATITUDE;
                lng = TEST_LONGITUDE;
            } else if (secs < 9.5 * HOUR) {
                lat -= (1. / 1260) / MILES_PER_DEGREE_LATITUDE;
            } else if (secs < 13.0 * HOUR) {
                lng += (1. / 1260) / MILES_PER_DEGREE_LONGITUDE(TEST_LATITUDE);
            } else if (secs < 16.5 * HOUR) {
                lat += (1. / 1260) / MILES_PER_DEGREE_LATITUDE;
            } else {  // secs must be less than 20*HOUR
                lng -= (1. / 1260) / MILES_PER_DEGREE_LONGITUDE(TEST_LATITUDE);
            }

            add_entry_to_loc_hist2(lat, lng);
        }
    }

    test_mode = false;
}

void test_get_current_ymd_hms(int *year, int *month, int *day, int *hour, int *minute, int *second)
{
    static long seconds;

    *year    = 2026;
    *month   = 9;
    *day     = (seconds / 86400) + 1;
    *hour    = (seconds / 3600) % 24;
    *minute  = (seconds / 60) % 60;
    *second  = seconds % 60;

    seconds += 10;
}

#endif
