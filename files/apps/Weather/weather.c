#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/lib/lib.h"

//
// defines
//

#define MAX_FORECAST 100

#define DAILY          0
#define DAY_AND_NIGHT  1
#define HOURLY         2
#define MAX_MODE       3

#define EVID_MODE_SELECT      1
#define EVID_RELOAD_FORECAST  2
#define EVID_FORECAST         100 // 100..(100+MAX_FORECASST)

#define PARSE_DAILY_FORECAST  1
#define PARSE_HOURLY_FORECAST 2

//
// typedefs
//

typedef struct {
    char *city;
    char *state;
    char *forecast_daily_url;
    char *forecast_hourly_url;
} info_t;

typedef struct {
    bool            valid;
    bool            is_daytime;
    char           *day_name;
    char           *day_name_unabbreviated;
    char           *icon_url;
    char           *icon_filename;
    sdlx_texture_t *icon_texture;
    char           *short_forecast;
    char           *detailed_forecast;
    char           *temperature;
    char           *wind;
    char           *precip;
} forecast_t;

//
// variables
//

char       *progname;
char       *data_dir;
char        icon_dir[100];

int         mode = DAILY;
bool        force_reload;

info_t      info;
forecast_t  daily[MAX_FORECAST];
forecast_t  hourly[MAX_FORECAST];

int         y;
int         y_top;
int         y_bottom;

//
// prototypes
//

void cleanup_all(void);
void cleanup_info(void);
void cleanup_forecast(forecast_t *forecast);

bool is_forecast_download_needed(void);
char *download_and_parse_forecast(void);

int parse_info(void);
int parse_forecast(int which);
void create_icon_texture_if_needed(forecast_t *x);

void display_forecast(void);
void display_detailed_forecast(int idx);

char *get_day_name(char * str, bool unabbreviated);

// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
    sdlx_event_t  event;
    bool          end_program = false;
    char         *mode_str;
    char         *state = "Loading";

    // save args
    progname = argv[0];
    if (argc != 2) {
        printf("E %s: data_dir arg expected\n", progname);
        return 1;
    }
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // create icon dir
    util_create_dir(data_dir, "icon");
    sprintf(icon_dir, "%s/%s", data_dir, "icon");

    // init variables that define the display region
    y_top    = 100;
    y_bottom = sdlx_win_height;
    y        = y_top;

    // init font size and color
    sdlx_print_set_default(FONT_SMALL, COLOR_WHITE);

    // runtime loop
    while (!end_program) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // display forecast         
        if (strcmp(state, "Loaded") == 0) {
            display_forecast();
        } else {
            sdlx_render_printf_ex2(sdlx_win_width/2, sdlx_win_height/2, 
                                   FONT_NORMAL, COLOR_WHITE, FLAG_XY_CTR, 
                                   "%s", state);
        }

        // register EVID_SHOW_README_FILE event
        reg_event_show_readme_file();

        // register events
        mode_str = (mode == DAILY         ? "Day" :
                   (mode == DAY_AND_NIGHT ? "D+N" 
                                          : "Hour"));
        sdlx_register_control_events(EVID_MODE_SELECT, mode_str,
                                     EVID_RELOAD_FORECAST, "R",
                                     EVID_QUIT, "X");
        sdlx_register_event(NULL, EVID_MOTION);

        // present the display
        sdlx_display_present();

        // if state is "Loading" then call download_and_parse_forecast
        if (strcmp(state, "Loading") == 0) {
            state = download_and_parse_forecast();
            continue;
        }

        // wait for event, with infinite timeout
        sdlx_get_event(-1, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_QUIT:
            end_program = true;
            break;
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_RELOAD_FORECAST:
            state = "Loading";
            force_reload = true;
            break;
        case EVID_MODE_SELECT:
            mode = (mode + 1) % MAX_MODE;
            y = y_top;
            break;
        case EVID_MOTION:
            y += event.u.motion.yrel;
            if (y >= y_top) {
                y = y_top;
            }
        }

        // if event is to display the detailed forecast then do so
        if (event.event_id >= EVID_FORECAST && event.event_id < EVID_FORECAST+MAX_FORECAST) {
            int idx = event.event_id - EVID_FORECAST;
            display_detailed_forecast(idx);
        }
    }

    // cleanup and end program
    cleanup_all();
    printf("I %s: terminating\n", progname);
    return 0;
}

