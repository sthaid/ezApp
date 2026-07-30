// The code in this file is no longer being used.
// It is being kept for possible future reference.

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
    
#include <sdlx.h>
#include <utils.h>

#include "apps/Clock/common.h"

static int run_curl(char *url, char *filename);
static void *get_json_root(char *filename);

void sunrise_sunset_web(char *sunrise, char *sunset, char *midday, char *daytime)
{
    char         curl_url[200];
    json_value_t rise, set;
    int          ret, cnt;
    double       latitude, longitude;
    void        *root = NULL;
    struct tm    tm;
    time_t       t_rise_gm, t_set_gm, t_midday_gm;

    // preset return string values
    strcpy(sunrise, "N/A");
    strcpy(sunset, "N/A");
    strcpy(midday, "N/A");
    strcpy(daytime, "N/A");

    // get location
    util_get_location(&latitude, &longitude, NULL, NULL);
    if (latitude == INVALID_NUMBER || longitude == INVALID_NUMBER) {
        printf("E %s: failed to get gps location\n", progname);
        goto done;
    }

    // get sunrise and sunset times in json format from sunrise-sunset.org
    // references:
    //   https://sunrise-sunset.org/
    //   https://sunrise-sunset.org/api
    sprintf(curl_url, "\"https://api.sunrise-sunset.org/json?lat=%0.4f&lng=%0.4f&formatted=0\"", latitude, longitude);
    ret = run_curl(curl_url, "curl.out");
    if (ret != 0) {
        printf("E %s: run_curl failed\n", progname);
        goto done;
    }

    // parse ths json
    root = get_json_root("curl.out");
    if (root == NULL) {
        printf("E %s: json parse failed\n", progname);
        goto done;
    }

    // extract sunrise time from json
    rise = *util_json_get_value(root, "results", "sunrise", NULL);
    printf("I %s: web  RISE %s UTC\n", progname, rise.u.string);
    memset(&tm, 0, sizeof(tm));
    cnt = sscanf(rise.u.string, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    if (cnt != 6) {
        printf("E %s: sscanf failed, cnt=%d\n", progname, cnt);
        goto done;
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    t_rise_gm = timegm(&tm);
    localtime_r(&t_rise_gm, &tm);
    sprintf(sunrise, "%02d:%02d", tm.tm_hour, tm.tm_min);

    // extract sunset time from json
    set = *util_json_get_value(root, "results", "sunset", NULL);
    printf("I %s: web  SET  %s UTC\n", progname, set.u.string);
    memset(&tm, 0, sizeof(tm));
    cnt = sscanf(set.u.string, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    if (cnt != 6) {
        printf("E %s: sscanf failed, cnt=%d\n", progname, cnt);
        goto done;
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    t_set_gm = timegm(&tm);
    localtime_r(&t_set_gm, &tm);
    sprintf(sunset, "%02d:%02d", tm.tm_hour, tm.tm_min);

    // calculate midday time, (solar noon)
    char s[100];
    t_midday_gm = (t_rise_gm + t_set_gm) / 2;
    localtime_r(&t_midday_gm, &tm);
    strftime(s, sizeof(s), "%c", &tm);
    printf("I %s: web  MID  %s\n", progname, s);
    sprintf(midday, "%02d:%02d", tm.tm_hour, tm.tm_min);

    // determine the daytime length
    int minutes, hours;
    minutes = (t_set_gm - t_rise_gm) / 60;
    hours = minutes / 60;
    minutes -= 60 * hours;
    sprintf(daytime, "%02d:%02d", hours, minutes);

done:
    util_json_free(root);
}

static int run_curl(char *url, char *filename)
{
    char cmd[500];
    int  ret;

    util_delete_file(progname, filename);

    sprintf(cmd, "curl -s %s > %s/%s", url, data_dir, filename);
    ret = system(cmd);
    if (ret != 0) {
        printf("E %s: curl failed for url '%s'\n", progname, url);
        return -1;
    }

    return 0;
}

static void *get_json_root(char *filename)
{
    char *str, *end_ptr;
    void *root;
    int   len;

    str = util_read_file(data_dir, filename, &len);
    if (str == NULL) {
        printf("E %s: failed to read file %s/%s\n", progname, data_dir, filename);
        return NULL;
    }

    root = util_json_parse(str, &end_ptr);
    if (root == NULL) {
        printf("E %s: failed to parse json\n", progname);
        free(str);
        return NULL;
    }

    free(str);

    return root;
}
