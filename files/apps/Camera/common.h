#ifndef __COMMON__
#define __COMMON__

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

//
// defines
//

#define MAX_PHOTOS    1000

#define GALLERY_VIEW  0
#define LOCATION_VIEW 1

#define ONE_SEC 1000000

#define THUMB   475
#define SPACING 525

#define TAKE "\u2b24"  // large filled circle

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

//
// typedefs
//

#define METADATA_MAGIC 0x12345678
typedef struct {
    int magic;  // xxx validate magic , also add sizeof check
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

//
// variables
//

char   *progname;
char   *data_dir;

bool    end_program;
int     view;

char    photos_dir[100];
photo_t photos[MAX_PHOTOS];
int     max_photos;

//
// prototypes
//

void gallery(void);
void location(void);

int take_photo(void);
void delete_photo(int idx);
void show_photo(int idx);

void settings(void);

#endif
