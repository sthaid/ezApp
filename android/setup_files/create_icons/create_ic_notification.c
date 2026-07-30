#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sdlx.h>
#include <utils.h>
#include <private.h>

#define EVID_OKAY 1

#define WIDTH  400
#define HEIGHT 170
#define Y     -110

#define RES_DIR "../app_src_main_res/"

struct {
    char *filename;
    int icon_size;
} tbl[] = {
    { RES_DIR "drawable-mdpi/ic_notification.png",    24 },
    { RES_DIR "drawable-hdpi/ic_notification.png",    36 },
    { RES_DIR "drawable-xhdpi/ic_notification.png",   48 },
    { RES_DIR "drawable-xxhdpi/ic_notification.png",  72 },
    { RES_DIR "drawable-xxxhdpi/ic_notification.png", 96 },
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

    // create large texture of the proposed notification icon
    t_proposed = sdlx_create_texture(WIDTH, HEIGHT);
    sdlx_set_render_target(t_proposed);
    sdlx_clear_texture(t_proposed, COLOR_PURPLE);
    sdlx_render_printf_ex2(
        WIDTH/2, Y,
        5, COLOR_WHITE, FLAG_X_CTR, "ez");

    // display the proposed notification icon;
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

    // loop, creating notification icon png files with the varying size
    for (i = 0; i < MAX_TBL; i++) {
        char *filename  = tbl[i].filename;
        int   icon_size = tbl[i].icon_size;

        // create target texture of the desired icon_size,
        // and render the large icon texture to this small texture
        t_target = sdlx_create_texture(icon_size, icon_size);
        sdlx_set_render_target(t_target);
        sdlx_render_texture_ex1(t_proposed, 0, 0, icon_size, icon_size);

        // retrieve pixels from the small texture, and
        // write the pixels to png file
        pixels = sdlx_get_texture_pixels(t_target, &w, &h);

        // update pixels:
        // - background must be transparent black
        // - image must be opaque white
        for (int j = 0; j < w*h; j++) {
            pixels[j] = (pixels[j] == COLOR_PURPLE ? 0 : 0xffffffff);
        }

        // write the png file
        printf("creating %-60s %d x %d\n", filename, icon_size, icon_size);
        util_write_png_file(".", filename, (unsigned char *)pixels, w, h);
        free(pixels);
    }

    // done
    sdlx_quit(SUBSYS_VIDEO);
    return 0;
}

