// xxx
// - rename util_take_picture ?  to util_take_photo
// - replace 'TAKE' with a circle
// - cleanup needed?
// - define for 300

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <sdlx.h>
#include <utils.h>

#include "lib/lib.h"

// defines
#define MAX_PHOTOS    1000
#define GALLERY_VIEW  0
#define LOCATION_VIEW 1

// typedefs
typedef struct {
    char date[50];
    char time[50];
    char city[50];
    double latitude;
    double longitude;
    bool favorite;
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
void photo_gallery(void);
void photo_location(void);
void settings(void);
metadata_t *create_and_map_metadata_file(int num);
void photo_take(void);
void photo_delete(int num);

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
            photo_gallery();
            break;
        case LOCATION_VIEW:
            photo_location();
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
    char        cmd[100], s[100], metadata_filename[100];
    FILE       *fp;
    metadata_t *md;

    // init global variable photos_dir
    sprintf(photos_dir, "%s/photos", data_dir);

    // initialize the photos array using sorted list of jpg 
    // files that are in the photos dir
    sprintf(cmd, "find %s -type f -name \"*.jpg\" | sort", photos_dir);
    fp = popen(cmd, "r");
    while (fgets(s, sizeof(s), fp) != NULL) {
        // extract the photo number from the pathname strings provided by the find cmd
        cnt = sscanf(s, "apps/Camera/photos/%d.jpg", &num);
        if (cnt != 1) {
            printf("E %s: failed to extract photo num from '%s'\n", progname, s);
            continue;
        }

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

// ------------------ PHOTO GALLERY --------------------

#define EVID_DEL   1
#define EVID_FAV   2
#define EVID_VIEW  3
#define EVID_STG   4
#define EVID_TAKE  5

void photo_gallery(void)
{
    sdlx_event_t event;
    int y;
    bool done = false;
    bool del_mode = false;
    bool favorites_mode = false;
    sdlx_texture_t *t;
    sdlx_loc_t dest;

    t = sdlx_create_texture(300, 300);

    while (!done && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // xxx display, todo
        for (int i = 0; i < max_photos; i++) {
            sdlx_set_texture_pixels(t, photos[i].md->pixels);
            dest.x = 0;
            dest.y = 0;
            dest.w = 300;
            dest.h = 300;
            sdlx_render_texture(t, NULL, &dest);
        }

        // register events
        reg_event_show_readme_file();
        y = sdlx_win_height - 1.5 * sdlx_char_height_dflt;
        reg_event(0, y, COLOR_LIGHT_BLUE, "DEL", EVID_DEL);
        reg_event(sdlx_win_width/2-1.5*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "FAV", EVID_FAV);
        reg_event(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "VIEW", EVID_VIEW);
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_STG, "STG",
                                     EVID_TAKE, "TAKE",
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for an event, with infinite timeout
        sdlx_get_event(-1, &event);
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
            photo_take();
            break;
        case EVID_QUIT:
            end_program = true;
            break;

        case EVID_DEL:
            del_mode = !del_mode;
            break;
        case EVID_FAV:
            favorites_mode = !favorites_mode;
            break;
        case EVID_VIEW:
            view = LOCATION_VIEW;
            done = true;
            break;

        case EVID_MOTION:
            break;
        }
    }
}

// ------------------ PHOTO LOCATION -------------------

#define EVID_CTR   6

void photo_location(void)
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
        y = sdlx_win_height - sdlx_char_height_dflt;
        reg_event(0, y, COLOR_LIGHT_BLUE, "CTR", EVID_CTR);
        reg_event(sdlx_win_width/2-1.5*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "FAV", EVID_FAV);
        reg_event(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "VIEW", EVID_VIEW);
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_STG, "STG",
                                     EVID_TAKE, "TAKE",
                                     EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with infinite timeout
        sdlx_get_event(-1, &event);
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
            photo_take();
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

// ------------------ SETTINGS -------------------------

void settings(void)
{
    // xxx todo
    return;
}

// ------------------ SUPPORT --------------------------

void photo_take(void)
{
    int rc, num;
    char photo_filename[100];
    metadata_t *md;

    // take photo; this will create file tmp/photo.jpg
    rc = util_take_picture();  
    if (rc != 0) {
        printf("E %s: util_take_picture failed\n", progname);
        // xxx maybe show_toast
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

metadata_t *create_and_map_metadata_file(int num)
{
    char        photo_file_pathname[100];
    char        metadata_filename[100];
    metadata_t *md;

    // create and map zero filled metadata file
    sprintf(metadata_filename, "%06d.meta", num);
    util_delete_file(photos_dir, metadata_filename);
    md = util_map_file(photos_dir, metadata_filename, sizeof(metadata_t), true, NULL);
    if (md == NULL) {
        printf("E %s: failed to map file %s/%s, %s\n", progname, photos_dir, metadata_filename, strerror(errno));
        util_delete_file(photos_dir, metadata_filename);
        return NULL;
    }

    // init metadata struct fields, except for pixels field
    // xxx todo
    strcpy(md->date, "Aug 22, 2026");
    strcpy(md->time, "11:00:00");
    strcpy(md->city, "Bolton");
    md->latitude  = 0;
    md->longitude = 0;
    md->favorite = false;

    // init the metadata struct pixels
    int fd, rc, w, h, jpeg_w, jpeg_h;
    unsigned int *pixels1, *pixels2;
    sdlx_texture_t *t1, *t2;
    // - open the jpg file
    sprintf(photo_file_pathname, "%s/photos/%06d.jpg", data_dir, num);
    fd = open(photo_file_pathname, O_RDONLY, 0);
    if (fd < 0) {
        printf("E %s: failed to open %s, %s\n", progname, photo_file_pathname, strerror(errno));
        util_unmap_file(md, sizeof(metadata_t));
        util_delete_file(photos_dir, metadata_filename);
        return NULL;
    }
    // - decode the jpg photo file to pixels1
    rc = util_decode_jpeg_to_raw(fd, &jpeg_w, &jpeg_h, &pixels1);
    if (rc != 0) {
        printf("E %s: util_decode_jpeg_to_raw failed, rc=%d\n", progname, rc);
        close(fd);
        util_unmap_file(md, sizeof(metadata_t));
        util_delete_file(photos_dir, metadata_filename);
        return NULL;
    }
    close(fd);
    printf("I %s: jpeg wXh = %d %d\n", progname, jpeg_w, jpeg_h);
    // - scale the jpeg image pixels (pixels1) to 300x300 pixels2
    t1 = sdlx_create_texture(jpeg_w, jpeg_h);
    t2 = sdlx_create_texture(300, 300);
    sdlx_set_texture_pixels(t1, pixels1);
    sdlx_set_render_target(t2);
    sdlx_render_texture(t1, NULL, NULL);
    pixels2 = sdlx_get_texture_pixels(t2, &w, &h);
    // xxx verify w and h
    // - copy the 300x300 pixels2 to md->pixels
    memcpy(md->pixels, pixels2, 4*300*300);
    // - sync the metadata file to storage
    util_sync_file(md, sizeof(metadata_t));
    // - cleanup    
    sdlx_set_render_target(NULL);
    sdlx_destroy_texture(t1);
    sdlx_destroy_texture(t2);
    free(pixels1);
    free(pixels2);

    // return mapped ptr to the new metadata file
    return md;
}
    
void photo_delete(int num)
{
    int i;
    char photo_filename[100], metadata_filename[100];

    // construct photo and metadata filenames
    sprintf(photo_filename, "%06d.jpg", num);
    sprintf(metadata_filename, "%06d.meta", num);

    // search list of photos for num
    for (i = 0; i < max_photos; i++) {
        if (photos[i].num == num) {
            break;
        }
    }
    if (i == max_photos) {
        printf("E %s: num %d not found in photos list\n", progname, num);
        return;
    }

    // unmap the metadata
    if (photos[i].md != NULL) {
        util_unmap_file(photos[i].md, sizeof(metadata_t));
        photos[i].md = NULL;
    }

    // remove the entry from the photos list
    memmove(&photos[i], &photos[i+1], (max_photos-i-1) * sizeof(photo_t));
    max_photos--;

    // delete photo jpg and meta files
    util_delete_file(photos_dir, photo_filename);
    util_delete_file(photos_dir, metadata_filename);

    // debug print
    printf("I %s: deleted %s\n", progname, photo_filename);
}