void cleanup_all(void)
{
    cleanup_info();
    cleanup_forecast(daily);
    cleanup_forecast(hourly);
}

void cleanup_info(void)
{
    free(info.city);
    free(info.state);
    free(info.forecast_daily_url);
    free(info.forecast_hourly_url);

    memset(&info, 0, sizeof(info));
}

void cleanup_forecast(forecast_t *forecast)
{
    for (int i = 0; i < MAX_FORECAST; i++) {
        free(forecast[i].day_name);
        free(forecast[i].day_name_unabbreviated);
        free(forecast[i].icon_url);
        free(forecast[i].icon_filename);
        sdlx_destroy_texture(forecast[i].icon_texture);
        free(forecast[i].short_forecast);
        free(forecast[i].detailed_forecast);
        free(forecast[i].temperature);
        free(forecast[i].wind);
        free(forecast[i].precip);
    }

    memset(forecast, 0, MAX_FORECAST * sizeof(forecast_t));
}

// -----------------  GET FORECAST JSON FILES  ---------------

#define HEADER "\"User-Agent: (ezApp-Weather, stevenhaid@gmail.com)\""
#define AGE_LIMIT (4*3600)

bool is_forecast_download_needed(void)
{
    long tnow, mtime;

    if (force_reload) {
        printf("I %s: download forecast needed, due to force_reload\n", progname);
        force_reload = false;
        return true;
    }

    tnow = time(NULL);
    mtime = util_file_mtime(data_dir, "info.json");
    if (mtime == 0 || tnow - mtime/1000000 > AGE_LIMIT) {
        printf("I %s: download forecast needed\n", progname);
        return true;
    }
    mtime = util_file_mtime(data_dir, "daily.json");
    if (mtime == 0 || tnow - mtime/1000000 > AGE_LIMIT) {
        printf("I %s: download forecast needed\n", progname);
        return true;
    }
    mtime = util_file_mtime(data_dir, "hourly.json");
    if (mtime == 0 || tnow - mtime/1000000 > AGE_LIMIT) {
        printf("I %s: download forecast needed\n", progname);
        return true;
    }

    printf("I %s: download forecast not needed\n", progname);
    return false;
}

