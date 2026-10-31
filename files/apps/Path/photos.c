#include <stdio.h>

#include <sdlx.h>
#include <utils.h>

#include "apps/Path/common.h"

// variables
char photos_dir[100];

// -----------------  PHOTOS  ----------------------------------------

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

int photos_take(void)
{   
    int idx, rc;
    double lat, lon;
    char photo_filename[100];

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

int photos_display(int idx)
{
    printf("I %s: display photo idx %d\n", progname, idx);
    printf("xxxxxxxxxx todo xxxxxxxxxxxxx\n");
    return 0;
}
