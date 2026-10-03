#ifndef __LOCATION_H__
#define __LOCATION_H__

#include <time.h>

// xxx comments needed

// -----------------

#define SVC_LOCATION_REQ_ADD_COUNTRY_INFO   11
#define SVC_LOCATION_REQ_DEL_COUNTRY_INFO   12
#define SVC_LOCATION_REQ_LIST_COUNTRY_INFO  13
#define SVC_LOCATION_REQ_CLEAR_HISTORY      14
#define SVC_LOCATION_REQ_QUERY_ENABLED      15
#define SVC_LOCATION_REQ_SET_ENABLED        16
#define SVC_LOCATION_REQ_GET_LOC_INFO       17

// -----------------

#define LOC_HIST_FILENAME   "loc_hist.dat"
#define MAX_LOC_HIST        1000

typedef struct {
    int count;
    int pad;
    struct loc_hist_entry_s {
        char data_str[100];
    } loc[MAX_LOC_HIST];
} loc_hist_t;

// -----------------

#define LOC_HIST2_FILENAME   "loc_hist2.dat"

#define MAX_LH2_DAY 30
#define MAX_LH2_LOC 8640   // sample approximately every 10 seconds for 1 day

typedef struct {
    int last_day;   // doesn't wrap
    struct loc_hist2_day_s {
        int year;
        int month;
        int day;
        int max_loc;
        struct {
            long   secs;  // seconds into the day, range 0-86399
            double latitude;
            double longitude;
        } loc[MAX_LH2_LOC];
    } day[MAX_LH2_DAY];
} loc_hist2_t;

#endif