char *download_and_parse_forecast(void)
{
    char   url[200], cmd[1000];
    char   daily_json[200];
    char   hourly_json[200];
    long   start_us;
    int    rc;
    double latitude, longitude;

    start_us = util_microsec_timer();

    if (is_forecast_download_needed()) {
        printf("I %s: download_and_parse_forecast starting\n", progname);

        // cleanup forecast data structures
        cleanup_all();

        // delete existing forecast files
        util_delete_file(data_dir, "info.json");
        util_delete_file(data_dir, "daily.json");
        util_delete_file(data_dir, "hourly.json");

        // get lat/long
        util_get_location(&latitude, &longitude, NULL, NULL);
        if (latitude == INVALID_NUMBER || longitude == INVALID_NUMBER) {
            printf("E %s: failed to get lat/long\n", progname);
            return "Get Loc Failure";
        }
        printf("I %s: long/lat = %0.4f %0.4f\n", progname, latitude, longitude);

        // get forecast information, save to info.json
        sprintf(url, "https://api.weather.gov/points/%0.4f,%0.4f", latitude, longitude);
        sprintf(cmd, "curl --silent --max-time 30 --output %s/%s --header %s %s",
                data_dir, "info.json", HEADER, url);
        printf("I %s: RUNNING '%s'\n", progname, cmd);
        rc = system(cmd);
        rc = WEXITSTATUS(rc);
        if (rc != 0) {
            printf("E %s: system curl info.json failed, rc=0x%x\n", progname, rc);
            return "Failed 1";
        }

        // parse file info.json, this will obtain the following, which are used below:
        // - info.forecast_daily_url
        // - info.forecast_hourly_url
        rc = parse_info();
        if (rc != 0) {
            printf("E %s: parse info.json failed\n", progname);
            return "Failed 2";
        }

        // dowload daily forecast, save to daily.json
        sprintf(daily_json, "%s/daily.json", data_dir);
        sprintf(cmd, "curl --silent --max-time 30 --output %s --header %s %s",
                daily_json, HEADER, info.forecast_daily_url);
        printf("I %s: RUNNING '%s'\n", progname, cmd);
        rc = system(cmd);
        rc = WEXITSTATUS(rc);
        if (rc != 0) {
            printf("E %s: system curl daily.json failed, rc=0x%x\n", progname, rc);
            return "Failed 3";
        }

        // dowload hourly forecast, save to hourly.json
        sprintf(hourly_json, "%s/hourly.json", data_dir);
        sprintf(cmd, "curl --silent --max-time 30 --output %s --header %s %s",
                hourly_json, HEADER, info.forecast_hourly_url);
        printf("I %s: RUNNING '%s'\n", progname, cmd);
        rc = system(cmd);
        rc = WEXITSTATUS(rc);
        if (rc != 0) {
            printf("E %s: system curl hourly.json failed, rc=0x%x\n", progname, rc);
            return "Failed 4";
        }
    }

    // verify that the json files exist
    if (!util_file_exists(data_dir, "info.json")) {
        printf("E %s: info.json does not exist\n", progname);
        return "Failed 5";
    }
    if (!util_file_exists(data_dir, "daily.json")) {
        printf("E %s: daily.json does not exist\n", progname);
        return "Failed 6";
    }
    if (!util_file_exists(data_dir, "hourly.json")) {
        printf("E %s: hourly.json does not exist\n", progname);
        return "Failed 7";
    }

    // parse info.json;
    // this is needed to acquire info.city and info.state when 
    // parse_info had not been called earlier in this routine
    if (info.city == NULL || info.state == NULL) {
        rc = parse_info();
        if (rc != 0) {
            printf("E %s: parse info failed\n", progname);
            return "Failed 8";
        }
    }

    // parse the daily and hourly forecast json files
    rc = parse_forecast(PARSE_DAILY_FORECAST);
    if (rc != 0) {
        printf("E %s: parse daily forecast failed\n", progname);
        return "Failed 9";
    }
    rc = parse_forecast(PARSE_HOURLY_FORECAST);
    if (rc != 0) {
        printf("E %s: parse hourly forecast failed\n", progname);
        return "Failed 10";
    }

    // print completed, with duration this routine took
    printf("I %s: download_and_parse_forecast done, %.1f secs\n", 
           progname, (util_microsec_timer() - start_us) / 1000000.);
    return "Loaded";
}

// -----------------  PARSE JSON FILES  ---------------------------

int parse_info(void)
{
    char         *str = NULL;
    void         *json = NULL;
    json_value_t *value;
    int           ret = -1, len_ret;

    // free existing allocations
    cleanup_info();

    // read file info.json
    str = util_read_file(data_dir, "info.json", &len_ret);
    if (str == NULL) {
        printf("E %s: parse_info, read info.json, %s\n", progname, strerror(errno));
        goto cleanup_and_return;
    }

    // init json parser
    json = util_json_parse(str, NULL);
    if (json == NULL) {
        printf("E %s: parse_info, parse json\n", progname);
        goto cleanup_and_return;
    }

    // extract needed fields from json
    // - forecast        : daily forecast url
    // - forecastHourly  : hourly forecast url
    // - city, state     : location of the forecast
    value = util_json_get_value(json, "properties", "forecast", NULL);
    if (value->type != JSON_TYPE_STRING) {
        printf("E %s: parse_info, get forecast url %d\n", progname, value->type);
        goto cleanup_and_return;
    }
    info.forecast_daily_url = strdup(value->u.string);

    value = util_json_get_value(json, "properties", "forecastHourly", NULL);
    if (value->type != JSON_TYPE_STRING) {
        printf("E %s: parse_info, get forecastHourly url %d\n", progname, value->type);
        goto cleanup_and_return;
    }
    info.forecast_hourly_url = strdup(value->u.string);

    value = util_json_get_value(json, "properties", "relativelocation", "properties", "city", NULL);
    if (value->type != JSON_TYPE_STRING) {
        printf("E %s: parse_info, city %d\n", progname, value->type);
        goto cleanup_and_return;
    }
    info.city = strdup(value->u.string);

    value = util_json_get_value(json, "properties", "relativelocation", "properties", "state", NULL);
    if (value->type != JSON_TYPE_STRING) {
        printf("E %s: parse_info, state %d\n", progname, value->type);
        goto cleanup_and_return;
    }
    info.state = strdup(value->u.string);

    // debug print the results
    printf("I %s: parse info.json results:\n", progname);
    printf("I %s:   location = %s %s\n", progname, info.city, info.state);
    printf("I %s:   daily    = %s\n", progname, info.forecast_daily_url);
    printf("I %s:   hourly   = %s\n", progname, info.forecast_hourly_url);

    // success
    ret = 0;

cleanup_and_return:
    util_json_free(json);
    free(str);
    return ret;
}

