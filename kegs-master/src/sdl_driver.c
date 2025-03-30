#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef USE_SDL
#include "../../platform_sdl/video_backend.h"
#include "../../platform_sdl/sound_backend.h"
#include "../../platform_sdl/input_backend.h"
#endif

#include "defc.h"
#include "sim65816.h"
#include "engine.h"
#include "video.h"
#include "adb.h"
#include "sound.h"

extern int g_sim65816_running;
extern uint32_t *video_get_framebuffer();

#ifdef USE_SDL
static void handle_key(int keycode, int pressed) {
    if (keycode == SDLK_ESCAPE && pressed) {
        g_sim65816_running = 0;
    } else {
        adb_key_event(keycode, pressed);
    }
}
#endif

int main(int argc, char *argv[]) {
#ifdef USE_SDL
    if (video_init(640, 480) != 0) {
        fprintf(stderr, "Video init failed\n");
        return 1;
    }
    if (sound_init(44100, 1024) != 0) {
        fprintf(stderr, "Sound init failed\n");
        video_shutdown();
        return 1;
    }
#endif

    engine_init(argc, argv);

    g_sim65816_running = 1;
    while (g_sim65816_running) {
#ifdef USE_SDL
        input_poll(handle_key);
#endif
        engine_execute();

#ifdef USE_SDL
        uint32_t *framebuffer = video_get_framebuffer();
        if (framebuffer != NULL) {
            video_update(framebuffer);
        }
#endif
    }

    engine_shutdown();
#ifdef USE_SDL
    sound_shutdown();
    video_shutdown();
#endif
    return 0;
}