// xxx
// - rename util_take_picture ?  to util_take_photo
// - replace 'TAKE' with a circle
// - cleanup needed?
// - define for 300
// - allow for metadata location to be INVALID_NUMBER
// - allow for city and state to be empty
// - add click when photo taken,  disable via settings
// - handle favorite setting

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"
#include "svcs/Location/location.h"

// defines
#define MAX_PHOTOS    1000
#define GALLERY_VIEW  0
#define LOCATION_VIEW 1

#define EVID_TAKE           1
#define EVID_DEL            2
#define EVID_FAV            3
#define EVID_VIEW           4
#define EVID_STG            5
#define EVID_CTR            6
#define EVID_RST            7
#define EVID_SHOW_PHOTO     10000
#define EVID_DELETE_PHOTO   20000

#define ONE_SEC 1000000

// typedefs
typedef struct {
    bool favorite;
    char date[50];
    char time[50];
    char city[50];
    char state[50];
    double latitude;
    double longitude;
    unsigned int pixels[300*300];
} metadata_t;

typedef struct {
    int num;
    metadata_t *md;
} photo_t;

// variables
char *progname;
char *data_dir;
bool  end_program;

int   view = GALLERY_VIEW;

char  photos_dir[100];
photo_t   photos[MAX_PHOTOS];
int   max_photos;
    
// prototypes
void init(void);
void cleanup(void);

void photo_gallery_view(void);
void photo_location_view(void);

void take_photo(void); // xxx maybe return idx
void delete_photo(int idx);
void show_photo(int idx);

void settings(void);

unsigned int *jpeg_file_to_rgba_pixels(char *dir, char *file, int *w, int *h);
unsigned int *jpeg_file_to_rgba_pixels_scaled(char *dir, char *file, int w, int h);
metadata_t *create_and_map_metadata_file(int num);

// -----------------  MAIN  ------------------------------------------
    
int main(int argc, char **argv)
{
    // verify arg count
    if (argc != 2) {
        printf("E %s: argc=%d is not 2\n", "Camera", argc);
        return 1;
    }

    // save args
    progname = argv[0];
    data_dir = argv[1];
    printf("I %s: starting, data_dir=%s\n", progname, data_dir);

    // initialize
    init();

    // runtime
    while (!end_program) {
        switch (view) {
        case GALLERY_VIEW:
            photo_gallery_view();
            break;
        case LOCATION_VIEW:
            photo_location_view();
            break;
        default:
            printf("E %s: invalid view %d\n", progname, view);
            end_program = 1;
        }
    }

    // cleanup and terminate
    cleanup();
    printf("I %s: terminating\n", progname);
    return 0;
}

void init(void)
{
    int         num, cnt;
    char        cmd[200], s[100], metadata_filename[100];
    FILE       *fp;
    metadata_t *md;

    // init global variable photos_dir
    sprintf(photos_dir, "%s/photos", data_dir);

    // initialize the photos array using sorted list of jpg 
    // files that are in the photos dir
    // xxx check for overflow
    // xxx time this startup with 1000 files
    sprintf(cmd, "find %s -type f -name \"*.jpg\" | sort", photos_dir);
    fp = popen(cmd, "r");
    while (fgets(s, sizeof(s), fp) != NULL) {
        // extract the photo number from the pathname strings provided by the find cmd
        cnt = sscanf(s, "apps/Camera/photos/%d.jpg", &num);
        if (cnt != 1) {
            printf("E %s: failed to extract photo num from '%s'\n", progname, s);
            continue;
        }

        // xxx get the metadata when needed
        // map the metadata file for this photo number
        sprintf(metadata_filename, "%06d.meta", num);
        md = util_map_file(photos_dir, metadata_filename, sizeof(metadata_t), true, NULL);
        if (md == NULL) {
            printf("E %s: failed to mmap %s, skipping photo %d\n", progname, metadata_filename, num);
            continue;
        }

        // add entry to photos array
        photos[max_photos].num = num;
        photos[max_photos].md = md;
        max_photos++;
    }
    pclose(fp);

#if 0
    // debug print the photos list
    printf("I %s: max_photos = %d\n", progname, max_photos);
    for (int i = 0; i < max_photos; i++) {
        printf("I %s: photo num = %d  md = %p\n", progname, photos[i].num, photos[i].md);
    }
#endif
}

void cleanup(void)
{
    // unmap the metadata files
    for (int i = 0; i < max_photos; i++) {
        if (photos[i].md == NULL) {
            printf("E %s: photos[%d].md is NULL\n", progname, i);
            continue;
        }
        util_unmap_file(photos[i].md, sizeof(metadata_t));
        photos[i].md = NULL;
    }
}

// ------------------ PHOTO GALLERY VIEW ---------------