int parse_forecast(int which)
{
    char *str = NULL;
    void *json = NULL;
    int   len_ret;
    char *json_filename;
    forecast_t *forecast;
    long start_us;

    // init json_filename, and forecast variables based on 'which' arg
    if (which == PARSE_DAILY_FORECAST) {
        json_filename = "daily.json";
        forecast = daily;
    } else {
        json_filename = "hourly.json";
        forecast = hourly;
    }
    cleanup_forecast(forecast);

    // print starting msg
    printf("I %s: parse_forecast %s starting\n", progname, json_filename);
    start_us = util_microsec_timer();

    // read forecast json file
    str = util_read_file(data_dir, json_filename, &len_ret);
    if (str == NULL) {
        printf("E %s: parse_forecast, read forecast json, %s\n", progname, strerror(errno));
        return -1;
    }

    // init json parser
    json = util_json_parse(str, NULL);
    if (json == NULL) {
        printf("E %s: parse_forecast, parse json\n", progname);
        free(str);
        return -1;
    }

    // loop over the periods in the json file
    for (int i = 0; i < MAX_FORECAST; i++) {
        forecast_t   *x = &forecast[i];
        json_value_t *value;
        void         *period;
        char          tmp_str[1000];
        char          period_str[200];

        // get json period object
        sprintf(period_str, "%d", i);
        value = util_json_get_value(json, "properties", "periods", period_str, NULL);
        if (value->type != JSON_TYPE_OBJECT) {
            break;
        }
        period = value->u.object;

        // get is_daytime
        value = util_json_get_value(period, "isDaytime", NULL);
        if (value->type != JSON_TYPE_FLAG) {
            printf("E %s: failed to get isDaytime, %d\n", progname, value->type);
            continue;
        }
        x->is_daytime = value->u.flag;

        // get day_name based on startTime of this period; and
        // also based o the is_daytime flag
        value = util_json_get_value(period, "startTime", NULL);
        if (value->type != JSON_TYPE_STRING) {
            printf("E %s: failed to get startTime, %d\n", progname, value->type);
            continue;
        }
        if (which == PARSE_DAILY_FORECAST) {
            if (x->is_daytime) {
                sprintf(tmp_str, "%s", get_day_name(value->u.string, false));
            } else {
                sprintf(tmp_str, "%s Night", get_day_name(value->u.string, false));
            }
        } else {
            int hour;
            sscanf(value->u.string+10, "T%d", &hour);
            char *ampm = hour < 12 ? "AM" : "PM";
            if (hour > 12) hour -= 12;
            if (hour == 0) hour = 12;
            sprintf(tmp_str, "%s %d%s", get_day_name(value->u.string, false), hour, ampm);
        }
        x->day_name = strdup(tmp_str);

        // get day_name_unabbreviated based on startTime of this period; and
        // also based o the is_daytime flag
        value = util_json_get_value(period, "startTime", NULL);
        if (value->type != JSON_TYPE_STRING) {
            printf("E %s: failed to get startTime, %d\n", progname, value->type);
            continue;
        }
        if (which == PARSE_DAILY_FORECAST) {
            if (x->is_daytime) {
                sprintf(tmp_str, "%s", get_day_name(value->u.string, true));
            } else {
                sprintf(tmp_str, "%s Night", get_day_name(value->u.string, true));
            }
        } else {
            int hour;
            sscanf(value->u.string+10, "T%d", &hour);
            char *ampm = hour < 12 ? "AM" : "PM";
            if (hour > 12) hour -= 12;
            if (hour == 0) hour = 12;
            sprintf(tmp_str, "%s %d%s", get_day_name(value->u.string, true), hour, ampm);
        }
        x->day_name_unabbreviated = strdup(tmp_str);

        // get icon_url
        value = util_json_get_value(period, "icon", NULL);
        if (value->type != JSON_TYPE_STRING) {
            printf("E %s: failed to get icon, %d\n", progname, value->type);
            continue;
        }
        x->icon_url = strdup(value->u.string);

        // create icon filename from icon url
        // url example: https://api.weather.gov/icons/land/day/few?size=medium
        // - remove leading https://api.weather.gov/
        // - replace '/' chars with '-'
        // - append ".png"
        if (strncmp(x->icon_url, "https://api.weather.gov/", 24) != 0) {
            printf("E %s: unexpected icon_url %s\n", progname, x->icon_url);
            continue;
        } else {
            strcpy(tmp_str, x->icon_url+24);
            for (int ii = 0; tmp_str[ii]; ii++) {
                if (tmp_str[ii] == '/') tmp_str[ii] = '-';
            }
            strcat(tmp_str, ".png");
            x->icon_filename = strdup(tmp_str);
        }

        // get shortForecast
        value = util_json_get_value(period, "shortForecast", NULL);
        if (value->type != JSON_TYPE_STRING) {
            printf("E %s: failed to get shortForecast, %d\n", progname, value->type);
            continue;
        }
        x->short_forecast = strdup(value->u.string);

        // get detailedForecast
        value = util_json_get_value(period, "detailedForecast", NULL);
        if (value->type != JSON_TYPE_STRING) {
            printf("E %s: failed to get detailedForecast, %d\n", progname, value->type);
            continue;
        }
        if (strlen(value->u.string) > 0) {
            sprintf(tmp_str, "%s. %s", x->day_name_unabbreviated, value->u.string);
            x->detailed_forecast = strdup(tmp_str);
        } else {
            x->detailed_forecast = strdup("");
        }

        // get temperature, and append temperatureUnit
        value = util_json_get_value(period, "temperature", NULL);
        if (value->type != JSON_TYPE_NUMBER) {
            printf("E %s: failed to get temperature, %d\n", progname, value->type);
            continue;
        }
        double temperature = value->u.number;
        value = util_json_get_value(period, "temperatureUnit", NULL);
        if (value->type != JSON_TYPE_STRING) {
            printf("E %s: failed to get temperatureUnit, %d\n", progname, value->type);
            continue;
        }
        sprintf(tmp_str, "%.0f%s", temperature, value->u.string);
        x->temperature = strdup(tmp_str);

        // get wind speed and direction
        int cnt, low, high;
        char wind_speed[40], wind_dir[40];

        value = util_json_get_value(period, "windSpeed", NULL);
        if (value->type != JSON_TYPE_STRING) {
            printf("E %s: failed to get windSpeed, %d\n", progname, value->type);
            continue;
        }
        strcpy(wind_speed, value->u.string);

        value = util_json_get_value(period, "windDirection", NULL);
        if (value->type != JSON_TYPE_STRING) {
            printf("E %s: failed to get windDirection, %d\n", progname, value->type);
            continue;
        }
        strcpy(wind_dir, value->u.string);

        cnt = sscanf(wind_speed, "%d to %d mph", &low, &high);
        if (cnt == 1) {
            sprintf(tmp_str, "%d%s", low, wind_dir);
        } else if (cnt == 2) {
            sprintf(tmp_str, "%d-%d%s", low, high, wind_dir);
        } else {
            tmp_str[0] = '\0';
        }

        x->wind = strdup(tmp_str);

        // get precip probability
        value = util_json_get_value(period, "probabilityOfPrecipitation", "value", NULL);
        if (value->type != JSON_TYPE_NUMBER) {
            printf("E %s: failed to get probabilityOfPrecipitation, %d\n", progname, value->type);
            continue;
        }
        sprintf(tmp_str, "%.0f%%", value->u.number);
        x->precip = strdup(tmp_str);

        // set forecast item valid
        x->valid = true;

#if 0
        // debug print forecast info
        printf("I %s: %s periods[%s] ...\n", progname, json_filename, period_str);
        printf("I %s:   is_daytime        %d\n", progname, x->is_daytime);
        printf("I %s:   day_name          %s\n", progname, x->day_name);
        printf("I %s:   day_name_unabbrev %s\n", progname, x->day_name_unabbreviated);
        printf("I %s:   icon_url          %s\n", progname, x->icon_url);
        printf("I %s:   icon_filename     %s\n", progname, x->icon_filename);
        printf("I %s:   short_forecast    %s\n", progname, x->short_forecast);
        printf("I %s:   detailed_forecast %s\n", progname, x->detailed_forecast);
        printf("I %s:   temperature       %s\n", progname, x->temperature);
        printf("I %s:   wind              %s\n", progname, x->wind);
        printf("I %s:   precip            %s\n", progname, x->precip);
#endif
    }

    // download icons that have not already been dowloaded
    for (int i = 0; i < MAX_FORECAST; i++) {
        forecast_t *x = &forecast[i];
        char cmd[500];

        if (!x->valid) {
            continue;
        }

        if (!util_file_exists(icon_dir, x->icon_filename)) {
            sprintf(cmd, "curl --silent --max-time 10 --output %s/%s --header %s %s",
                    icon_dir, x->icon_filename, HEADER, x->icon_url);
            printf("I %s: %s\n", progname, cmd);
            system(cmd);
        }
    }

    // cleanup and return
    printf("I %s: parse_forecast %s completed, %0.3f secs\n", 
           progname, json_filename, 
           (util_microsec_timer() - start_us) / 1000000.0);
    util_json_free(json);
    free(str);
    return 0;
}

