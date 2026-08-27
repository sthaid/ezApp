// xxx
// - tap dispaly to show arrow controls
// - display the photo nuber position adjust
// - bring in noto fonts
// - home end, pgup pgdn
// - when take photo, update the scroll location in galery
// - recreate metadata, or delete photo if bad metadata, or skip photo if bad metadata
//
// - replace 'TAKE' with a circle
// - cleanup needed?
// - in galery mode, when show photo and go back to gallery, may want to indicate which was the last photo viewed
// - also search for yyy

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
#define EVID_VIEW           4
#define EVID_STG            5
#define EVID_CTR            6
#define EVID_RST            7
#define EVID_NEXT           8
#define EVID_PREV           9
#define EVID_NOOP           10
#define EVID_HOME           11
#define EVID_END            12
#define EVID_PGUP           13
#define EVID_PGDN           14
#define EVID_SHOW_PHOTO     10000
#define EVID_DELETE_PHOTO   20000

#define ONE_SEC 1000000

#define THUMB 475
#define SPACING 525

#define METADATA_MAGIC 0x12345678

// typedefs
typedef struct {
    int magic;  // yyy validate magic , also add sizeof check
    int size;
    int num;
    char day[50];
    char date[50];
    char time[50];
    char city[50];
    char state[50];
    double latitude;
    double longitude;
    unsigned int pixels[THUMB*THUMB];
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

int take_photo(void);
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
    long        t_start;

    // init global variable photos_dir
    sprintf(photos_dir, "%s/photos", data_dir);

#if 0 // yyy del
    // make test files
    for (int i = 20; i < 1000; i++) {
        char dest[100];
        printf("i = %d\n", i);
        sprintf(dest, "%06d.jpg", i);
        util_copy_file(photos_dir, "000015.jpg", photos_dir, dest);
        sprintf(dest, "%06d.meta", i);
        util_copy_file(photos_dir, "000015.meta", photos_dir, dest);
    }
#endif

    // initialize the photos array using sorted list of jpg 
    // files that are in the photos dir

    t_start = util_microsec_timer();

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

        // if photos array is full then break
        if (max_photos == MAX_PHOTOS) {
            break;
        }
    }
    pclose(fp);

    printf("I %s: init complete, max_photos = %d  duration = %ld ms\n", 
          progname, max_photos, (util_microsec_timer() - t_start) / 1000);

