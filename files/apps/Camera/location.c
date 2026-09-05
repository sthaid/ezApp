// yyy todo
// - comments
// - make ctrls same in gallery
// - draw line to separate map from photos
// - when all photos deleted the map city went away

// xxx
// - pan and pinch map

#include "apps/Camera/common.h"

//
// defines
//

#define MAX_MAPI  7
#define MAX_MAPJ  5

#define MAP_Y     0
#define PHOTOS_Y  700
#define CTRLS_Y   1700

#define MAP_W     1000
#define MAP_H     700
#define PHTOOS_W  1000
#define PHOTOS_H  1000
#define CTRLS_W   1000
#define CTRLS_H   300

// xxx define for 69.17

//
// typedefs
//

typedef struct {
    node_t node;
    bool   selected;
    int    num_entries;
} head_t;

//
// variables
//

// map center and width/height
double  map_latitude_ctr;
double  map_longitude_ctr;
double  map_w_miles;

// list of photos indexed by location on map
head_t  head[MAX_MAPI][MAX_MAPJ];

// used for scolling photos
double  y_top;

// enable photo delete
bool    del_mode;

//
// prototypes
//

void display_init(void);
void display_photos(void);
void display_map(void);

void lat_long_to_map_xy(double latitude, double longitude, int *x, int *y);
double cosd(double degrees);

// ------------------ LOCATION VIEW --------------

