#include <SDL/SDL.h>
#include "input_backend.h"

int input_poll(void (*on_key)(int keycode, int pressed)) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            on_key(SDLK_ESCAPE, 1);
        } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            on_key(e.key.keysym.sym, e.type == SDL_KEYDOWN);
        }
    }
    return 0;
}