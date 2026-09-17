#include "apps/Camera/common.h"

//
// defines
//

#define MAP_Y     0
#define PHOTOS_Y  700

#define MAP_W     1000
#define MAP_H     700
#define PHOTOS_W  1000
#define PHOTOS_H  1000

#define MAX_HEAD  48

#define LAT2MILES 69.17

#define BOSTON_LATITUDE  42.3611
#define BOSTON_LONGITUDE -71.0571

#define MOTIONING_OFF       0
#define MOTIONING_IN_MAP    1
#define MOTIONING_IN_PHOTOS 20

#define MIN_MAP_LATITUDE  -72
#define MAX_MAP_LATITUDE   72
#define MIN_MAP_LONGITUDE -180
#define MAX_MAP_LONGITUDE  180

//
// typedefs
//

typedef struct {
    node_t       node;
    int          num_entries;
    unsigned int n_idx;
    unsigned int e_idx;
} head_t;

//
// variables
//

// map center and width/height and cos of latitude
double map_latitude;
double map_longitude;
char   map_loc_state[21];
double map_lat_cosine;

// list of photos indexed by location on map
head_t head[MAX_HEAD];

// used for scolling photos
double y_top;

// enable photo delete
bool del_mode;

// state of motioning, either off, or motioning in map, or motioning in photos
int motioning_state;

// the map is first rendered to this texture, then this texture is rendered 
// to the display; the purpose is to prevent the 'head' squares from overlaying the
// photos display
sdlx_texture_t *display_map_texture;

// for rendering the photo thumbs
sdlx_texture_t *thumb_texture;

// used to adjust the map width
#define MAX_MAP_W_MILES_TBL     12
#define MAP_W_MILES_IDX_DEFAULT 8
#define MAP_W_MILES             (map_w_miles_tbl[map_w_miles_idx])
int map_w_miles_idx;
double map_w_miles_tbl[MAX_MAP_W_MILES_TBL] = {
        0.1, 0.2, 0.5,
        1, 2, 5,
        10, 20, 50,
        100, 200, 500 };

// flag indicating call to display_init is needed; 
// don't want to call display_init unless needed because it takes many cpu cycles
// to place the photos on the proper head list
bool display_init_needed;

//
// prototypes
//

void map_location_init(void);
void sanitize_map_lat_and_long(void);

void display_init(void);
void display_photos(void);
void display_map(void);

double cosd(double degrees);
void set_selected(unsigned int e_idx, unsigned int n_idx);
bool is_selected(unsigned int e_idx, unsigned int n_idx);
void clear_selected(void);

// ------------------ LOCATION VIEW --------------

