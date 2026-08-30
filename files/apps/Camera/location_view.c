#include "apps/Camera/common.h"

// ------------------ PHOTO LOCATION VIEW --------------

void location_view(void)
{
    sdlx_event_t event;
    int y;
    bool done = false;

    // xxx todo
    // - add pinch event
    while (!done && !end_program) {
        // init the backbuffer to COLOR_BLACK
        sdlx_display_init(COLOR_BLACK, PORTRAIT);

        // register events
        reg_event_show_readme_file();
        y = sdlx_win_height - 1.5 * sdlx_char_height_dflt;
        reg_event_str(0, y, COLOR_LIGHT_BLUE, "CTR", EVID_CTR);
        reg_event_str(sdlx_win_width-4*sdlx_char_width_dflt, y, COLOR_LIGHT_BLUE, "VIEW", EVID_VIEW);
        sdlx_register_event(NULL, EVID_MOTION);
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
        case EVID_SHOW_README_FILE:
            show_file(data_dir, "README");
            break;
        case EVID_STG:
            settings();
            break;
        case EVID_TAKE:
            take_photo();
            break;
        case EVID_QUIT:
            end_program = true;
            break;
        case EVID_CTR:
            break;
        case EVID_VIEW:
            view = GALLERY_VIEW;
            done = true;
            break;
        case EVID_MOTION:
            break;
        }
    }
}
