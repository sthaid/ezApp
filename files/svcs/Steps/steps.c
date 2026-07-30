#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include <sdlx.h>
#include <utils.h>
#include <svcs.h>

#include "svcs/Steps/steps.h"

// defines
#define CREATE_IF_NEEDED true

// program args
char *progname;
char *data_dir;

// variables
bool          end_program = false;
steps_file_t *steps_file;

// prototypes
void process_req(svc_req_t *req);
void periodic_processing(void);

// -----------------  MAIN  ----------------------------------------

int initialize(void);
void cleanup(void);

int main(int argc, char **argv)
{
    svc_req_t *req;
    int        rc;

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // initialize
    rc = initialize();
    if (rc != 0) {
        printf("E %s: failed to initialize\n", progname);
        cleanup();
        return 1;
    }

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

    // cleanup
    cleanup();
            
    // print terminating msg
    printf("I %s: terminating\n", progname);
    return 0;
}

int initialize(void)
{
    int created_flag;

    printf("I %s: sizeof(steps_file_t) = 0x%zx %0.3f MB\n",
           progname, sizeof(steps_file_t), (double)sizeof(steps_file_t)/0x100000);

    // map the steps data file;
    // create the file if needed because either it doesn't exist or has incorrect size
    steps_file = util_map_file(data_dir, STEPS_FILENAME, sizeof(steps_file_t),
                               CREATE_IF_NEEDED, &created_flag);
    if (steps_file == NULL) {
        printf("E %s: failed to map %s\n", progname, STEPS_FILENAME);
        return -1;
    }

    // if file was created or has incorrect version then init the file
    if (created_flag || steps_file->version != STEPS_FILE_VERSION) {
        printf("I %s: initializing steps_file\n", progname);
        memset(steps_file, 0, sizeof(steps_file_t));
        steps_file->version = STEPS_FILE_VERSION;
        util_sync_file(steps_file, sizeof(steps_file_t));
    }

    // success
    return 0;
}

void cleanup(void)
{
    if (steps_file) {
        util_unmap_file(steps_file, sizeof(steps_file_t));
        steps_file = NULL;
    }
}

// -----------------  PROCESS REQ  ---------------------------------

void process_req(svc_req_t *req)
{
    printf("I %s: processing req id %d\n", progname, req->id);

    // process the request
    switch (req->id) {
    case SVC_REQ_ID_STOP:
        svc_req_completed(progname, req, 0);
        end_program = true;
        break;
    default:
        printf("E %s: req %d is invalid\n", progname, req->id);
        svc_req_completed(progname, req, 99);
        break;
    }
}

// -----------------  PERIODIC PROCESSING  -------------------------

void periodic_processing(void)
{
    unsigned long step_count_sensor;
    int           steps, rc, year, month, day, hour;
    time_t        t_now;
    struct tm     tm;

    static time_t        t_last_sync;
    static bool          sync_request;
    static unsigned long last_step_count_sensor;

    // get current time
    t_now = time(NULL);

#if 0
    // print interval since last call
    static time_t t_last_call;
    if (t_last_call != 0) {
        printf("I %s: periodic interval = %ld secs\n", progname, t_now-t_last_call);
    }
    t_last_call = t_now;
#endif

    // if sync_request is pending and steps_file has not been synced for 60 seconds
    // then sync the steps file
    if (sync_request && (t_now - t_last_sync > 60)) {
        printf("I %s: calling util_sync_file\n", progname);
        util_sync_file(steps_file, sizeof(steps_file_t));
        t_last_sync = t_now;
        sync_request = false;
    }

    // read step counter sensor
    rc = sdlx_sensor_read_step_counter(&step_count_sensor);
    if (rc != 0) {
        //printf("E %s: failed to read step counter sensor\n", progname);
        return;
    }

    // if don't yet have the last_step_count_sensor value then set it and return
    if (last_step_count_sensor == 0) {
        last_step_count_sensor = step_count_sensor;
        printf("I %s: init last_step_count_sensor %ld\n", progname, last_step_count_sensor);
        return;
    }

    // determine number of steps since previous read of the sensor
    steps = (step_count_sensor - last_step_count_sensor);
    last_step_count_sensor = step_count_sensor;

    // if no new steps then return
    if (steps == 0) {
        return;
    }

    // accumulate steps to memory mapped steps_file
    localtime_r(&t_now, &tm);
    year  = tm.tm_year + 1900 - YEAR0;
    month = tm.tm_mon;       // 0 - 11
    day   = tm.tm_mday - 1;  // 0 - 30
    hour  = tm.tm_hour;      // 0 - 23 
    steps_file->year[year]                   += steps;
    steps_file->month[year][month]           += steps;
    steps_file->day[year][month][day]        += steps;
    steps_file->hour[year][month][day][hour] += steps;

    // set sync_request flag
    sync_request = true;

    // debug print
    printf("I %s: ymdh=%d %d %d %d  steps=%d  sensor=%ld\n", 
           progname, year, month, day, hour, steps, step_count_sensor);
}
