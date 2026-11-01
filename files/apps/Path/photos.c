#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // xxx del

#include <sdlx.h>
#include <utils.h>

#include "apps/Path/common.h"

// Much of the code in this file has been ported from the Camera miniApp.

// defines
#define EVID_SCALE_RESET 1
#define EVID_CHANGE_NAME 2

// variables
char photos_dir[100];

// xxx all writes to params should sync
// xxx add Del photo

// -----------------  INIT AND CLEANUP  ------------------------------

int photos_init(void)
{
    int idx;

    sprintf(photos_dir, "%s/%s", data_dir, "photos");
    photos = util_map_file(photos_dir, "photos.dat", MAX_PHOTOS*sizeof(photo_t), true, NULL);

    for (idx = 0; idx < MAX_PHOTOS; idx++) {
        if (PHOTO_EXISTS(idx)) {
            max_photos = idx+1;
        }
    }

    printf("I %s: max_photos = %d\n", progname, max_photos);
    return 0; //xxx check for error
}

void photos_cleanup(void)
{
    util_unmap_file(photos, MAX_PHOTOS*sizeof(photo_t));
}

// -----------------  TAKE PHOTO  -------------------------------------

int photos_take(void) //xxx name
{   
    int     idx, rc;
    double  lat, lon;
    char    photo_filename[100];
    char   *photo_name;

    // find unused slot in photos array;
    // if not found then return error
    for (idx = 0; idx < MAX_PHOTOS; idx++) {
        if (!PHOTO_EXISTS(idx)) {
            break;
        }
    }
    if (idx == MAX_PHOTOS) {
        printf("E %s: photos tbl is full\n", progname);
        return -1;
    }

    // get lat/lon location that will be saved for this photo
    util_get_location(&lat, &lon, NULL, NULL);
    if (lat == INVALID_NUMBER || lon == INVALID_NUMBER) {
        printf("E %s: failed to get lat/lon for photo\n", progname);
        return -1;
    }

    // take photo; this will create file tmp/photo.jpg
    rc = util_take_photo();
    if (rc != 0) {
        printf("E %s: util_take_photo failed\n", progname);
        sdlx_show_toast("take picture failed");
        return -1;
    }

    // move photo from tmp/photo.jpg to photos subdir
    sprintf(photo_filename, "%06d.jpg", idx);
    printf("I %s: creating %s\n", progname, photo_filename);
    util_rename_file("tmp", "photo.jpg", photos_dir, photo_filename);

    // save photo location in photos array, which is mapped to file photos.dat
    sleep(1); // xxx temp
    photo_name = sdlx_get_input_str("Name", false, NULL);
    snprintf(photos[idx].name, sizeof(photos[idx].name), "%s", photo_name);
    photos[idx].lat = lat;
    photos[idx].lon = lon;

    util_sync_file(&photos[idx], sizeof(photo_t));

    // update max_photos, if needed
    if (idx+1 > max_photos) {
        max_photos = idx+1;
    }
    printf("I %s: max_photos is now %d\n", progname, max_photos);

    // success
    return 0;
}

// -----------------  DISPLAY PHOTO  ----------------------------------

// this value determined empirically
#define ZOOM_SCALE 0.5625

double xc, yc, scale;
int    jpeg_w, jpeg_h;

void get_src_and_dest(sdlx_loc_t *src, sdlx_loc_t *dest);