void photo_gallery_view(void)
{
    sdlx_event_t event;
    int x, y;
    bool done = false;
    bool del_mode = false;
    bool fav_mode = false;
    sdlx_texture_t *t;
    sdlx_loc_t dest;

    t = sdlx_create_texture(300, 300);

    while (!done && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // xxx display, todo
        for (int i = 0; i < max_photos; i++) {
            sdlx_set_texture_pixels(t, photos[i].md->pixels);
            dest.x = (i % 3) * 350;
            dest.y = (i / 3) * 350;
            dest.w = 300;
            dest.h = 300;
            sdlx_render_texture(t, NULL, &dest);

            sdlx_register_event(&dest, EVID_SHOW_PHOTO+i);

            if (del_mode) {
                x = dest.x + 300 - sdlx_char_width_dflt;
                y = dest.y;
                reg_event(x, y, COLOR_RED, "X", EVID_DELETE_PHOTO+i);
            }
        }

        // register events
        if (!del_mode) {
            reg_event_show_readme_file();
        }
        y = sdlx_win_height - 1.5 * sdlx_char_height_dflt;
        reg_event(0, y, COLOR_LIGHT_BLUE, "DEL", EVID_DEL);
        reg_event(sdlx_win_width/2-1.5*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "FAV", EVID_FAV);
        reg_event(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "VIEW", EVID_VIEW);
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_STG, "STG", EVID_TAKE, "TAKE", EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for an event, with timeout
        sdlx_get_event(ONE_SEC, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        if (event.event_id >= EVID_DELETE_PHOTO && event.event_id < EVID_DELETE_PHOTO+max_photos) {
            int idx = event.event_id - EVID_DELETE_PHOTO;
            delete_photo(idx);
        } else if (event.event_id >= EVID_SHOW_PHOTO && event.event_id < EVID_SHOW_PHOTO+max_photos) {
            int idx = event.event_id - EVID_SHOW_PHOTO;
            show_photo(idx);
        } else {
            switch (event.event_id) {
            case EVID_SHOW_README_FILE:
                show_file(data_dir, "README");
                break;
            case EVID_MOTION:
                break;
            case EVID_QUIT:
                end_program = true;
                break;
            case EVID_TAKE:
                take_photo();
                break;
            case EVID_DEL:
                del_mode = !del_mode;
                break;
            case EVID_FAV:
                fav_mode = !fav_mode;
                break;
            case EVID_VIEW:
                view = LOCATION_VIEW;
                done = true;
                break;
            case EVID_STG:
                settings();
                break;
            }
        }
    }

    sdlx_destroy_texture(t);
}

// ------------------ PHOTO LOCATION VIEW --------------

void photo_location_view(void)
{
    sdlx_event_t event;
    int y;
    bool done = false;
    bool favorites_mode = false;

    while (!done && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // register events xxx add pinch
        reg_event_show_readme_file();
        y = sdlx_win_height - 1.5 * sdlx_char_height_dflt;
        reg_event(0, y, COLOR_LIGHT_BLUE, "CTR", EVID_CTR);
        reg_event(sdlx_win_width/2-1.5*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "FAV", EVID_FAV);
        reg_event(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "VIEW", EVID_VIEW);
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_STG, "STG", EVID_TAKE, "TAKE", EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with timeout
        sdlx_get_event(ONE_SEC, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_STG:
            settings();
            break;
        case EVID_TAKE:
            take_photo();
            break;
        case EVID_QUIT:
            end_program = true;
            break;
        case EVID_CTR:
            break;
        case EVID_FAV:
            favorites_mode = !favorites_mode;
            break;
        case EVID_VIEW:
            view = GALLERY_VIEW;
            done = true;
            break;
        case EVID_MOTION:
            break;
        }
    }
}

// ------------------ TAKE PHOTO -----------------------

void take_photo(void)
{
    int rc, num;
    char photo_filename[100];
    metadata_t *md;

    // take photo; this will create file tmp/photo.jpg
    rc = util_take_picture();  
    if (rc != 0) {
        printf("E %s: util_take_picture failed\n", progname);
        sdlx_show_toast("take picture failed");
        return;
    }

    // determine next photo num
    if (max_photos == 0) {
        num = 1;
    } else {
        num = photos[max_photos-1].num + 1;
    }

    // move photo from tmp/photo.jpg to photos subdir
    sprintf(photo_filename, "%06d.jpg", num);
    printf("I %s: creating %s\n", progname, photo_filename);
    util_rename_file("tmp", "photo.jpg", photos_dir, photo_filename);

    // create and map metadata file
    md = create_and_map_metadata_file(num);
    if (md == NULL) {
        printf("E %s: create_and_map_metadata_file(%d) failed\n", progname, num);
        util_delete_file(photos_dir, photo_filename);
        return;
    }
    
    // add photo to list
    photos[max_photos].num = num;
    photos[max_photos].md = md;
    max_photos++;
}

// ------------------ DELETE PHOTO ---------------------

void delete_photo(int idx)
{
    int num;
    char photo_filename[100], metadata_filename[100];

    // check idx arg
    if (idx < 0 || idx >= max_photos) {
        printf("E %s: idx %d out of range 0..%d\n", progname, idx, max_photos);
        return;
    }
    num = photos[idx].num;
    if (num <= 0) {
        printf("E %s: invalid num %d\n", progname, num);
        return;
    }

    // unmap the metadata
    if (photos[idx].md != NULL) {
        util_unmap_file(photos[idx].md, sizeof(metadata_t));
        photos[idx].md = NULL;
    } else {
        printf("E %s: photos[%d].md is NULL, continuing with delete_photo\n", 
              progname, idx);
    }

    // remove the entry from the photos list
    memmove(&photos[idx], &photos[idx+1], (max_photos-idx-1) * sizeof(photo_t));
    max_photos--;

    // delete photo jpg and meta files
    sprintf(photo_filename, "%06d.jpg", num);
    sprintf(metadata_filename, "%06d.meta", num);
    util_delete_file(photos_dir, photo_filename);
    util_delete_file(photos_dir, metadata_filename);

    // debug print
    printf("I %s: deleted %s\n", progname, photo_filename);
}

// ------------------ SHOW PHOTO -----------------------

void show_photo(int idx)
{
    int             num, w, h;
    char            file[50];
    metadata_t     *md;
    sdlx_texture_t *t;
    sdlx_loc_t      dest;
    sdlx_event_t    event;
    unsigned int   *pixels;
    bool            done = false;

    // check idx arg
    if (idx < 0 || idx >= max_photos) {
        printf("E %s: idx %d out of range 0..%d\n", progname, idx, max_photos);
        return;
    }
    num = photos[idx].num;
    if (num <= 0) {
        printf("E %s: invalid num %d\n", progname, num);
        return;
    }
    md = photos[idx].md;
    if (md == NULL) {
        printf("E %s: photos[%d].md is NULL\n", progname, idx);
        return;
    }
        
    // construct photo filename
    sprintf(file, "%06d.jpg", num);
    printf("I %s: show photo %s\n", progname, file);

    // create texture xxx check pixels return
    pixels = jpeg_file_to_rgba_pixels(photos_dir, file, &w, &h);
    t = sdlx_create_texture(w, h);
    sdlx_set_texture_pixels(t, pixels);
    free(pixels);

    while (!done) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // render the photo texture
        // xxx handle pan and zoom
        dest.x = 0;
        dest.y = 0;
        dest.w = 1000;
        dest.h = 1333;
        sdlx_render_texture(t, NULL, &dest);

        // display metadata
        // xxx todo

        // register events
        sdlx_register_event(NULL, EVID_PINCH);
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_RST, "RST", EVID_TAKE, "TAKE", EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for an event, with timeout
        sdlx_get_event(ONE_SEC, &event);
        if (event.event_id == -1) {
            continue;
        }

        switch (event.event_id) {
        case EVID_QUIT:
            done = true;
            break;
        case EVID_MOTION:
            //printf("xxxx motion\n");
            break;
        case EVID_PINCH:
            //printf("xxxx pinch\n");
            break;
        case EVID_TAKE:
            take_photo();
            // update idx to show this photo
            break;
        case EVID_RST:
            // reset pan and zoom
            break;
        }
    }

    sdlx_destroy_texture(t);
}

