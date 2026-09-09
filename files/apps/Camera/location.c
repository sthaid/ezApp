// xxx MAX_PHOTOS, try 10000,  and test this   10000 * 4MB =  40 GB
// xxx settings,  display usage and memory remaining

// xxx when taking test photos, assign city name to Bolton  ? Maybe not needed
// xxx work on zoom,  maybe don't pinch,  use +,-

// xxx display state or city name on map, when motion ends

// yyy option to delete all photos

// yyy how to backup files

// yyy consider a way to also support multiple selections
// yyy all apps need a sdlevent timeout, or an event trigger when coming out of doze

// yyy printf FLAG_RIGHT (maybe not)
// yyy comments
// yyy make ctrls same in gallery

#include "apps/Camera/common.h"

//
// defines
//

#define MAP_Y     0
#define PHOTOS_Y  700
#define CTRLS_Y   1700

#define MAP_W     1000
#define MAP_H     700
#define PHOTOS_W  1000
#define PHOTOS_H  1000
#define CTRLS_W   1000
#define CTRLS_H   300

#define MAX_HEAD  48

#define LAT2MILES 69.17

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

// map center and width/height
double  map_latitude_ctr;
double  map_longitude_ctr;
double  map_w_miles;

// list of photos indexed by location on map
head_t  head[MAX_HEAD];

// used for scolling photos
double  y_top;

// enable photo delete
bool    del_mode;

// yyy comment
sdlx_texture_t *display_map_texture;

//
// prototypes
//

void display_init(void);
void display_photos(void);
void display_map(void);

void lat_long_to_map_xy(double latitude, double longitude, int *x, int *y);
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

    static bool motioning_in_map;

    // init 
    // yyy start where left off on prior run
    map_w_miles = 50;

    util_get_location(&map_latitude_ctr, &map_longitude_ctr, NULL, NULL); //yyy dont let these be invalid
    if (map_latitude_ctr == INVALID_NUMBER || map_longitude_ctr == INVALID_NUMBER) {
        map_latitude_ctr = HOME_LATITUDE;     // yyy rename to DEFAULT
        map_longitude_ctr = HOME_LONGITUDE;
    }
    printf("LOCATION STARTING %0.4f %0.4f\n", map_latitude_ctr, map_longitude_ctr);

    // yyy comment
    while (!switch_view && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // yyy  comment
        display_init();
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
        // - MOTION, PINCH
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_event(NULL, EVID_PINCH);
        // - show-readme-file, STG, TAKE, QUIT
        reg_event_show_readme_file();
        sdlx_register_control_events(EVID_STG, "Stg", EVID_TAKE, UNICODE_CIRCLE, EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with 1 sec timeout
        sdlx_get_event(ONE_SEC, &event);
        if (event.event_id == -1) {
            continue;
        }

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
                take_photo(); // yyy maybe adjust y_top
                break;
            case EVID_DEL:
                del_mode = !del_mode;
                break;
            case EVID_MOTION_BEGIN:
                motioning_in_map = (event.u.motion_begin.y < MAP_Y+MAP_H);
                break;
            case EVID_MOTION:
                if (motioning_in_map) {
                    double map_h_miles = map_w_miles * ((double)MAP_H / MAP_W);
                    map_latitude_ctr  += event.u.motion.yrel * (map_h_miles / MAP_H) / 
                                         LAT2MILES;
                    map_longitude_ctr -= event.u.motion.xrel * (map_w_miles / MAP_W) / 
                                         (LAT2MILES * cosd(map_latitude_ctr));
                } else {
                    y_top -= event.u.motion.yrel;
                }
                break;
            case EVID_PINCH:
                clear_selected();
                map_w_miles /= event.u.pinch.scale;
                if (map_w_miles < 2) {
                    map_w_miles = 2;
                } else if (map_w_miles > 100) {
                    map_w_miles = 100;
                }
                break;
            case EVID_HOME:
                y_top = 0;
                break;
            case EVID_END:
                y_top = 1e99;
                break;
            case EVID_PGUP:
                y_top -= (3 * SPACING);
                break;
            case EVID_PGDN:
                y_top += (3 * SPACING);
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

    if (display_map_texture) {
        sdlx_destroy_texture(display_map_texture);
        display_map_texture = NULL;
    }
}

// -----------------  DISPLAY ROTUINES  --------------------------

// yyy names, or just comment them
double map_n, map_e, map_h, map_w;
double head_w, head_h;

void display_init(void)
{
    int           i, j; 
    unsigned int  head_n_idx, head_e_idx, head_e_first_idx;

    static double cos_map_lat;

    if (cos_map_lat == 0) cos_map_lat = cosd(map_latitude_ctr);  // yyy get again

    map_w = map_w_miles;
    map_h = map_w_miles * ((double)MAP_H / MAP_W);

    head_w = map_w / 7;
    head_h = map_h / 5;

    map_n = (map_latitude_ctr + 90) * LAT2MILES + map_h/2;
    map_e = (map_longitude_ctr + 180) * (LAT2MILES * cos_map_lat) - map_w/2;

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
        
        photo_e = (md->longitude + 180) * LAT2MILES * cos_map_lat;
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

    if (display_map_texture == NULL) {
        display_map_texture = sdlx_create_texture(MAP_W, MAP_H);
    }

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

    sdlx_render_printf_ex(0, MAP_Y+MAP_H-sdlx_char_height(FONT_SMALL),
                           FONT_SMALL, COLOR_WHITE, FLAG_NONE, "%0.4f %0.4f", 
                           map_latitude_ctr, map_longitude_ctr);
    char w_str[20]; // yyy use FLAG_RIGHT
    sprintf(w_str, "w=%0.1f", map_w_miles);
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
    sdlx_texture_t *t;
    bool slctd[MAX_HEAD];

    for (i = 0; i < max_photos; i++) {
        photos[i].show = false;
    }

    // determine number of selected photos
    // yyy simplify because just one is ever selected
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

    // create texture to display the thumb
    t = sdlx_create_texture(THUMB, THUMB);

    // loop over map list heads
    // yyy dont need to loop
    for (i = 0; i < MAX_HEAD; i++) {
        head_t *hd = &head[i];

        // if list head is not selected then continue
        if (!slctd[i]) {
            continue;
        }

        // loop over all photos in the list 
        for (node = hd->node.next; node != &hd->node; node = node->next) {
            photo_t *photo = (photo_t*)node;
            metadata_t *md = photo->md;
            sdlx_loc_t dest;
            int x, y, idx;

            cnt++;
            photo->show = true;

            x = (cnt % 2) * SPACING;
            y = (cnt / 2) * SPACING;

            if (y < y_top - SPACING) {
                continue;
            }
            if (y > y_top + PHOTOS_H) {
                continue;
            }

            sdlx_set_texture_pixels(t, md->pixels);
            dest.x = x;
            dest.y = y - y_top + PHOTOS_Y;
            dest.w = THUMB;
            dest.h = THUMB;
            sdlx_render_texture(t, NULL, &dest);

            idx = ((char*)photo - (char*)&photos[0]) / sizeof(photo_t); //yyy comment picoc issue
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

    sdlx_destroy_texture(t);
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