void location(void)
{
    sdlx_event_t event;
    int          y;
    bool         switch_view = false;

    // init 
    map_w_miles = 60;

    util_get_location(&map_latitude_ctr, &map_longitude_ctr, NULL, NULL); //yyy dont let these be invalid
    if (map_latitude_ctr == INVALID_NUMBER || map_longitude_ctr == INVALID_NUMBER) {
        map_latitude_ctr = HOME_LATITUDE;     // yyy rename to DEFAULT
        map_longitude_ctr = HOME_LONGITUDE;
    }
    printf("---------------- %0.4f %0.4f --------------\n", 
          map_latitude_ctr, map_longitude_ctr);

    // yyy
    while (!switch_view && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // yyy 
        display_init();  // yyy move init to inside photos ?
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
        sdlx_get_event(5*ONE_SEC, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        if (event.event_id >= EVID_MAP && event.event_id < EVID_MAP+(MAX_MAPI*MAX_MAPJ)) {
            int i = (event.event_id - EVID_MAP) % MAX_MAPI; //yyy check range?
            int j = (event.event_id - EVID_MAP) / MAX_MAPI;
            printf("GOT ij %d %d\n", i,j);
            head[i][j].selected = !head[i][j].selected;
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
            case EVID_MOTION:
                if (event.u.motion.y >= PHOTOS_Y) {
                    y_top -= event.u.motion.yrel;
                } else {
                    double map_h_miles = map_w_miles * ((double)MAP_H / MAP_W);
                    map_latitude_ctr  += event.u.motion.yrel * (map_h_miles / MAP_H) / 
                                         69.17;
                    map_longitude_ctr -= event.u.motion.xrel * (map_w_miles / MAP_W) / 
                                         (69.17 * cosd(map_latitude_ctr));

                    printf("map motion  map_h_miles = %0.3f\n", map_h_miles);
                    // xxx limit lat long,  is long 0 to 360 or -180 to 180
                }
                break;
            case EVID_PINCH:
                map_w_miles *= event.u.pinch.scale;
                // xxx limit
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
}

// -----------------  DISPLAY ROTUINES  --------------------------

void display_init(void)
{
    int           i, j, idx;

    // init photo list heads all to empty list
    for (i = 0; i < MAX_MAPI; i++) {
        for (j = 0; j < MAX_MAPJ; j++) {
            head[i][j].num_entries = 0;
            init_list_head(&head[i][j].node);
        }
    }

    // yyy
    for (idx = 0; idx < max_photos; idx++) {
        metadata_t *md = photos[idx].md;
        int         x, y;

        lat_long_to_map_xy(md->latitude, md->longitude, &x, &y);

        // xxx check this
        i = x / ((double)PHOTOS_H / MAX_MAPI);
        j = y / ((double)MAP_H / MAX_MAPJ);
        if (i < 0 || i >= MAX_MAPI || j < 0 || j >= MAX_MAPJ) {
            continue;
        }

        add_to_list_tail(&head[i][j].node, &photos[idx].node);
        head[i][j].num_entries++;
    }

    // clear selected flag for lists that have no entries
    for (i = 0; i < MAX_MAPI; i++) {
        for (j = 0; j < MAX_MAPJ; j++) {
            if (head[i][j].num_entries == 0){
                head[i][j].selected = false;
            }
        }
    }
}

void display_photos(void)
{
    int i, j, num_selected_photos, max_y_top;
    node_t *node;
    int cnt = -1;
    sdlx_texture_t *t;

    // determine number of selected photos
    num_selected_photos = 0;
    for (i = 0; i < MAX_MAPI; i++) {
        for (j = 0; j < MAX_MAPJ; j++) {
            head_t *hd = &head[i][j];
            if (hd->selected) {
                num_selected_photos += hd->num_entries;
            }
        }
    }

    // clamp y_top
    max_y_top = ((num_selected_photos + 1) / 2 - 2) * SPACING;
    if (y_top > max_y_top) y_top = max_y_top;
    if (y_top < 0) y_top = 0;

    // create texture to display the thumb
    t = sdlx_create_texture(THUMB, THUMB);

    // loop over map list heads
    for (i = 0; i < MAX_MAPI; i++) {
        for (j = 0; j < MAX_MAPJ; j++) {
            head_t *hd = &head[i][j];

            // if list head is not selected then continue
            if (!hd->selected) {
                continue;
            }

            // loop over all photos in the list 
            for (node = hd->node.next; node != &hd->node; node = node->next) {
                photo_t *photo = (photo_t*)node;
                metadata_t *md = photo->md;
                sdlx_loc_t dest;
                int y, idx;

                cnt++;

                y = (cnt / 2) * SPACING;
                if (y < y_top - SPACING) {
                    continue;
                }
                if (y > y_top + PHOTOS_H) {
                    break;
                }

                sdlx_set_texture_pixels(t, md->pixels);
                dest.x = (cnt % 2) * SPACING;
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

                sdlx_render_printf_ex2(dest.x, dest.y, FONT_SMALL, COLOR_WHITE, 0, "%d", md->num);
                sdlx_render_printf_ex2(dest.x+THUMB/2, dest.y+THUMB-sdlx_char_height(FONT_SMALL), 
                                       FONT_SMALL, COLOR_WHITE, FLAG_X_CTR, "%s", md->date);
            }
        }
    }

    sdlx_destroy_texture(t);
}

void display_map(void)
{
    int i, j;
    sdlx_loc_t loc;

    sdlx_render_fill_rect(0, MAP_Y, MAP_W, MAP_H, COLOR_BLACK);

    for (i = 0; i < MAX_MAPI; i++) {
        for (j = 0; j < MAX_MAPJ; j++) {
            head_t *hd = &head[i][j];

            if (is_list_empty(&hd->node)) {
                continue;
            }

            loc.w = (double)MAP_W / MAX_MAPI;
            loc.h = (double)MAP_H / MAX_MAPJ;
            loc.x = i * loc.w;
            loc.y = j * loc.h + MAP_Y;
            sdlx_color_t color = (hd->selected ? COLOR_ORANGE : COLOR_WHITE);
            sdlx_render_fill_rect(loc.x+10, loc.y+10, loc.w-20, loc.h-20, color);
            printf("reg event %d\n", EVID_MAP + i + j * MAX_MAPI);
            sdlx_register_event(&loc, EVID_MAP + i + j*MAX_MAPI);
        }
    }

    // if map center has changed then get the name and location of the
    // city nearest to the map center
    // xxx avoid doing this too often when map is in motion
    static double  nearest_city_latitude;
    static double  nearest_city_longitude;
    static char    nearest_city_name[101];
    static double  last_map_latitude_ctr, last_map_longitude_ctr;

    if (map_latitude_ctr != last_map_latitude_ctr || map_longitude_ctr != last_map_longitude_ctr) {
        char city[50], state[50];

        find_nearest_city(map_latitude_ctr, map_longitude_ctr, 
                          city, sizeof(city), state, sizeof(state),
                          &nearest_city_latitude, &nearest_city_longitude);
        snprintf(nearest_city_name, sizeof(nearest_city_name), "%s %s", city, state);
        printf("I %s: nearest city '%s' %0.4f %0.4f\n", 
               progname, nearest_city_name,
               nearest_city_latitude, nearest_city_longitude);

        last_map_latitude_ctr = map_latitude_ctr;
        last_map_longitude_ctr = map_longitude_ctr;
    }

    // if nearest city info is available then display it on the map
    if (nearest_city_name[0] != '\0' && 
        nearest_city_latitude != INVALID_NUMBER &&
        nearest_city_longitude != INVALID_NUMBER)
    {
        // display the nearest_city_name at the bottom of the map
        sdlx_render_printf_ex1(0, MAP_Y+MAP_H-sdlx_char_height(FONT_SMALL), 
                               FONT_SMALL, COLOR_WHITE, 
                               "%s", nearest_city_name);

#if 0
        // display a color blue point on the map at the city location
        // xxx check this
        lat_long_to_map_xy(nearest_city_latitude, nearest_city_longitude, &x, &y);
        printf("XY %d %d\n", x, y);
        if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) {
            sdlx_render_point(x,y+MAP_Y,COLOR_BLUE,MAX_POINT_SIZE);
        }
#endif
    }
}

// -----------------  UTILS  -------------------------------------

// what is distance and true bearing between 42.4222 -71.6226 and 42.4342 -71.6098 

// yyy return error if out of range ?
// yyy comment on what returned x,y are
void lat_long_to_map_xy(double latitude, double longitude, int *x, int *y)
{
    double map_h_miles = map_w_miles * ((double)MAP_H / MAP_W);

    //double ns_miles, ew_miles, bearing;
    //ns_miles = -(map_latitude_ctr - latitude) * 69.17;
    //ew_miles = (longitude - map_longitude_ctr) * (69.17 * cosd(map_latitude_ctr));
    //printf("zzzzzzzzzzzzzzzzzzzz \n");
    //printf("ns_miles = %0.3f  ew_miles = %0.3f\n", ns_miles, ew_miles);
    //printf("total_miles = %0.3f\n", sqrt(ns_miles * ns_miles + ew_miles * ew_miles));
    //bearing = atan2(ns_miles, ew_miles) * 180.0 / M_PI;
    //bearing = 90 - (180/M_PI) * atan(ns_miles / ew_miles);
    //printf("bearing = %0.3f\n", bearing);
    //printf("^^^^^^^^^^^^^^^^^^^^ \n");


    *x = (longitude - map_longitude_ctr) * (69.17 * cosd(map_latitude_ctr)) * (MAP_W / map_w_miles) + (MAP_W/2);
    *y = (map_latitude_ctr - latitude) * (69.17) * (MAP_H / map_h_miles) + (MAP_H/2);


    // yyy use nearbyint
}

double cosd(double degrees)
{
    return cos(degrees * DEG2RAD);
}
