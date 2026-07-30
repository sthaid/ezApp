#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sdlx.h>
#include <utils.h>
#include <private.h>

#define EVID_OKAY 1

#define PROPOSED_ICON_SIZE 512

#define RES_DIR "../app_src_main_res/"

struct {
    char *filename;
    int icon_size;
} tbl[] = {
    { RES_DIR "mipmap-mdpi/ic_launcher.png",     48 },
    { RES_DIR "mipmap-hdpi/ic_launcher.png",     72 },
    { RES_DIR "mipmap-xhdpi/ic_launcher.png",    96 },
    { RES_DIR "mipmap-xxhdpi/ic_launcher.png",  144 },
    { RES_DIR "mipmap-xxxhdpi/ic_launcher.png", 192 },
//  { "icon_512x512.png",                       512 },
                };

#define MAX_TBL (sizeof(tbl) / sizeof(tbl[0]))

int main(int argc, char **argv)
{
    int              rc, w, h, i;
    sdlx_texture_t  *t_proposed, *t_target;
    unsigned int    *pixels;
    sdlx_event_t     event;
    bool             okay = false;

    // init sdl video
    rc = sdlx_init(SUBSYS_VIDEO);

    // create texture of the proposed launcher icon
    t_proposed = sdlx_create_texture(PROPOSED_ICON_SIZE, PROPOSED_ICON_SIZE);
    sdlx_set_render_target(t_proposed);
    sdlx_clear_texture(t_proposed, COLOR_PURPLE);
    sdlx_render_printf_ex2(
        PROPOSED_ICON_SIZE/2, PROPOSED_ICON_SIZE/2 - sdlx_char_height(5) * 0.10,
        5, COLOR_WHITE, FLAG_XY_CTR, "ez");

    // display the proposed launcher icon;
    // user selects 'OK' to proceed with generating the png files
    while (!okay) {
        sdlx_display_init(COLOR_BLACK, PORTRAIT);
        sdlx_render_texture(t_proposed, 0, 0);
        sdlx_register_control_events(EVID_OKAY, "OK", 0, NULL, EVID_QUIT, "X");
        sdlx_display_present();
        sdlx_get_event(-1, &event);
        switch (event.event_id) {
        case EVID_OKAY:
            okay = true;
            break;
        case EVID_QUIT:
            printf("ABORTING\n");
            sdlx_quit(SUBSYS_VIDEO);
            return 1;
        }
    }

    // loop, creating icon png files with varying size
    for (i = 0; i < MAX_TBL; i++) {
        char *filename  = tbl[i].filename;
        int   icon_size = tbl[i].icon_size;

        // create small texture of the desired icon_size,
        // and render the large icon texture to this small texture
        t_target = sdlx_create_texture(icon_size, icon_size);
        sdlx_set_render_target(t_target);
        sdlx_render_texture_ex1(t_proposed, 0, 0, icon_size, icon_size);

        // retrieve pixels from the small texture, and
        // write the pixels to png file
        pixels = sdlx_get_texture_pixels(t_target, &w, &h);
        printf("creating %-60s %d x %d\n", filename, icon_size, icon_size);
        util_write_png_file(".", filename, (unsigned char *)pixels, w, h);
        free(pixels);
    }

    // done
    sdlx_quit(SUBSYS_VIDEO);
    return 0;
}

