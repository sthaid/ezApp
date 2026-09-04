#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>

#include <sdlx.h>
#include <utils.h>

#include "svcs/Location/location.h"
#include "svcs/Location/common.h"

#define DEG2RAD (M_PI / 180)

typedef struct {
    double latitude;
    double longitude;
    char   name[MAX_NAME];
    char   state[MAX_NAME];
} loc_data_t;

static loc_data_t *loc_data;
static int         max_loc_data;

// -----------------  READ LOC DATA  --------------------------------

int read_loc_data(void)
{
    char cmd[200];
    int  len;

    // if loc data had previously been initialized, 
    // this call will free the prior loc_data and set max_loc_data to 0
    free_loc_data();

    // catenate the *.loc files
    sprintf(cmd, "cat %s/*.loc > %s/all_loc", data_dir, data_dir);
    system(cmd);

    // read the catenated file to variable 'loc_data'
    loc_data = util_read_file(data_dir, "all_loc", &len);
    if (loc_data == NULL) {
        printf("E %s: failed to read all_loc\n", progname);
        return -1;
    }

    // delete all_loc file
    util_delete_file(data_dir, "all_loc");

    // set max_loc_data
    if ((len % sizeof(loc_data_t)) != 0) {
        printf("E %s: invalid len %d of file all_loc\n", progname, len);
        free_loc_data();
        return -1;
    }
    max_loc_data = len / sizeof(loc_data_t);
    printf("I %s: max_loc_data = %d\n", progname, max_loc_data);

    // return success
    return 0;
}

void free_loc_data(void)
{
    free(loc_data);
    loc_data = NULL;
    max_loc_data = 0;
}

// -----------------  FIND CLOSEST LOC DATA  ------------------------

// There are approximately 364,000 feet (69 miles) in one degree of latitude
//
// The number of feet in one degree of longitude varies based on your latitude,
// decreasing from approximately 364,000 feet (69 miles) at the equator to zero
// at the poles. For a specific location, you can calculate this distance by
// multiplying the distance at the equator by the cosine of your latitude

void find_closest_loc_data(
            double req_latitude, double req_longitude, 
            char *name, char *state,
            double *actual_latitude, double *actual_longitude)
{
    double cos_req_lat, point5_div_cos_req_lat;

    char dummy_name[50];
    char dummy_state[50];
    double dummy_actual_latitude;
    double dummy_actual_longitude;

    static double save_req_latitude;
    static double save_req_longitude;
    static char   save_actual_name[MAX_NAME];
    static char   save_actual_state[MAX_NAME];
    static double save_actual_latitude;
    static double save_actual_longitude;

    // if arg not supplied then use dummy value
    if (!name) name = dummy_name;
    if (!state) state = dummy_state;
    if (!actual_latitude) actual_latitude = &dummy_actual_latitude;
    if (!actual_longitude) actual_longitude = &dummy_actual_longitude;

    // init
    cos_req_lat = cos(req_latitude * DEG2RAD);
    point5_div_cos_req_lat = 0.5 / cos_req_lat;

    // preset return values to invalid
    memset(name, 0, sizeof(MAX_NAME));
    memset(state, 0, sizeof(MAX_NAME));
    *actual_latitude = INVALID_NUMBER;
    *actual_longitude = INVALID_NUMBER;

    // if latitude or longitude are invalid then return 
    if (req_latitude == INVALID_NUMBER || req_longitude == INVALID_NUMBER) {
        return;
    }

    // if requested latitude/longitude is within 0.25 miles of saved result then
    // return the saved result; note: ns, ew are in miles
    if (save_actual_name[0] != '\0') {
        double ns = (req_latitude - save_req_latitude) * 69.0;
        double ew = ((req_longitude - save_req_longitude) * cos_req_lat) * 69.0;
        double distance_squared = (ns * ns) + (ew * ew);
        if (distance_squared < 0.0625) {
            strcpy(name, save_actual_name);
            strcpy(state, save_actual_state);
            *actual_latitude = save_actual_latitude;
            *actual_longitude = save_actual_longitude;
            printf("I %s: returning saved location %s %s %0.4f %0.4f\n", 
                   progname, save_actual_name, save_actual_state, 
                   save_actual_latitude, save_actual_longitude);
            return;
        }
    }

    // loop over all locations, and find the closest
    double min_distance_squared = 1e99;
    loc_data_t *min_loc_data = NULL;
    for (int i = 0; i < max_loc_data; i++) {
        loc_data_t *x = &loc_data[i];
        double delta_lat, delta_long, ns, ew, distance_squared;

        delta_lat = fabs(req_latitude - x->latitude);
        if (delta_lat > 0.5) {
            continue;
        }

        delta_long = fabs(req_longitude - x->longitude);
        if (delta_long > 350) {
            delta_long = 360 - delta_long;
        }
        if (delta_long > point5_div_cos_req_lat) {
            continue;
        }

        ns = delta_lat;
        ew = delta_long * cos_req_lat;
        distance_squared = (ns * ns) + (ew * ew);

        if (distance_squared < min_distance_squared) {
            min_loc_data = x;
            min_distance_squared = distance_squared;
        }
    }

    // if no closest location found then return
    if (min_loc_data == NULL) {
        printf("I %s: closest not found for %0.4f %0.4f\n", progname, req_latitude, req_longitude);
        return;
    }
        
    // return name, state and distance of the closest location
    strncpy(name, min_loc_data->name, MAX_NAME-1);
    strncpy(state, min_loc_data->state, MAX_NAME-1);
    *actual_latitude = min_loc_data->latitude;
    *actual_longitude = min_loc_data->longitude;
    printf("I %s: found closest to %0.4f %0.4f - name=%s %s %0.4f %0.4f\n",
           progname, req_latitude, req_longitude, 
           name, state, *actual_latitude, *actual_longitude);

    // save result, so a subsequent call can use the result if the
    // subsequent call lat/long is close to the saved lat/long
    save_req_latitude = req_latitude;
    save_req_longitude = req_longitude;
    strcpy(save_actual_name, name);
    strcpy(save_actual_state, state);
    save_actual_latitude = *actual_latitude;
    save_actual_longitude = *actual_longitude;
}