void location(void)
{
    sdlx_event_t event;
    int          y;
    bool         switch_view = false;

    printf("I %s: location starting\n", progname);

    // init 
    map_location_init();
    map_w_miles_idx = MAP_W_MILES_IDX_DEFAULT;
    motioning_state = MOTIONING_OFF;
    display_map_texture = sdlx_create_texture(MAP_W, MAP_H);
    thumb_texture = sdlx_create_texture(THUMB, THUMB);
    display_init_needed = true;

    // loop until either reqeusted to switch to gallery view, or end program
    while (!switch_view && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // render the map and photos to the display
        if (display_init_needed) {
            display_init();
            display_init_needed = false;
        }
        display_photos();
        display_map();

        // register events 
        // - override any SHOW_PHOTO events that have been
        //   registered in the controls area
        reg_event_fill_rect(0, CTRLS_Y, CTRLS_W, CTRLS_H, COLOR_BLACK, EVID_NOOP);
        // - HOME, PGUP, PGDN, and END
        y = CTRLS_Y + 50;
        reg_event_str(COL2X(0), y, COLOR_LIGHT_BLUE, "Home", EVID_HOME);
        reg_event_str(COL2X(7), y, COLOR_LIGHT_BLUE, "Up", EVID_PGUP);
        reg_event_str(COL2X(12), y, COLOR_LIGHT_BLUE, "Dn", EVID_PGDN);
        reg_event_str(COL2X(17), y, COLOR_LIGHT_BLUE, "End", EVID_END);
        // - DEL and VIEW
        y += 130;
        reg_event_str(0, y, COLOR_LIGHT_BLUE, "Del", EVID_DEL);
        reg_event_str(CTRLS_W-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "View", EVID_VIEW);
        // - MOTION
        sdlx_register_event(NULL, EVID_MOTION);
        // - MAP_SCALE_PLUS, MAP_SCALE_MINUS, MAP_CENTER
        reg_event_str(MAP_W-COL2X(1.5), MAP_Y+0.333*MAP_H-ROW2Y(0.5), 
                      COLOR_LIGHT_BLUE, "+", EVID_MAP_SCALE_PLUS);
        reg_event_str(MAP_W-COL2X(1.5), MAP_Y+0.666*MAP_H-ROW2Y(0.5), 
                      COLOR_LIGHT_BLUE, "-", EVID_MAP_SCALE_MINUS);
        reg_event_str(MAP_W-COL2X(3.5), MAP_Y, 
                      COLOR_LIGHT_BLUE, "Ctr", EVID_MAP_CENTER);
        // - STG, TAKE, QUIT
        sdlx_register_control_events(EVID_STG, "Stg", EVID_TAKE, UNICODE_CIRCLE, EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with infinite timeout
        sdlx_get_event(-1, &event);

        // process events
        if (event.event_id >= EVID_MAP && event.event_id < EVID_MAP+MAX_HEAD) {
            int i = event.event_id - EVID_MAP;
            set_selected(head[i].e_idx, head[i].n_idx);
        } else if (event.event_id >= EVID_SHOW_PHOTO && event.event_id < EVID_SHOW_PHOTO+max_photos) {
            int idx = event.event_id - EVID_SHOW_PHOTO; 
            show_photo(idx);
        } else if (event.event_id >= EVID_DELETE_PHOTO && event.event_id < EVID_DELETE_PHOTO+max_photos) {
            int idx = event.event_id - EVID_DELETE_PHOTO;
            delete_photo(idx);
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
                display_init_needed = true;
                break;
            case EVID_DEL:
                del_mode = !del_mode;
                display_init_needed = true;
                break;
            case EVID_MOTION_BEGIN:
                motioning_state = (event.u.motion_begin.y >= PHOTOS_Y 
                                   ? MOTIONING_IN_PHOTOS
                                   : MOTIONING_IN_MAP);
                if (motioning_state == MOTIONING_IN_MAP) {
                    map_loc_state[0] = '\0';
                }
                break;
            case EVID_MOTION:
                if (motioning_state == MOTIONING_IN_MAP) {
                    double map_h_miles = MAP_W_MILES * ((double)MAP_H / MAP_W);

                    map_latitude  += event.u.motion.yrel * (map_h_miles / MAP_H) / 
                                         LAT2MILES;
                    map_longitude -= event.u.motion.xrel * (MAP_W_MILES / MAP_W) / 
                                         (LAT2MILES * map_lat_cosine);
                    display_init_needed = true;

                    sanitize_map_lat_and_long();

                    double new_map_lat_cosine = cosd(map_latitude);
                    if ((fabs(new_map_lat_cosine - map_lat_cosine) / map_lat_cosine) > 0.2) {
                        printf("I %s: updating map_lat_cosine from %0.3f to %0.3f\n", 
                               progname, map_lat_cosine, new_map_lat_cosine);
                        map_lat_cosine = new_map_lat_cosine;
                    }
                } else if (motioning_state == MOTIONING_IN_PHOTOS) {
                    y_top -= event.u.motion.yrel;
                } else {
                    printf("E %s: EVID_MOTION not expected\n", progname);
                }
                break;
            case EVID_MOTION_END:
                if (motioning_state == MOTIONING_IN_MAP) {
                    find_nearest_city(map_latitude, map_longitude,
                                      NULL, 0, map_loc_state, sizeof(map_loc_state));
                }
                motioning_state = MOTIONING_OFF;
                break;
            case EVID_MAP_SCALE_PLUS:
                if (map_w_miles_idx < MAX_MAP_W_MILES_TBL-1) {
                    map_w_miles_idx++;
                    display_init_needed = true;
                }
                break;
            case EVID_MAP_SCALE_MINUS:
                if (map_w_miles_idx > 0) {
                    map_w_miles_idx--;
                    display_init_needed = true;
                }
                break;
            case EVID_MAP_CENTER:
                map_location_init();
                display_init_needed = true;
                break;
            case EVID_HOME:
                y_top = 0;
                break;
            case EVID_END:
                y_top = 1e99;
                break;
            case EVID_PGUP:
                y_top -= (2 * SPACING);
                break;
            case EVID_PGDN:
                y_top += (2 * SPACING);
                break;
            case EVID_VIEW:
                view = GALLERY_VIEW;  
                switch_view = true;
                break;
            case EVID_STG:
                settings();
                break;
            }
        }
    }

    // cleanup
    sdlx_destroy_texture(display_map_texture);
    display_map_texture = NULL;
    sdlx_destroy_texture(thumb_texture);
    thumb_texture = NULL;
}

void map_location_init(void)
{
    util_get_location(&map_latitude, &map_longitude, NULL, NULL);
    if (map_latitude == INVALID_NUMBER || map_longitude == INVALID_NUMBER) {
        map_latitude  = BOSTON_LATITUDE;
        map_longitude = BOSTON_LONGITUDE;
    }

    sanitize_map_lat_and_long();

    find_nearest_city(map_latitude, map_longitude,
                      NULL, 0, map_loc_state, sizeof(map_loc_state));

    map_lat_cosine = cosd(map_latitude);
}