int photos_display(int idx)
{
    bool            done   = false;
    unsigned int   *pixels = NULL;
    sdlx_texture_t *t      = NULL;
    char            photo_filename[100];
    sdlx_loc_t      src, dest, *loc;
    sdlx_event_t    event;
    int             y;

    printf("I %s: display photo idx %d\n", progname, idx);

    // check idx arg
    if (idx < 0 || idx >= max_photos) {
        printf("E %s: idx %d out of range 0..%d\n", progname, idx, max_photos);
        return -1;
    }

    // get jpeg pixels, and jpeg width & height;
    sprintf(photo_filename, "%06d.jpg", idx);
    util_decode_jpeg_to_raw(photos_dir, photo_filename, &jpeg_w, &jpeg_h, &pixels);
    if (pixels == NULL) {
        printf("E %s: jpeg_file_to_rgba_pixels failed\n", progname);
        return -1;
    }

    // create texture 't', with the jpeg_w,jpeg_h dimensions
    t = sdlx_create_texture(jpeg_w, jpeg_h);
    if (t == NULL) {
        printf("E %s: sdlx_create_texture failed\n", progname);
        free(pixels);
        return -1;
    }

    // copy the pixels to the texture 't', and free pixels
    sdlx_set_texture_pixels(t, pixels);
    free(pixels);

    // init scale and center, so that the photo will be displayed
    // scaled to use the entire dest; this will zoom the photo if
    // it was taken in landscape
    xc = jpeg_w / 2;
    yc = jpeg_h / 2;
    scale = (jpeg_w > jpeg_h ? ZOOM_SCALE : 1);
        
    // display the photo and handle events, until EVID_QUIT recvd
    while (!done) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // display the scaled photo
        get_src_and_dest(&src, &dest);
        sdlx_render_texture(t, &src, &dest);

        // display photo name and lat/lon
        y = 1400;
        loc = sdlx_render_printf_ex(
                    0, y, 
                    FONT_NORMAL, COLOR_LIGHT_BLUE, FLAG_NONE,
                    "%s", photos[idx].name);
        sdlx_register_event(loc, EVID_CHANGE_NAME);
        y += sdlx_char_height_dflt;

        sdlx_render_printf(0, y, "%0.4f %0.4f", photos[idx].lat, photos[idx].lon);

        // register events
        sdlx_register_event(NULL, EVID_PINCH);
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_SCALE_RESET, "Reset", 0, NULL, EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for an event, with infinite timeout
        sdlx_get_event(-1, &event);
        if (event.event_id == -1) {
            continue;  // shouldn't ever happen
        }

        // process the event
        switch (event.event_id) {
        case EVID_QUIT:
            done = true;
            break;
        case EVID_MOTION: {
            double k;
            k = (double)jpeg_w / sdlx_win_width;
            xc -= event.u.motion.xrel * scale * k;
            yc -= event.u.motion.yrel * scale * k;
            break; }
        case EVID_PINCH:
            if (event.u.pinch.scale == 0) break;
            scale /= event.u.pinch.scale;
            if (scale > 1) scale = 1;
            if (scale < 0.01) scale = 0.01;
            break;
        case EVID_SCALE_RESET:
            xc = jpeg_w / 2;
            yc = jpeg_h / 2;
            if (scale == 1 && jpeg_w > jpeg_h) {
                scale = ZOOM_SCALE;
            } else {
                scale = 1;
            }
            break;
        case EVID_CHANGE_NAME: {
            char *photo_name;
            photo_name = sdlx_get_input_str("Name", false, NULL);
            snprintf(photos[idx].name, sizeof(photos[idx].name), "%s", photo_name);
            break; }
        }
    }

    // cleanup and return success
    sdlx_destroy_texture(t);
    t = NULL;
    return 0;
}

void get_src_and_dest(sdlx_loc_t *src, sdlx_loc_t *dest)
{
    double aspect;

    // determine the src image rectangle
    src->w = jpeg_w * scale;
    src->h = jpeg_h;
    aspect = (double)src->h / src->w;
    if (aspect > 1.333) {
        src->h = src->w * 1.333;
        aspect = 1.333;
    }
    src->x = xc - src->w / 2;
    src->y = yc - src->h / 2;

    // determine the destination (display) rectangle
    dest->w = 1000;
    dest->h = aspect * dest->w;
    dest->x = 0;
    dest->y = (1333 - dest->h) / 2;

    // if src region extends beyond the bounds of the image
    // then adjust src to keep it within the image bounds
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

#if 0
    // debug prints
    printf("I %: ASPECT = %f\n", progname, aspect);
    printf("I %: SRC %d %d - %d %d\n", progname, src->x, src->y, src->w, src->h);
    printf("I %: DST %d %d - %d %d\n", progname, dest->x, dest->y, dest->w, dest->h);
#endif
}
