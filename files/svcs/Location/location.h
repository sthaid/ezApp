#ifndef __LOCATION_H__
#define __LOCATION_H__

#define SVC_LOCATION_REQ_GET_LOC_DATA_STR   10
#define SVC_LOCATION_REQ_ADD_COUNTRY_INFO   11
#define SVC_LOCATION_REQ_DEL_COUNTRY_INFO   12
#define SVC_LOCATION_REQ_LIST_COUNTRY_INFO  13
#define SVC_LOCATION_REQ_CLEAR_HISTORY      14
#define SVC_LOCATION_REQ_QUERY_ENABLED      15
#define SVC_LOCATION_REQ_SET_ENABLED        16
#define SVC_LOCATION_REQ_GET_LOC_INFO       17

#define LOC_HIST_FILENAME   "loc_hist.dat"
#define MAX_LOC_HIST        1000

#define MAX_NAME 32

typedef struct {
    int count;
    int pad;
    struct loc_hist_entry_s {
        char data_str[100];
    } loc[MAX_LOC_HIST];
} loc_hist_t;

#endif
