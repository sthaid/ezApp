#ifndef __COMMON_H__
#define __COMMON_H__

// defines
#define MAX_PHOTOS 1000
#define PHOTO_EXISTS(idx) (photos[idx].lon != 0)

// typedefs
typedef struct {
    double lat;
    double lon;
} photo_t;

// variables
char    *progname;
char    *data_dir;
int      max_photos;
photo_t *photos;

// prototypes
int photos_init(void);
void photos_cleanup(void);
int photos_take(void);
int photos_display(int idx);

#endif