void sanitize_map_lat_and_long(void)
{
    if (map_latitude < MIN_MAP_LATITUDE) {
        map_latitude = MIN_MAP_LATITUDE;
    } else if (map_latitude > MAX_MAP_LATITUDE) {
        map_latitude = MAX_MAP_LATITUDE;
    }
        
    if (map_longitude < MIN_MAP_LONGITUDE) {
        map_longitude = MIN_MAP_LONGITUDE;
    } else if (map_longitude > MAX_MAP_LONGITUDE) {
        map_longitude = MAX_MAP_LONGITUDE;
    }
}

// -----------------  DISPLAY ROTUINES  --------------------------

double map_n, map_e, map_h, map_w;
double head_w, head_h;

void display_init(void)
{
    int           i, j; 
    unsigned int  head_n_idx, head_e_idx, head_e_first_idx;

    map_w = MAP_W_MILES;
    map_h = MAP_W_MILES * ((double)MAP_H / MAP_W);

    head_w = map_w / 7;
    head_h = map_h / 5;

    map_n = (map_latitude + 90) * LAT2MILES + map_h/2;
    map_e = (map_longitude + 180) * (LAT2MILES * map_lat_cosine) - map_w/2;

    map_n -= head_h;
    map_e += head_w;

    head_n_idx = map_n / head_h + 1;
    head_e_idx = map_e / head_w;
    head_e_first_idx = head_e_idx;

    for (i = 0; i < MAX_HEAD; i++) {
        head_t *hd = &head[i];

        init_list_head(&hd->node);
        hd->num_entries = 0;
        hd->e_idx       = head_e_idx;
        hd->n_idx       = head_n_idx;
        
        head_e_idx++;
        if (head_e_idx > head_e_first_idx + 7) {
            head_e_idx = head_e_first_idx;
            head_n_idx--;
        }
    }

    for (i = 0; i < max_photos; i++) {
        metadata_t *md = photos[i].md;
        double photo_n, photo_e;
        unsigned int photo_n_idx, photo_e_idx;
        
        photo_e = (md->longitude + 180) * LAT2MILES * map_lat_cosine;
        photo_n = (md->latitude + 90) * LAT2MILES;

        photo_e_idx = photo_e / head_w;
        photo_n_idx = photo_n / head_h;

        if (photo_e_idx < head[0].e_idx) continue;
        if (photo_n_idx > head[0].n_idx) continue;
        if (photo_e_idx > head[MAX_HEAD-1].e_idx) continue;
        if (photo_n_idx < head[MAX_HEAD-1].n_idx) continue;

        for (j = 0; j < MAX_HEAD; j++) {
            head_t *hd = &head[j];
            if (hd->n_idx == photo_n_idx && hd->e_idx == photo_e_idx) {
                add_to_list_tail(&hd->node, &photos[i].node);
                hd->num_entries++;
                break;
            }
        }
    }
}

void display_map(void)
{
    sdlx_color_t color;
    int i;
    sdlx_loc_t loc;
    char name[9];
    bool slctd;

    sdlx_set_render_target(display_map_texture);

    sdlx_render_fill_rect(0, MAP_Y, MAP_W, MAP_H, COLOR_DARK_GRAY);

    for (i = 0; i < MAX_HEAD; i++) {
        head_t *hd = &head[i];

        if (hd->num_entries == 0) {
            continue;
        }

        slctd = is_selected(hd->e_idx, hd->n_idx);
        color = (slctd ? COLOR_ORANGE : COLOR_WHITE);

        photo_t *photo = (photo_t*)(hd->node.next);
        metadata_t *md = photo->md;
        strncpy(name, md->city, 8);
        name[8] = '\0';

        loc.w = head_w * (1000 / map_w);
        loc.h = head_h * (700 / map_h);
        loc.x = ((hd->e_idx * head_w)  - map_e) * (1000 / map_w);
        loc.y = (map_n - (hd->n_idx * head_h)) * (700 / map_h);
        sdlx_render_fill_rect(loc.x+10, loc.y+10, loc.w-20, loc.h-20, color);

        sdlx_render_printf_ex(loc.x+15, loc.y+15, 35, COLOR_BLACK, loc.w-30, "%s", name);

        sdlx_register_event(&loc, EVID_MAP + i);
    }

    if (map_loc_state[0] != '\0') {
        sdlx_render_printf_ex(0, MAP_Y+MAP_H-sdlx_char_height(FONT_SMALL),
                            FONT_SMALL, COLOR_WHITE, FLAG_NONE, "%s", 
                            map_loc_state);
    } else {
        sdlx_render_printf_ex(0, MAP_Y+MAP_H-sdlx_char_height(FONT_SMALL),
                            FONT_SMALL, COLOR_WHITE, FLAG_NONE, "%0.4f %0.4f", 
                            map_latitude, map_longitude);
    }

    char w_str[20];
    if (MAP_W_MILES < 1) {
        sprintf(w_str, "%0.1f", MAP_W_MILES);
    } else {
        sprintf(w_str, "%0.0f", MAP_W_MILES);
    }
    sdlx_render_printf_ex(WIN_W-strlen(w_str)*sdlx_char_width(FONT_SMALL), MAP_Y+MAP_H-sdlx_char_height(FONT_SMALL),
                           FONT_SMALL, COLOR_WHITE, FLAG_NONE, "%s", w_str);

    sdlx_set_render_target(NULL);
    loc.x = 0;
    loc.y = MAP_Y;
    loc.w = MAP_W;
    loc.h = MAP_H;
    sdlx_render_texture(display_map_texture, NULL, &loc);
}

