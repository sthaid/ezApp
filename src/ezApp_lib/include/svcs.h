#ifndef __SVCS_H__
#define __SVCS_H__

#ifdef __cplusplus
extern "C" {
#endif

// common req id value; the miniSvc must terminate when SVC_REQ_ID_STOP is received
#define SVC_REQ_ID_STOP 1

// sizeof of req->data
#define MAX_SVC_REQ_DATA 1000

// Status values returned by svc_make_req.
#define SVC_REQ_ERROR_FAILURE           -1
#define SVC_REQ_ERROR_SVC_NOT_FOUND     -2
#define SVC_REQ_ERROR_SVC_NOT_RUNNING   -3
#define SVC_REQ_ERROR_TIMEDOUT          -4

// svc request struct
typedef struct {
    int  id;
    int  comp_status;
    char data[MAX_SVC_REQ_DATA];
} svc_req_t;

// The svc_make_req routine is called by miniApps to issue a request to a MiniSvc.
// Return value is < 0 if the request could not be issued; otherwise the 
// return value is the completion status (comp_status) value provided by the
// miniSvc.
int svc_make_req(char *svc_name, svc_req_t *req, int timeout_secs);

// Convert status value returned by svc_make_req to string.
char *svc_make_req_status_to_str(int rc);

// The following 2 routines are called by the miniSvcs:
// - svc_wait_for_req waits for a request, or timeout. Return value 0
//   indicates a request was received; non-zero return value indicates a
//   timeout. MiniSvcs should make use of the timeout to perform periodic
//   processing.
// - svc_req_completed is called by a miniSvc after the miniSvc received
//   and finished processing a request. The miniApp call to scv_make_req will
//   return after the miniSvc has called svc_req_completed. 
//   MiniSvc updates made to req data are returned to the miniApp.
int svc_wait_for_req(char *svc_name, svc_req_t **req, int timeout_secs);
void svc_req_completed(char *svc_name, svc_req_t *req, int comp_status);

#ifdef __cplusplus
}
#endif

#endif