void create_icon_texture_if_needed(forecast_t *x)
{
    sdlx_texture_t *t;
    int w, h, rc;
    unsigned char *pixels = NULL;

    // if icon_texture already exists then return
    if (x->icon_texture) {
        return;
    }

    // if no icon_filename then print error and return
    if (x->icon_filename == NULL) {
        printf("E %s: no icon_filename\n", progname);
        return;
    }

    // if png read of the icon_file failed then print and return error
    rc = util_read_png_file(icon_dir, x->icon_filename, &pixels, &w, &h);
    if (rc != 0) {
        printf("E %s: failed to decode png file %s\n", progname, x->icon_filename);
        return;
    }

    // create the texture
    //printf("I %s: creating texture for %s\n", progname, x->icon_filename);
    t = sdlx_create_texture(w, h);
    if (t == NULL) {
        printf("E %s failed to create icon_texture\n", progname);
        free(pixels);
        return;
    }

    // copy the png file pixels in to the texture
    sdlx_set_texture_pixels(t, (sdlx_color_t*)pixels);

    // done with the png file pixels
    free(pixels);

    // save ptr to the created texture in x->icon_texture
    x->icon_texture = t;
}

// -----------------  DISPLAY FORECAST  ---------------------------

#define ICON_WH 200  // width and height of forecast icon
#define Y_STEP  250
#define MAX_SHORT_FORECAST_LINES 2

