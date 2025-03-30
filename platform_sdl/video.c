#include <SDL/SDL.h>
#include "video_backend.h"
//#include "video.h"

static SDL_Surface *screen = NULL;
static int window_width = 640;
static int window_height = 480;
typedef struct {
    uint8_t *data_ptr;
} Kimage;

extern Kimage g_mainwin_kimage;

uint32_t* video_get_framebuffer() {
    return (uint32_t*)g_mainwin_kimage.data_ptr;
}

int video_init(int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0) {
        return -1;
    }
    window_width = width;
    window_height = height;
    screen = SDL_SetVideoMode(window_width, window_height, 32, SDL_SWSURFACE);
    if (!screen) return -1;
    SDL_WM_SetCaption("KEGS SDL", NULL);
    return 0;
}

void video_update(const void *pixels) {
    if (!screen) return;
    memcpy(screen->pixels, pixels, window_width * window_height * 4);
    SDL_UpdateRect(screen, 0, 0, 0, 0);
}

void video_shutdown() {
    SDL_Quit();
}