// ------------------ SETTINGS -------------------------

void settings(void)
{
    // xxx todo
    return;
}

// ------------------ UTILS ----------------------------

unsigned int *jpeg_file_to_rgba_pixels(char *dir, char *file, int *w_arg, int *h_arg)
{
    int           rc, w, h;
    unsigned int *pixels;

    rc = util_decode_jpeg_to_raw(dir, file, &w, &h, &pixels);
    if (rc != 0) {
        printf("E %s: util_decode_jpeg_to_raw failed %s/%s, rc=%d\n", 
               progname, dir, file, rc);
        return NULL;
    }
    printf("I %s: decode JPEG okay, w/h=%d,%d\n", progname, w, h);

    *w_arg = w;
    *h_arg = h;
    return pixels;
}

unsigned int *jpeg_file_to_rgba_pixels_scaled(char *dir, char *file, int w, int h)
{
    int jpeg_w, jpeg_h, rc;
    unsigned int *pixels1, *pixels2;
    sdlx_texture_t *t1, *t2;

    // decode the jpg photo file to pixels1
    rc = util_decode_jpeg_to_raw(dir, file, &jpeg_w, &jpeg_h, &pixels1);
    if (rc != 0) {
        printf("E %s: util_decode_jpeg_to_raw failed %s/%s, rc=%d\n", 
               progname, dir, file, rc);
        return NULL;
    }
    printf("I %s: jpeg wXh = %d %d\n", progname, jpeg_w, jpeg_h);

    // scale the jpeg image pixels (pixels1) to dimension wXh;
    // result is in pixels2
    t1 = sdlx_create_texture(jpeg_w, jpeg_h);
    t2 = sdlx_create_texture(w, h);
    sdlx_set_texture_pixels(t1, pixels1);
    sdlx_set_render_target(t2);
    sdlx_render_texture(t1, NULL, NULL);
    pixels2 = sdlx_get_texture_pixels(t2, NULL, NULL);

    // cleanup
    sdlx_set_render_target(NULL);
    sdlx_destroy_texture(t1);
    sdlx_destroy_texture(t2);
    free(pixels1);

    // return pixels, caller must free
    return pixels2;
}