#if 0 // yyy del
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
    sdlx_event_t    event;
    int             x, y, ctrls_h, images_h, y_last;
    sdlx_texture_t *t;
    sdlx_loc_t      dest;
    double          y_top = 0;
    bool            del_mode = false;
    bool            done = false;

    t = sdlx_create_texture(THUMB, THUMB);

    ctrls_h = 3.5 * sdlx_char_height_dflt;
    images_h = sdlx_win_height - ctrls_h;  // yyx not used?

    while (!done && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // xxx todo
        // xxx optimize loop start
        for (int i = 0; i < max_photos; i++) {
            metadata_t *md = photos[i].md;

            y = (i / 2) * SPACING;
            if (y < y_top - SPACING) {
                continue;
            }
            if (y > y_top + sdlx_win_height) {
                break;
            }

            sdlx_set_texture_pixels(t, md->pixels);
            dest.x = (i % 2) * SPACING;
            dest.y = y - y_top;
            dest.w = THUMB;
            dest.h = THUMB;
            sdlx_render_texture(t, NULL, &dest);

            sdlx_register_event(&dest, EVID_SHOW_PHOTO+i);

            int tmp_x = ((dest.x == 0 && dest.y == 0) ? 40 : dest.x);
            sdlx_render_printf_ex2(tmp_x, dest.y, FONT_SMALL, COLOR_WHITE, 0, "%d", md->num);
            sdlx_render_printf_ex2(dest.x+THUMB/2, dest.y+THUMB-sdlx_char_height(FONT_SMALL), 
                                   FONT_SMALL, COLOR_WHITE, FLAG_X_CTR, "%s", md->date);

            if (del_mode) {
                x = dest.x + THUMB - sdlx_char_width_dflt;
                y = dest.y;
                reg_event_str(x, y, COLOR_RED, "X", EVID_DELETE_PHOTO+i);
            }
        }

        // register events
        if (!del_mode) {
            reg_event_show_readme_file();
        }

        reg_event_fill_rect(0, sdlx_win_height-ctrls_h, sdlx_win_width, ctrls_h, COLOR_BLACK, EVID_NOOP);

        y = sdlx_win_height - ROW2Y(3);

        reg_event_str(COL2X(0), y, COLOR_LIGHT_BLUE, "Home", EVID_HOME);
        reg_event_str(COL2X(7), y, COLOR_LIGHT_BLUE, "Up", EVID_PGUP);
        reg_event_str(COL2X(12), y, COLOR_LIGHT_BLUE, "Dn", EVID_PGDN);
        reg_event_str(COL2X(17), y, COLOR_LIGHT_BLUE, "End", EVID_END);

        y += 1.5 * sdlx_char_height_dflt;
        reg_event_str(0, y, COLOR_LIGHT_BLUE, "Del", EVID_DEL);
        reg_event_str(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "View", EVID_VIEW);

        sdlx_register_event(NULL, EVID_MOTION);

        sdlx_register_control_events(EVID_STG, "Stg", EVID_TAKE, "Take", EVID_QUIT, "X");

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
            case EVID_QUIT:
                end_program = true;
                break;
            case EVID_SHOW_README_FILE:
                show_file(data_dir, "README");
                break;
            case EVID_TAKE:
                take_photo();
                y_last = (max_photos == 0 ? 0 : ((max_photos + 1) / 2 - 1) * SPACING);
                y_top = y_last - 2 * SPACING;
                break;
            case EVID_DEL:
                del_mode = !del_mode;
                break;
            case EVID_MOTION:
                y_top -= event.u.motion.yrel;
                break;
            case EVID_HOME:
                y_top = 0;
                break;
            case EVID_END:
                y_top = 1e99; //xxx do this like take
                break;
            case EVID_PGUP:
                y_top -= (3 * SPACING);
                break;
            case EVID_PGDN:
                y_top += (3 * SPACING);
                break;
            case EVID_VIEW:
                view = LOCATION_VIEW;
                done = true;
                break;
            case EVID_STG:
                settings();
                break;
            }

            y_last = (max_photos == 0 ? 0 : ((max_photos + 1) / 2 - 1) * SPACING);

            if (y_top > y_last - 2 * SPACING) y_top = y_last - 2 * SPACING;
            if (y_top < 0) y_top = 0;
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

    while (!done && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // register events yyy add pinch
        reg_event_show_readme_file();
        y = sdlx_win_height - 1.5 * sdlx_char_height_dflt;
        reg_event_str(0, y, COLOR_LIGHT_BLUE, "CTR", EVID_CTR);
        reg_event_str(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "VIEW", EVID_VIEW);
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

int take_photo(void)
{
    int rc, num;
    char photo_filename[100];
    metadata_t *md;

    // take photo; this will create file tmp/photo.jpg
    rc = util_take_photo();  
    if (rc != 0) {
        printf("E %s: util_take_photo failed\n", progname);
        sdlx_show_toast("take picture failed");
        return -1;
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
        return -1;
    }
    
    // add photo to list
    photos[max_photos].num = num;
    photos[max_photos].md = md;
    max_photos++;

    // success
    return 0;
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

// yyy tighten up this routine, what?

void get_src_and_dest(sdlx_loc_t *src, sdlx_loc_t *dest);

int jpeg_w, jpeg_h;
double xc, yc, scale;

void show_photo(int idx)
{
    int             num, rc, texture_w=0, texture_h=0, y;
    char            file[50];
    metadata_t     *md;
    sdlx_texture_t *t = NULL;
    sdlx_loc_t      src, dest;
    sdlx_event_t    event;
    unsigned int   *pixels;
    sdlx_loc_t     *loc;
    bool            show;
    char           *s;
    long            last_next_prev_time = util_microsec_timer();
    bool            done = false;
    bool            restart = false;

    int orientation = PORTRAIT; // yyy ?

    // check idx arg
    if (idx < 0 || idx >= max_photos) {
        printf("E %s: idx %d out of range 0..%d\n", progname, idx, max_photos);
        return;
    }

    // yyy comment
    do {
        restart = false;

        // yyy
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

        // create texture yyy check pixels return
        pixels = jpeg_file_to_rgba_pixels(photos_dir, file, &jpeg_w, &jpeg_h);
        if (t != NULL && (jpeg_w != texture_w || jpeg_h != texture_h)) {
            sdlx_destroy_texture(t);
            t = NULL; texture_w = 0; texture_h = 0;
        }
        if (t == NULL) {
            t = sdlx_create_texture(jpeg_w, jpeg_h);
            texture_w = jpeg_w;
            texture_h = jpeg_h;
        }
        sdlx_set_texture_pixels(t, pixels);
        free(pixels);

        // yyy
        xc = jpeg_w / 2;
        yc = jpeg_h / 2;
        scale = 1;
            
        while (!done && !restart) {
            // init the backbuffer to COLOR_BLACK
            sdlx_display_init(COLOR_BLACK, PORTRAIT);

            // yyy comment
            get_src_and_dest(&src, &dest);
            sdlx_render_texture(t, &src, &dest);

            // display photo num at top left of photo
            int tmp_x = ((dest.x == 0 && dest.y == 0) ? 40 : dest.x);
            sdlx_render_printf_ex2(tmp_x, dest.y, FONT_SMALL, COLOR_WHITE, 0, "%d", md->num);

            // display metadata below photo
            y = 1400;
            sdlx_render_printf(0, y, "%s %s\n%s", md->day, md->date, md->time);
            y += 2 * sdlx_char_height_dflt;
            if (md->city[0] != '\0') {
                sdlx_render_printf(0, y, "%s", md->city);
                y += sdlx_char_height_dflt;
            }
            if (md->state[0] != '\0') {
                sdlx_render_printf(0, y, "%s", md->state);
                y += sdlx_char_height_dflt;
            }
            if (md->latitude != INVALID_NUMBER && md->longitude != INVALID_NUMBER) {
                sdlx_render_printf(0, y, "%.4f %.4f", md->latitude, md->longitude);
                y += sdlx_char_height_dflt;
            }

            // register events
            show = (util_microsec_timer() - last_next_prev_time) < 3000000;
            s = (show ? "<" : " ");
            loc = sdlx_render_printf_ex2(0.5*sdlx_char_width(FONT_LARGE), 1333/2, 
                                         FONT_LARGE, COLOR_WHITE, FLAG_XY_CTR, "%s", s);
            sdlx_register_event(loc, EVID_PREV);

            s = (show ? ">" : " ");
            loc = sdlx_render_printf_ex2(sdlx_win_width-0.5*sdlx_char_width(FONT_LARGE), 1333/2, 
                                         FONT_LARGE, COLOR_WHITE, FLAG_XY_CTR, "%s", s);
            sdlx_register_event(loc, EVID_NEXT);

            sdlx_register_event(NULL, EVID_PINCH);
            sdlx_register_event(NULL, EVID_MOTION);
            sdlx_register_control_events(EVID_RST, "Rst", EVID_TAKE, "Take", EVID_QUIT, "X");

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
            case EVID_MOTION: {
                double k;
                if (orientation == PORTRAIT) {
                    k = (double)jpeg_w / sdlx_win_width;
                } else {
                    k = (double)jpeg_h / sdlx_win_height;
                }
                xc -= event.u.motion.xrel * scale * k;
                yc -= event.u.motion.yrel * scale * k;
                last_next_prev_time = util_microsec_timer();
                break; }
            case EVID_PINCH:
                if (event.u.pinch.scale == 0) break;
                scale /= event.u.pinch.scale;
                if (scale > 1) scale = 1;
                if (scale < 0.01) scale = 0.01;
                break;
            case EVID_RST:
                xc = jpeg_w / 2;
                yc = jpeg_h / 2;
                scale = 1;
                break;
            case EVID_TAKE: {
                rc = take_photo();
                if (rc != 0) {
                    break;
                }
                restart = true;
                idx = max_photos-1;
                break; }
            case EVID_NEXT:
                idx = (idx < max_photos-1 ? idx+1 : 0);
                restart = true;
                last_next_prev_time = util_microsec_timer();
                break;
            case EVID_PREV:
                idx = (idx > 0 ? idx-1 : max_photos-1);
                restart = true;
                last_next_prev_time = util_microsec_timer();
                break;
            }
        }
    } while (restart);

    sdlx_destroy_texture(t);
    t = NULL;
}

// yyy use nearbyint
void get_src_and_dest(sdlx_loc_t *src, sdlx_loc_t *dest)
{
    double aspect;

    src->w = jpeg_w * scale;
    src->h = jpeg_h;
    aspect = (double)src->h / src->w;
    if (aspect > 1.333) {
        src->h = src->w * 1.333;
        aspect = 1.333;
    }
    src->x = xc - src->w / 2;
    src->y = yc - src->h / 2;

    dest->w = 1000;
    dest->h = aspect * dest->w;
    dest->x = 0;
    dest->y = (1333 - dest->h) / 2;

    // if src region extends beyond the bounds of the image
    // then adjust src to keep it within the image bounds
    // yyy cleanup, move prints to bottom, etc
    if (src->x < 0) {
        src->x = 0;
        xc = src->x + src->w / 2;
    } else if (src->x + src->w >= jpeg_w) {
        src->x = jpeg_w - src->w;
        xc = src->x + src->w / 2;
    }
    if (src->y < 0) {
        src->y = 0;
        yc = src->y + src->h / 2;
    } else if (src->y + src->h >= jpeg_h) {
        src->y = jpeg_h - src->h;
        yc = src->y + src->h / 2;
    }

#if 0 // yyy also print the first Src
    // yyy update prints with progname
    printf("ASPECT = %f\n", aspect);
    printf("SRC %d %d - %d %d\n", src->x, src->y, src->w, src->h);
    printf("DST %d %d - %d %d\n", dest->x, dest->y, dest->w, dest->h);
#endif
}

// ------------------ SETTINGS -------------------------

void settings(void)
{
    // yyy todo
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

    // - magic, size and num
    md->magic = METADATA_MAGIC;
    md->size = sizeof(metadata_t);
    md->num = num;

    // - date & time
    t = time(NULL);
    tm = localtime(&t);
    strftime(md->day, sizeof(md->day), "%a", tm);
    strftime(md->date, sizeof(md->date), "%b %d %Y", tm);
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
    pixels = jpeg_file_to_rgba_pixels_scaled(photos_dir, photo_filename, THUMB, THUMB);
    if (pixels == NULL) {
        util_unmap_file(md, sizeof(metadata_t));
        util_delete_file(photos_dir, metadata_filename);
        return NULL;
    }
    memcpy(md->pixels, pixels, 4*THUMB*THUMB);
    free(pixels);

    // sync the metadata file to storage
    util_sync_file(md, sizeof(metadata_t));

    // debug print metadata yyy check that all fields are printed
    printf("I %s: metadata ...\n", progname);
    printf("I %s:   magic      = 0x%x\n",  progname, md->magic);
    printf("I %s:   size       = %d\n",    progname, md->size);
    printf("I %s:   num        = %d\n",    progname, md->num);
    printf("I %s:   day        = %s\n",    progname, md->day);
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
