// xxx todo
// - pan and pinch map

#include "apps/Camera/common.h"

#define DEG_TO_RAD (M_PI / 180.0)

#define MAX_MAPI 7
#define MAX_MAPJ 5

#define EVID_LOC  1000 //xxx move

#define BOLTON_MASS_LATITUDE     42.4334 //xxx temp
#define BOLTON_MASS_LONGITUDE   -71.6078

typedef struct {
    node_t node;
    bool   selected;
    int    num_entries;
} head_t;

double  latitude_ctr;
double  longitude_ctr;
double  latitude_nearest_city;
double  longitude_nearest_city;
char    city[100];
char    state[100];
double  w_miles;
double  h_miles;
head_t  head[MAX_MAPI][MAX_MAPJ];  // xxx global?
bool    del_mode = false;
double y_top;
int num_displayed_photos;

void proc(void);
void display_map(void);
void display_photos(void);

// ------------------ LOCATION VIEW --------------

void location(void)
{
    sdlx_event_t event;
    int          y;
    bool         switch_view = false;

    latitude_ctr = INVALID_NUMBER; // xxx always choose valid
    longitude_ctr = INVALID_NUMBER;
    city[0] = '\0';
    state[0] = '\0';
    w_miles = 1;
    h_miles = 0.7;

    y_top = 0;
    memset(head, 0, sizeof(head));
    // xxx other inits

    // xxx todo
    // - add pinch event
    while (!switch_view && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        proc();
        num_displayed_photos = 0;
        display_photos();
        display_map();

        //make_list_of_photos_on_map();
        //display_map();
        //display_photos();

#if 0
        int ctrls_h = 300;
        y = sdlx_win_height - ctrls_h + (150 - sdlx_char_height_dflt) / 2;
        // xxx todo,  // xxx use same as in gallery
        y += 150;
        reg_event_str(0, y, COLOR_LIGHT_BLUE, "Ctr", EVID_CTR); //xxx what is this for
        reg_event_str(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "View", EVID_VIEW);
        sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_STG, "STG", EVID_TAKE, TAKE, EVID_QUIT, "X");
#endif
        // register events
        reg_event_show_readme_file();

        int ctrls_h = 300;
        reg_event_fill_rect(0, sdlx_win_height-ctrls_h, sdlx_win_width, ctrls_h, COLOR_BLACK, EVID_NOOP);

        y = sdlx_win_height - 250; //xxx make same in gallery
        reg_event_str(COL2X(0), y, COLOR_LIGHT_BLUE, "Home", EVID_HOME);
        reg_event_str(COL2X(7), y, COLOR_LIGHT_BLUE, "Up", EVID_PGUP);
        reg_event_str(COL2X(12), y, COLOR_LIGHT_BLUE, "Dn", EVID_PGDN);
        reg_event_str(COL2X(17), y, COLOR_LIGHT_BLUE, "End", EVID_END);

        y += 130;
        reg_event_str(0, y, COLOR_LIGHT_BLUE, "Del", EVID_DEL);
        reg_event_str(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "View", EVID_VIEW);

        sdlx_register_event(NULL, EVID_MOTION);

        sdlx_register_control_events(EVID_STG, "Stg", EVID_TAKE, TAKE, EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with 1 sec timeout
        sdlx_get_event(5*ONE_SEC, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        if (event.event_id >= EVID_LOC && event.event_id < EVID_LOC+(MAX_MAPI*MAX_MAPJ)) {
            int i,j; //xxx check they are in range
            head_t *hd;

            printf("GOT EVENT ID %d\n", event.event_id - EVID_LOC);
            i = (event.event_id - EVID_LOC) / MAX_MAPI;
            j = (event.event_id - EVID_LOC) % MAX_MAPI;
            printf("GOT EVENT   %d %d\n", i,j);
            hd = &head[i][j];
            hd->selected = !hd->selected;
        } else if (event.event_id >= EVID_SHOW_PHOTO && event.event_id < EVID_SHOW_PHOTO+max_photos) {
            int idx = event.event_id - EVID_SHOW_PHOTO; 
            show_photo(idx);
        } else if (event.event_id >= EVID_DELETE_PHOTO && event.event_id < EVID_DELETE_PHOTO+max_photos) {
            int idx = event.event_id - EVID_DELETE_PHOTO;
            delete_photo(idx);
        } else {
            int y_last;

            switch (event.event_id) {
            case EVID_QUIT:
                end_program = true;
                break;
            case EVID_SHOW_README_FILE:
                show_file(data_dir, "README");
                break;
            case EVID_TAKE:
                take_photo();
                //y_last = (num_displayed_photos == 0 ? 0 : ((num_displayed_photos + 1) / 2 - 1) * SPACING);
                //y_top = y_last - 2 * SPACING;
                //y_top = 1e99;
                break;
            case EVID_DEL:
                del_mode = !del_mode;
                break;
            case EVID_MOTION:
                printf("MOTION y_top %0.3f\n", y_top);
                y_top -= event.u.motion.yrel;
                break;
            case EVID_HOME:
                y_top = 0;
                break;
            case EVID_END:
                //y_last = (num_displayed_photos == 0 ? 0 : ((num_displayed_photos + 1) / 2 - 1) * SPACING);
                //y_top = y_last - 2 * SPACING;
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

            // limit the min/max value of y_top 
            y_last = (num_displayed_photos == 0 
                      ? 0 
                      : ((num_displayed_photos + 1) / 2 - 1) * SPACING);
            printf("num_displayed %d  y_last=%d\n", num_displayed_photos, y_last);
            if (y_top > y_last - SPACING) y_top = y_last - SPACING;
            if (y_top < 0) y_top = 0;
            printf("y_top is now %0.0f\n", y_top);
        }
    }
}

void lat_long_to_map_pixel_coord(double latitude, double longitude, int *x, int *y)
{
    *x = (longitude - longitude_ctr) * 
         (69.17 * cos(latitude_ctr * DEG_TO_RAD)) / 
         (w_miles / 2) * 1000 + 500;
    *y = (latitude_ctr - latitude) * 
         (69.17) / 
         (h_miles / 2) * 700 + 350;
    // xxx nearbyint
}


void proc(void)
{
    int i, j, idx, x, y;
    //double cos_lat, x_miles, y_miles;
    metadata_t *md;

    if (latitude_ctr == INVALID_NUMBER || longitude_ctr == INVALID_NUMBER) {
        // xxx or every minute
        util_get_location(&latitude_ctr, &longitude_ctr, NULL, NULL);
        find_nearest_city(latitude_ctr, longitude_ctr, 
                          city, sizeof(city), 
                          state, sizeof(state));
        latitude_nearest_city = BOLTON_MASS_LATITUDE;
        longitude_nearest_city = BOLTON_MASS_LONGITUDE;
        printf("I %s: lat,long=%0.3f %0.3f  city,state=%s %s\n", progname, latitude_ctr, longitude_ctr, city, state);
    }

    for (i = 0; i < MAX_MAPI; i++) {
        for (j = 0; j < MAX_MAPJ; j++) {
            head[i][j].num_entries = 0;
            init_list_head(&head[i][j].node);
        }
    }

    //double cos_lat_ctr = cos(latitude_ctr * DEG_TO_RAD);
    for (idx = 0; idx < max_photos; idx++) {
        md = photos[idx].md;

        lat_long_to_map_pixel_coord(md->latitude, md->longitude, &x, &y);

        i = x / (1000.0 / MAX_MAPI);
        j = y / (700.0 / MAX_MAPJ);

        printf("%d %d  got i j %d %d\n", x,y,i,j);
        
        if (i < 0 || i >= MAX_MAPI || j < 0 || j >= MAX_MAPJ) {
            continue;
        }

        add_to_list_tail(&head[i][j].node, &photos[idx].node);
        head[i][j].num_entries++;
    }

    for (i = 0; i < MAX_MAPI; i++) {
        for (j = 0; j < MAX_MAPJ; j++) {
            if (head[i][j].num_entries == 0){
                head[i][j].selected = false;
            }
        }
    }
}

void display_map(void)
{
    int i, j, x, y;
    sdlx_loc_t loc;

    sdlx_render_fill_rect(0, 0, 1000, 700, COLOR_BLACK);

    for (i = 0; i < MAX_MAPI; i++) {
        for (j = 0; j < MAX_MAPJ; j++) {
            head_t *hd = &head[i][j];

            if (is_list_empty(&hd->node)) {
                continue;
            }

            loc.w = 1000.0 / MAX_MAPI;
            loc.h = 700.0 / MAX_MAPJ;
            loc.x = i * loc.w;
            loc.y = j * loc.h;
            sdlx_color_t color = (hd->selected ? COLOR_ORANGE : COLOR_WHITE);
            sdlx_render_fill_rect(loc.x+10, loc.y+10, loc.w-20, loc.h-20, color);
            printf("Registering %d\n", EVID_LOC + i*MAX_MAPI + j);
            sdlx_register_event(&loc, EVID_LOC + i*MAX_MAPI + j);
        }
    }

    // xxx ctr if string not too long, else dont center
    sdlx_render_printf_ex1(0, 700-sdlx_char_height(FONT_SMALL), FONT_SMALL, COLOR_WHITE, "%s %s", city, state);

    lat_long_to_map_pixel_coord(latitude_nearest_city, longitude_nearest_city, &x, &y);
    sdlx_render_point(x,y,COLOR_BLUE,MAX_POINT_SIZE);
}

void display_photos(void)
{
    int i, j;
    node_t *node;
    int cnt = -1;
    sdlx_texture_t *t;

    t = sdlx_create_texture(THUMB, THUMB);

    for (i = 0; i < MAX_MAPI; i++) {
        for (j = 0; j < MAX_MAPJ; j++) {
            head_t *hd = &head[i][j];

            if (!hd->selected) {
                continue;
            }
            //printf("selected %d %d  num_entries=%d\n", i, j, hd->num_entries);

            num_displayed_photos += hd->num_entries;

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
                if (y > y_top + 1300) {
                    break;
                }


                sdlx_set_texture_pixels(t, md->pixels);
                dest.x = (cnt % 2) * SPACING;
                dest.y = y - y_top + 700;
                dest.w = THUMB;
                dest.h = THUMB;
                sdlx_render_texture(t, NULL, &dest);

                idx = ((char*)photo - (char*)&photos[0]) / sizeof(photo_t); //xxx comment picoc issue
                sdlx_register_event(&dest, EVID_SHOW_PHOTO+idx);

                if (del_mode) {
                    reg_event_str(dest.x + THUMB - sdlx_char_width_dflt, dest.y,
                                  COLOR_RED, "X", EVID_DELETE_PHOTO+idx);
                }

// xxx del mode

                //int tmp_x = ((dest.x == 0 && dest.y == 0) ? 40 : dest.x);
                sdlx_render_printf_ex2(dest.x, dest.y, FONT_SMALL, COLOR_WHITE, 0, "%d", md->num);
                sdlx_render_printf_ex2(dest.x+THUMB/2, dest.y+THUMB-sdlx_char_height(FONT_SMALL), 
                                       FONT_SMALL, COLOR_WHITE, FLAG_X_CTR, "%s", md->date);
            }
        }
    }

    sdlx_destroy_texture(t);
}


#if 0
    sdlx_event_t    event;
    int             x, y, y_last;
    sdlx_texture_t *t;
    sdlx_loc_t      dest;
    double          y_top = 0;


    while (!done && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // xxx todo
        // xxx optimize loop start
        // xxx comment
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

        int ctrls_h = 300;
        reg_event_fill_rect(0, sdlx_win_height-ctrls_h-25, sdlx_win_width, ctrls_h+25, COLOR_BLACK, EVID_NOOP);

        y = sdlx_win_height - ctrls_h + (150 - sdlx_char_height_dflt) / 2;
        reg_event_str(COL2X(0), y, COLOR_LIGHT_BLUE, "Home", EVID_HOME);
        reg_event_str(COL2X(7), y, COLOR_LIGHT_BLUE, "Up", EVID_PGUP);
        reg_event_str(COL2X(12), y, COLOR_LIGHT_BLUE, "Dn", EVID_PGDN);
        reg_event_str(COL2X(17), y, COLOR_LIGHT_BLUE, "End", EVID_END);

        y += 150;
        reg_event_str(0, y, COLOR_LIGHT_BLUE, "Del", EVID_DEL);
        reg_event_str(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "View", EVID_VIEW);

        sdlx_register_event(NULL, EVID_MOTION);

        sdlx_register_control_events(EVID_STG, "Stg", EVID_TAKE, TAKE, EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for an event, with 1 sec timeout
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
            }

            // limit the min/max value of y_top 
            y_last = (max_photos == 0 ? 0 : ((max_photos + 1) / 2 - 1) * SPACING);
            if (y_top > y_last - 2 * SPACING) y_top = y_last - 2 * SPACING;
            if (y_top < 0) y_top = 0;
        }
    }

    sdlx_destroy_texture(t);
#endif

