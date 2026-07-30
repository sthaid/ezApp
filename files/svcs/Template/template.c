#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include <sdlx.h>
#include <svcs.h>

#include "svcs/Template/template.h"

// program args
char *progname;
char *data_dir;

// flag set when SVC_REQ_ID_STOP received
bool end_program = false;

// prototypes
void process_req(svc_req_t *req);
void periodic_processing(void);

// -----------------  TEMPLATE SERVICE  --------------------------------------

int main(int argc, char **argv)
{
    svc_req_t *req;
    int rc;

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

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
            
    // print terminating msg
    printf("I %s: terminating\n", progname);
    return 0;
}

// -----------------  PROCESS REQ  ---------------------------------

void process_req(svc_req_t *req)
{
    //printf("I %s: processing req id %d\n", progname, req->id);

    // process the request
    switch (req->id) {
    case SVC_REQ_ID_STOP:
        svc_req_completed(progname, req, 0);
        end_program = true;
        break;
    case SVC_TEMPLATE_REQ_ADD_ONE:
        *(int*)(&req->data[0]) = *(int*)(&req->data[0]) + 1;
        svc_req_completed(progname, req, 0);
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
    // print interval since last call
    static time_t t_last_call;
    time_t t_now = time(NULL);
    if (t_last_call != 0) {
        printf("I %s: periodic interval = %ld secs\n", progname, t_now-t_last_call);
    }
    t_last_call = t_now;
}
