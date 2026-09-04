#include "apps/Camera/common.h"

// ------------------ PHOTO GALLERY VIEW ---------------

void gallery(void)
{
    sdlx_event_t    event;
    int             x, y, y_last;
    sdlx_texture_t *t;
    sdlx_loc_t      dest;
    double          y_top = 0;
    bool            del_mode = false;
    bool            switch_view = false;

    // init
    t = sdlx_create_texture(THUMB, THUMB);

    while (!switch_view && !end_program) {
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

        sdlx_register_control_events(EVID_STG, "Stg", EVID_TAKE, UNICODE_CIRCLE, EVID_QUIT, "X");

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
            case EVID_QUIT:
                end_program = true;
                break;
            case EVID_SHOW_README_FILE:
                show_file(data_dir, "README");
                break;
            case EVID_TAKE:
                take_photo();
                y_last = (max_photos == 0 ? 0 : ((max_photos + 1) / 2 - 1) * SPACING);
                y_top = y_last - 2 * SPACING;
                break;
            case EVID_DEL:
                del_mode = !del_mode;
                break;
            case EVID_MOTION:
                y_top -= event.u.motion.yrel;
                break;
            case EVID_HOME:
                y_top = 0;
                break;
            case EVID_END:
                y_last = (max_photos == 0 ? 0 : ((max_photos + 1) / 2 - 1) * SPACING);
                y_top = y_last - 2 * SPACING;
                break;
            case EVID_PGUP:
                y_top -= (3 * SPACING);
                break;
            case EVID_PGDN:
                y_top += (3 * SPACING);
                break;
            case EVID_VIEW:
                view = LOCATION_VIEW;
                switch_view = true;
                break;
            case EVID_STG:
                settings();
                break;
            }

            // limit the min/max value of y_top 
            y_last = (max_photos == 0 ? 0 : ((max_photos + 1) / 2 - 1) * SPACING);
            if (y_top > y_last - 2 * SPACING) y_top = y_last - 2 * SPACING;
            if (y_top < 0) y_top = 0;
        }
    }

    sdlx_destroy_texture(t);
}