metadata_t *create_and_map_metadata_file(int num)
{
    char        photo_filename[100];
    char        metadata_filename[100];
    metadata_t *md;
    time_t      t;
    struct tm  *tm;
    int         rc;
    char        req_data[MAX_SVC_REQ_DATA];
    svc_req_t  *req;
    unsigned int *pixels;

    // create and map zero filled metadata file
    sprintf(metadata_filename, "%06d.meta", num);
    util_delete_file(photos_dir, metadata_filename);
    md = util_map_file(photos_dir, metadata_filename, sizeof(metadata_t), true, NULL);
    if (md == NULL) {
        printf("E %s: failed to create and map file %s/%s, %s\n", 
               progname, photos_dir, metadata_filename, strerror(errno));
        util_delete_file(photos_dir, metadata_filename);
        return NULL;
    }

    // md should already be zero, zero it here, to be certain
    memset(md, 0, sizeof(metadata_t));

    // init metadata struct fields ...

    // - favorite
    md->favorite = false;

    // - date & time
    t = time(NULL);
    tm = localtime(&t);
    strftime(md->date, sizeof(md->date), "%a %b %d %Y", tm);
    strftime(md->time, sizeof(md->time), "%H:%M %Z", tm);

    // - latitude & longitude
    util_get_location(&md->latitude, &md->longitude, NULL, NULL);

    // - city & state
    memset(req_data, 0, sizeof(req_data));
    *(double*)(&req_data[0]) = md->latitude;
    *(double*)(&req_data[8]) = md->longitude;
    req = svc_req_init(SVC_LOCATION_REQ_GET_LOC_NAME_FROM_LAT_LONG, req_data, sizeof(req_data));
    rc = svc_make_req("Location", req, 5);
    if (rc != 0) {
        strcpy(md->city, "Unknown");
    } else {
        // expected response format: <city>\n<state>\n\0
        // either city or state can be empty strings, or can contain space chars
        char *newline, *city, *state;

        // for safety
        req_data[MAX_SVC_REQ_DATA-3] = '\n';
        req_data[MAX_SVC_REQ_DATA-2] = '\n';
        req_data[MAX_SVC_REQ_DATA-1] = '\0';

        // copy city and state from req_data to metadata
        city = req->data;
        newline = strchr(city, '\n'); *newline = '\0';
        state = newline + 1;
        newline = strchr(state, '\n'); *newline = '\0';
        strncpy(md->city, city, sizeof(md->city)-1);
        strncpy(md->state, state, sizeof(md->state)-1);

        // if city and state are both empty then set city to Unknown
        if (md->city[0] == '\0' && md->state[0] == '\0') {
            strcpy(md->city, "Unknown");
        }
    }

    // - pixels
    sprintf(photo_filename, "%06d.jpg", num);
    pixels = jpeg_file_to_rgba_pixels_scaled(photos_dir, photo_filename, 300, 300);
    if (pixels == NULL) {
        util_unmap_file(md, sizeof(metadata_t));
        util_delete_file(photos_dir, metadata_filename);
        return NULL;
    }
    memcpy(md->pixels, pixels, 4*300*300);
    free(pixels);

    // sync the metadata file to storage
    util_sync_file(md, sizeof(metadata_t));

    // debug print metadata
    printf("I %s: metadata ...\n", progname);
    printf("I %s:   favorite   = %d\n",    progname, md->favorite);
    printf("I %s:   date       = %s\n",    progname, md->date);
    printf("I %s:   time       = %s\n",    progname, md->time);
    printf("I %s:   city       = %s\n",    progname, md->city);
    printf("I %s:   state      = %s\n",    progname, md->state);
    printf("I %s:   latitude   = %0.4f\n", progname, md->latitude);
    printf("I %s:   longitude  = %0.4f\n", progname, md->longitude);
    printf("I %s:   pixels     = %p\n",    progname, md->pixels);

    // return mapped ptr to the new metadata file
    return md;
}