void display_photos(void)
{
    int i, num_selected_photos, max_y_top;
    node_t *node;
    int cnt = -1;
    bool slctd[MAX_HEAD];

    for (i = 0; i < max_photos; i++) {
        photos[i].show = false;
    }

    // determine number of selected photos
    num_selected_photos = 0;
    for (i = 0; i < MAX_HEAD; i++) {
        head_t *hd = &head[i];
        slctd[i] = is_selected(hd->e_idx, hd->n_idx);
        if (slctd[i]) {
            num_selected_photos += hd->num_entries;
        }
    }

    // clamp y_top
    max_y_top = ((num_selected_photos + 1) / 2 - 2) * SPACING;
    if (y_top > max_y_top) y_top = max_y_top;
    if (y_top < 0) y_top = 0;

    // loop over map list heads
    for (i = 0; i < MAX_HEAD; i++) {
        head_t *hd = &head[i];

        // if list head is not selected then continue
        if (!slctd[i]) {
            continue;
        }

        // loop over all photos in the list, and set the show flag; 
        // the show flag specifies which of the photos will be displayed
        // when calling show_photo and selecting '>' (next) or '<' (previous)
        for (node = hd->node.next; node != &hd->node; node = node->next) {
            photo_t *photo = (photo_t*)node;
            photo->show = true;
        }

        // loop over all photos in the list, and display the thumb
        // for each photo that is visible on the display
        for (node = hd->node.next; node != &hd->node; node = node->next) {
            photo_t *photo = (photo_t*)node;
            metadata_t *md = photo->md;
            sdlx_loc_t dest;
            int x, y, idx;

            cnt++;

            x = (cnt % 2) * SPACING;
            y = (cnt / 2) * SPACING;

            if (y < y_top - SPACING) {
                continue;
            }
            if (y > y_top + PHOTOS_H) {
                break;
            }

            sdlx_set_texture_pixels(thumb_texture, md->pixels);
            dest.x = x;
            dest.y = y - y_top + PHOTOS_Y;
            dest.w = THUMB;
            dest.h = THUMB;
            sdlx_render_texture(thumb_texture, NULL, &dest);

            // should be able to use 'idx = photo - photos;' 
            // however picoc does not handle that correctly, 
            // instead the following code is used
            idx = ((char*)photo - (char*)&photos[0]) / sizeof(photo_t);
            sdlx_register_event(&dest, EVID_SHOW_PHOTO+idx);

            if (del_mode) {
                reg_event_str(dest.x + THUMB - sdlx_char_width_dflt, dest.y,
                              COLOR_RED, "X", EVID_DELETE_PHOTO+idx);
            }

            sdlx_render_printf_ex(dest.x, dest.y, FONT_SMALL, COLOR_WHITE, FLAG_NONE, "%d", md->num);
            sdlx_render_printf_ex(dest.x+THUMB/2, dest.y+THUMB-sdlx_char_height(FONT_SMALL), 
                                   FONT_SMALL, COLOR_WHITE, FLAG_X_CTR, "%s", md->date);
        }
    }
}

// -----------------  UTILS  -------------------------------------

double cosd(double degrees)
{
    return cos(degrees * DEG2RAD);
}

unsigned long Slctd;

void set_selected(unsigned int e_idx, unsigned int n_idx)
{
    unsigned long slctd = ((unsigned long)e_idx << 32) | (n_idx);

    if (Slctd == slctd) {
        Slctd = 0;
    } else {
        Slctd = slctd;
    }
}

bool is_selected(unsigned int e_idx, unsigned int n_idx)
{
    unsigned long slctd = ((unsigned long)e_idx << 32) | (n_idx);
    return slctd == Slctd;
}

void clear_selected(void)
{
    Slctd = 0;
}
