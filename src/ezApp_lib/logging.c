#include <std_hdrs.h>
#include <sdlx.h>
#include <private.h>

// ----------------- LOG_MSG -------------------------

void log_msg(const char *lvl, const char *func, const char *fmt, ...)
{
    va_list ap;
    char    msg[1000];
    int     len;

    // construct msg
    va_start(ap, fmt);
    len = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    // remove terminating newline
    if (len > 0 && msg[len-1] == '\n') {
        msg[len-1] = '\0';
        len--;
    }

    // log to stderr, which is redirected to the log fifo
    fprintf(stderr, "%s %s: %s\n", lvl, func, msg);
}

#ifdef ANDROID

// ----------------- ANDROID LOGGING -----------------

#include <SDL3/SDL.h>
#include <android/log.h>
#include <private.h>

#define ANDROID_LOG_FIFO "log_fifo"

static int android_logging_thread(void *cx);

int log_init(void)
{
    int rc;
    FILE *fp;

    // set line buffering
    setlinebuf(stdout);
    setlinebuf(stderr);

    // make fifo, log_msg print will be directed to this fifo
    mkfifo(ANDROID_LOG_FIFO, 0666);

    // create thread that will read log_msg prints from the fifo
    sdlx_create_detached_thread(android_logging_thread, "logging", NULL);

    // reopen stdout using the fifo
    fp = freopen(ANDROID_LOG_FIFO, "w", stdout);
    if (fp == NULL) {
        ERROR("failed to reopen stdout, %s\n", strerror(errno));
        return -1;
    }
    setlinebuf(stdout);

    // dup stdout to stderr
    rc = dup2(fileno(stdout), fileno(stderr));
    if (rc < 0) {
        ERROR("failed to dup stdout to stderr, %s\n", strerror(errno));
        return -1;
    }

    // success
    return 0;
}

static int android_logging_thread(void *cx)
{
    char buff[10000];
    int len, fd;
    char *buffp, *p;

    // open the fifo
    fd = open(ANDROID_LOG_FIFO, O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    // loop forever
    while (true) {
        // read from the fifo
        len = read(fd, buff, sizeof(buff)-2);
        if (len <= 0) {
            goto done;
        }

        // if last char in buff is not newline, then add newline
        if (buff[len-1] != '\n') {
            len++;
            buff[len-1] = '\n';
        }

        // null terminate buff
        buff[len] = '\0';

        // loop over buffer, processing one line at a time until 
        // all lines in buffer are processed
        buffp = buff;
        while (true) {
            // find newline char and replace with string terminator
            p = strchr(buffp, '\n');
            if (p == NULL) {
                break;
            }
            *p = '\0';

            // call __android_log_write to log the string
            if (strncmp(buffp, "I ", 2) == 0) {
                __android_log_write(ANDROID_LOG_INFO, "EZAPP", buffp+2);
            } else if (strncmp(buffp, "E ", 2) == 0) {
                __android_log_write(ANDROID_LOG_ERROR, "EZAPP", buffp+2);
            } else if (strcasestr(buffp, "ERROR") != NULL ||
                       strcasestr(buffp, "FAIL") != NULL) 
            {
                __android_log_write(ANDROID_LOG_ERROR, "EZAPP", buffp);
            } else {
                __android_log_write(ANDROID_LOG_INFO, "EZAPP", buffp);
            }

            // advance bufp beyond the string just processed
            buffp = p + 1;
        }
    }

done:
    return 0;
}
    
#else

// ------------- LINUX LOGGING -----------------

#include <private.h>

// When this code is running on Linux, there is no logging thread.
// The log_msg routine print to stderr is not redirected to a fifo
// when running on Linux.

int log_init(void)
{
    // set line buffering
    setlinebuf(stdout);
    setlinebuf(stderr);
    return 0;
}

#endif