// -----------------  COUNTRY LOC DATA DOWNLOAD  --------------------

int read_and_parse_json_file(char *json_filename, FILE *fp_out);

int download_country_loc_data(char *id_arg)
{
    int ret = -1;
    char cmd[200], json_filename[100], out_filename[100], zip_filename[100], id[3];
    FILE *fp_out = NULL;

    // verify 
    strncpy(id, id_arg, sizeof(id));
    id[sizeof(id)-1] = '\0';
    if (strlen(id) != 2 || !islower(id[0]) || !islower(id[1])) {
        printf("E %s: invalid 2 char country id code '%s'\n", progname, id);
        return ret;
    }

    // init
    sprintf(zip_filename, "%s.zip", id);

    // create output file
    sprintf(out_filename, "%s/%s.loc", data_dir, id);
    fp_out = fopen(out_filename, "wb");
    if (fp_out == NULL) {
        printf("E %s: failed to create %s\n", progname, out_filename);
        goto done;
    }

    // download zip file containing city/town location and names
    util_delete_file(data_dir, zip_filename);
    sprintf(cmd, "curl --silent --max-time 10 --output %s/%s.zip https://www.geoapify.com/data-share/localities/%s.zip",
            data_dir, id, id);
    ret = system(cmd);
    if (ret != 0) {
        printf("E %s: '%s' failed, ret=%d, %s\n", progname, cmd, ret, strerror(errno));
        goto done;
    }
    
    // unzip
    sprintf(cmd, "unzip -o -d %s %s/%s.zip", data_dir, data_dir, id);
    ret = system(cmd);
    if (ret != 0) {
        printf("E %s: '%s' failed, ret=%d, %s\n", progname, cmd, ret, strerror(errno));
        goto done;
    }

    // parse the json files to obtain city/town/village names and latitude/longitude;
    // these will be written to the file associated with 'fp_out'
    sprintf(json_filename, "%s/%s/%s", data_dir, id, "place_city.ndjson");
    read_and_parse_json_file(json_filename, fp_out);

    sprintf(json_filename, "%s/%s/%s", data_dir, id, "place-town.ndjson");
    read_and_parse_json_file(json_filename, fp_out);

    sprintf(json_filename, "%s/%s/%s", data_dir, id, "place-village.ndjson");
    read_and_parse_json_file(json_filename, fp_out);

    // set ret to success
    ret = 0;

done:
    // cleanup
    if (strcmp(data_dir, "svcs/Location") == 0 && strlen(id) == 2) {
        util_delete_file(data_dir, zip_filename);
        util_delete_dir(data_dir, id);
    }
    if (fp_out) {
        fclose(fp_out);
    }

    // return status
    return ret;
}

int read_and_parse_json_file(char *json_filename, FILE *fp_out)
{
    char         *end_ptr;
    void         *root = NULL;
    char         *str = NULL, *str_orig = NULL;
    int           len, success_cnt=0, skip_cnt=0;
    json_value_t  name, latitude, longitude, state;

    // read json into str_orig
    str_orig = util_read_file(".", json_filename, &len);
    if (str_orig == NULL) {
        printf("E %s: failed read file %s\n", progname, json_filename);
        goto error;
    }

    // parse json
    str = str_orig;
    while (true) {
        // parse json
        root = util_json_parse(str, &end_ptr);
        if (root == NULL) {
            printf("E %s: util_json_parse failed, %s\n", progname, json_filename);
            goto error;
        }

        // extract json fields
        name         = *util_json_get_value(root, "name", NULL);
        state        = *util_json_get_value(root, "address", "state", NULL);
        longitude    = *util_json_get_value(root, "location", "0", NULL);
        latitude     = *util_json_get_value(root, "location", "1", NULL);

        // if fields extracted okay them save info, else skip
        if (name.type == JSON_TYPE_STRING &&
            latitude.type == JSON_TYPE_NUMBER &&
            longitude.type == JSON_TYPE_NUMBER)
        {
            loc_data_t x;
            x.latitude = latitude.u.number;
            x.longitude = longitude.u.number;
            strncpy(x.name, name.u.string, MAX_NAME);
            x.name[MAX_NAME-1] = '\0';
            if (state.type == JSON_TYPE_STRING) {
                strncpy(x.state, state.u.string, MAX_NAME);
                x.state[MAX_NAME-1] = '\0';
            } else {
                x.state[0] = '\0';
            }

            fwrite(&x, sizeof(loc_data_t), 1, fp_out);
            //printf("I %s: %32s %32s %0.4f %0.4f\n", progname, x.name, x.state, x.latitude, x.longitude);
            success_cnt++;
        } else {
            skip_cnt++;
        }

        // free the parsed json
        util_json_free(root);
        root = NULL;

        // advance str to the next json object
        str = end_ptr;
        while (*str != '{' && *str != '\0') {
            str++;
        }

        // if no more json objects then break
        if (*str == '\0') {
            break;
        }
    }

    // cleanup and return success
    printf("I %s: read_and_parse_json_file %s success_cnt=%d skip_cnt=%d\n", 
           progname, json_filename, success_cnt, skip_cnt);
    free(str_orig);
    util_json_free(root);
    return 0;

    // error return
error:
    free(str_orig);
    util_json_free(root);
    return -1;
}
