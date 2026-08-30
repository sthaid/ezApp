// xxx
// - bring in noto fonts
// - recreate metadata, or delete photo if bad metadata, or skip photo if bad metadata
// - replace 'TAKE' with a circle
// - in galery mode, when show photo and go back to gallery, may want to indicate which was the last photo viewed
// - make script to create the test files
// - change the large EVID numbers to 1000000000

// - full review and comments

// ==================================

#include "apps/Camera/common.h"

// prototypes
void init(void);
void cleanup(void);

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
            gallery_view();
            break;
        case LOCATION_VIEW:
            location_view();
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
            printf("E %s: photos array is full\n", progname);
            break;
        }
    }
    pclose(fp);

    printf("I %s: init complete, max_photos = %d  duration = %ld ms\n", 
          progname, max_photos, (util_microsec_timer() - t_start) / 1000);
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

    // determine the photo number
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

    // check idx arg
    if (idx < 0 || idx >= max_photos) {
        printf("E %s: idx %d out of range 0..%d\n", progname, idx, max_photos);
        return;
    }

    // this loop supports moving to the next or prev photo
    do {
        restart = false;

        // get the photo num and the metadata ptr
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

        // get jpeg pixels, and jpeg width & height;
        // create texture 't', with the jpeg_w,jpeg_h dimensions
        // copy the pixels to the texture 't'
        pixels = jpeg_file_to_rgba_pixels(photos_dir, file, &jpeg_w, &jpeg_h);
        if (pixels == NULL) {
            printf("E %s: jpeg_file_to_rgba_pixels returned NULL\n", progname);
            if (t != NULL) {
                sdlx_destroy_texture(t);
                t = NULL; texture_w = 0; texture_h = 0;
            }
            return;
        }
        if (t != NULL && (jpeg_w != texture_w || jpeg_h != texture_h)) {
            sdlx_destroy_texture(t);
            t = NULL; texture_w = 0; texture_h = 0;
        }
        if (t == NULL) {
            t = sdlx_create_texture(jpeg_w, jpeg_h);
            if (t == NULL) {
                printf("E %s: sdlx_create_texture failed\n", progname);
                return;
            }
            texture_w = jpeg_w;
            texture_h = jpeg_h;
        }
        sdlx_set_texture_pixels(t, pixels);
        free(pixels);

        // init scale and center, so that the full photo will be displayed
        xc = jpeg_w / 2;
        yc = jpeg_h / 2;
        scale = 1;
            
        // display the photo and handle events, 
        // until eiter the EVID_QUIT or EVID_NEXT/PREV envents rcvd
        while (!done && !restart) {
            // init the backbuffer to COLOR_BLACK
            sdlx_display_init(COLOR_BLACK, PORTRAIT);

            // display the scaled photo
            get_src_and_dest(&src, &dest);
            sdlx_render_texture(t, &src, &dest);

            // display photo num at top left of photo;
            // the x coord is adjusted when at the top left of the photo because
            //  that is mostly obscured by the bezel
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
            sdlx_register_control_events(EVID_RST, "Rst", EVID_TAKE, TAKE, EVID_QUIT, "X");

            // present the display
            sdlx_display_present();

            // wait for an event, with 1 sec timeout
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
                k = (double)jpeg_w / sdlx_win_width;
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
    printf("ASPECT = %f\n", aspect);
    printf("SRC %d %d - %d %d\n", src->x, src->y, src->w, src->h);
    printf("DST %d %d - %d %d\n", dest->x, dest->y, dest->w, dest->h);
#endif
}


// ------------------ UTILS ----------------------------

// caller must free returned pixels
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

// caller must free returned pixels
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

// metadata must be unmapped when program terminates
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

    // - magic, size, num
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

    // debug print metadata
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