void display_forecast(void)
{
    int y2;
    sdlx_loc_t loc, dest;

    y2 = y;

    sdlx_render_printf_ex2(
            sdlx_win_width/2, y, 
            FONT_SMALL, COLOR_WHITE, FLAG_XY_CTR,
            "%s %s", info.city, info.state);
    y2 += sdlx_char_height_dflt;

    for (int i = 0; i < MAX_FORECAST; i++) {
        forecast_t *x = (mode == HOURLY ? &hourly[i] : &daily[i]);

        // if forecast entry is not valid then continue
        if (!x->valid) {
            continue;
        }        

        // skip nightime forecast when mode is DAILY
        if (mode == DAILY && !x->is_daytime) {
            continue;
        }

        // if y coord is below bottom then done displaying
        if (y2 > y_bottom - ICON_WH) {
            break;
        }

        // if y coord is above top then skip
        if (y2 < y_top - ICON_WH) {
            y2 += Y_STEP;
            continue;
        }

        // display the forecast icon
        create_icon_texture_if_needed(x);
        if (x->icon_texture) {
            dest.x = 0;
            dest.y = y2;
            dest.w = ICON_WH;
            dest.h = ICON_WH;
            sdlx_render_texture(x->icon_texture, NULL, &dest);

            loc.x = 0;
            loc.y = y2;
            loc.w = sdlx_win_width;
            loc.h = ICON_WH;
            sdlx_register_event(&loc, EVID_FORECAST+i);
        }

        // display forecast info ...
        // - day_name, temperature, wind, and precip probability
        sdlx_render_printf(ICON_WH, y2+2, 
                           "%s: %s %s %s", 
                           x->day_name, x->temperature, x->wind, x->precip);

        // - short_forecast
        int wrap = sdlx_win_width-ICON_WH;
        sdlx_render_printf_ex2(ICON_WH, y2+2+sdlx_char_height_dflt, 
                              FONT_SMALL, COLOR_WHITE, wrap,
                              "%s", x->short_forecast);

        // advance y
        y2 += Y_STEP;
    }
}

