#include "apps/Camera/common.h"

#define DEG_TO_RAD (M_PI / 180.0)

#define MAX_MAP 7

double  latitude;
double  longitude;
char    city[100];
char    state[100];
double  w_miles;
node_t  Head[MAX_MAP][MAX_MAP];  // xxx global?

void proc(void);
void display_map(void);
void display_photos(void);

// ------------------ LOCATION VIEW --------------

void location(void)
{
    sdlx_event_t event;
    int          y;
    bool         done = false;

    latitude = INVALID_NUMBER;
    longitude = INVALID_NUMBER;
    city[0] = '\0';
    state[0] = '\0';
    w_miles = 1;

    // xxx todo
    // - add pinch event
    while (!done && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        proc();
        display_photos();
        display_map();

        //make_list_of_photos_on_map();
        //display_map();
        //display_photos();

        // register events
        reg_event_show_readme_file();

        int ctrls_h = 300;
        y = sdlx_win_height - ctrls_h + (150 - sdlx_char_height_dflt) / 2;
        // xxx todo
        y += 150;
        reg_event_str(0, y, COLOR_LIGHT_BLUE, "Ctr", EVID_CTR);
        reg_event_str(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "View", EVID_VIEW);  // xxx use same as in gallery

        //sdlx_register_event(NULL, EVID_MOTION);
        sdlx_register_control_events(EVID_STG, "STG", EVID_TAKE, TAKE, EVID_QUIT, "X");

        // present the display
        sdlx_display_present();

        // wait for event, with 1 sec timeout
        sdlx_get_event(ONE_SEC, &event);
        if (event.event_id == -1) {
            continue;
        }

        // process events
        switch (event.event_id) {
        case EVID_QUIT:
            end_program = true;
            break;
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_VIEW:
            view = GALLERY_VIEW;
            done = true;
            break;
        case EVID_STG:
            settings();
            break;

#if 0 //xxx todo
        case EVID_TAKE:
            take_photo();
            break;
        case EVID_CTR:
            break;
        case EVID_MOTION:
            break;
#endif
        }
    }
}

void proc(void)
{
    int i, j, idx;
    double cos_lat, x_miles, y_miles;
    metadata_t *md;

    if (latitude == INVALID_NUMBER || longitude == INVALID_NUMBER) {
        // xxx or every minute
        util_get_location(&latitude, &longitude, NULL, NULL);
        find_nearest_city(latitude, longitude, 
                          city, sizeof(city), 
                          state, sizeof(state));
        printf("I %s: lat,long=%0.3f %0.3f  city,state=%s %s\n", progname, latitude, longitude, city, state);
    }

    for (i = 0; i < MAX_MAP; i++) {
        for (j = 0; j < MAX_MAP; j++) {
            init_list_head(&Head[i][j]);
        }
    }

    cos_lat = cos(latitude * DEG_TO_RAD);
    for (idx = 0; idx < max_photos; idx++) {
        md = photos[idx].md;

        y_miles = fabs(md->latitude - latitude) * 69.17;
        x_miles = fabs(md->longitude - longitude) * 69.17 * cos(latitude * DEG_TO_RAD);
        printf("%f %f\n", x_miles, y_miles);
        i = nearbyint((x_miles + w_miles/2) / w_miles * MAX_MAP - 0.5);
        j = nearbyint((y_miles + w_miles/2) / w_miles * MAX_MAP - 0.5);
        printf("%d %d\n", i, j);
        if (i < 0 || i >= MAX_MAP || j < 0 || j >= MAX_MAP) {
            continue;
        }

        add_to_list_tail(&Head[i][j], &photos[idx].node);
    }

#if 0
    for (i = 0; i < MAX_MAP; i++) {
        for (j = 0; j < MAX_MAP; j++) {
            node_t *head = &Head[i][j];
            node_t *node;
            photo_t *photo;

            if (is_list_empty(head)) {
                continue;
            }

            printf("list %d %d is not empty\n", i, j);
            for (node = head->next; node != head; node = node->next) {
                photo = (photo_t*)node;
                printf("  num %d\n", photo->num);
            }
        }
    }
#endif
}

void display_map(void)
{
    int i, j;
    sdlx_loc_t loc;

    for (i = 0; i < MAX_MAP; i++) {
        for (j = 0; j < MAX_MAP; j++) {
            node_t *head = &Head[i][j];

            if (is_list_empty(head)) {
                continue;
            }

#if 0
            photo_t *photo;
            node_t *node;
            printf("list %d %d is not empty\n", i, j);
            for (node = head->next; node != head; node = node->next) {
                photo = (photo_t*)node;
                printf("  num %d\n", photo->num);
            }
#endif

            //double k = sdlx_win_width / 20.0;
            //x = i * k;  // xxx check the location of the box, probably just use render rect
            //y = 1000 - (j + 1) * k;
            //sdlx_render_printf_ex2(x, y, FONT_NORMAL, COLOR_WHITE, 0, "\u2588");  // full block character

            loc.w = sdlx_win_width / MAX_MAP;
            loc.h = loc.w;
            loc.x = i * loc.w;
            loc.y = j * loc.h;
            sdlx_render_fill_rect(loc.x+10, loc.y+10, loc.w-20, loc.h-20, COLOR_WHITE);
        }
    }
}

void display_photos(void)
{
    // if no location chosen then display all that are within map area
}
