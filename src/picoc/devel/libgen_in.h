// example
sdlx_texture_t *sdlx_create_texture(int w, int h);
void sdlx_destroy_texture(sdlx_texture_t *t);
void sdlx_query_texture(sdlx_texture_t *t, int *w, int *h);
void sdlx_clear_texture(sdlx_texture_t *t, sdlx_color_t color);

void sdlx_set_texture_pixels(sdlx_texture_t *t, unsigned int *pixels);
unsigned int *sdlx_get_texture_pixels(sdlx_texture_t *t, int *w, int *h);

void sdlx_render_texture(sdlx_texture_t *t, int x, int y);
void sdlx_render_texture_ex1(sdlx_texture_t *t, int x, int y, int w, int h);
void sdlx_render_texture_ex2(sdlx_texture_t *t, int x, int y, int w, int h, double angle);
void sdlx_render_texture_ex3(sdlx_texture_t *texture, int x, int y, int w, int h, double angle, int xctr, int yctr);

void sdlx_set_render_target(sdlx_texture_t *t);