#define MAX_DETAILED_FORECAST_LINES 20
#define EVID_PREVIOUS 1
#define EVID_NEXT     2

void display_detailed_forecast(int idx)
{
    forecast_t  *x = (mode == HOURLY ? &hourly[idx] : &daily[idx]);
    sdlx_event_t event;
    int          wrap;
    sdlx_loc_t   dest;
    bool         done = false;

    while (!done) {
        // init the backbuffer
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // display the forecast icon
        create_icon_texture_if_needed(x);
        if (x->icon_texture) {
            dest.x = 0;
            dest.y = y_top;
            dest.w = ICON_WH;
            dest.h = ICON_WH;
            sdlx_render_texture(x->icon_texture, NULL, &dest);
        }

        // display forecast info ...
        // - day_name, temperature, wind, and precip probability
        sdlx_render_printf(ICON_WH, y_top+2, 
                           "%s: %s %s %s", 
                           x->day_name, x->temperature, x->wind, x->precip);

        // - short_forecast
        wrap = sdlx_win_width-ICON_WH;
        sdlx_render_printf_ex2(ICON_WH, y_top+2+sdlx_char_height_dflt, 
                               FONT_SMALL, COLOR_WHITE, wrap,
                               "%s", x->short_forecast);

        // - detailed_forecast
        wrap  = sdlx_win_width;
        sdlx_render_printf_ex2(0, y_top+ICON_WH+sdlx_char_height_dflt, 
                               FONT_SMALL, COLOR_WHITE, wrap,
                               "%s", x->detailed_forecast);

        // pass detailed_forecast to text_to_speech
        if (strlen(x->detailed_forecast) > 0) {
            util_text_to_speech(x->detailed_forecast);
        }

        // register EVID_SHOW_README_FILE event
        reg_event_show_readme_file();

        // register events
        sdlx_register_control_events(EVID_PREVIOUS, "^",
                                     EVID_NEXT, "v",
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with infinit timeout
        sdlx_get_event(-1, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_QUIT:
            util_text_to_speech_stop();
            done = true;
            break;
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_PREVIOUS: {
            forecast_t *fc = (mode == HOURLY ? &hourly[0] : &daily[0]);
            int i = idx-1;
            while (i >= 0 && (!fc[i].valid || (mode == DAILY && !fc[i].is_daytime))) {
                i--;
            }
            if (i >= 0) {
                idx = i;
                x = (mode == HOURLY ? &hourly[idx] : &daily[idx]);
            }
            break; }
        case EVID_NEXT: {
            forecast_t *fc = (mode == HOURLY ? &hourly[0] : &daily[0]);
            int i = idx+1;
            while (i < MAX_FORECAST && (!fc[i].valid || (mode == DAILY && !fc[i].is_daytime))) {
                i++;
            }
            if (i < MAX_FORECAST) {
                idx = i;
                x = (mode == HOURLY ? &hourly[idx] : &daily[idx]);
            }
            break; }
        }
    }
}

// -----------------  UTILS  ----------------------------------

#ifdef LINUX
extern char *strptime(const char *s, const char *format, struct tm *tm);
#endif

char *get_day_name(char * time_str, bool unabbreviated)
{
    time_t t;
    struct tm tm;
    char *fmt = (unabbreviated ? "%A" : "%a");
    static char day_name_str[20];

    memset(&tm, 0, sizeof(tm));
    if (strptime(time_str, "%Y-%m-%d", &tm) != NULL) {
        t = mktime(&tm);
        if (t > 0) {
            strftime(day_name_str, sizeof(day_name_str), fmt, &tm);
            return day_name_str;
        } else {
            return "Error";
        }
    }

    return "Error";
}
