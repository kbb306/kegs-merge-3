#include <SDL/SDL.h>
#include "sound_backend.h"

#define AUDIO_BUFFER_SIZE 32768
static Uint8 audio_buffer[AUDIO_BUFFER_SIZE];
static int buffer_len = 0;
static SDL_AudioSpec want;

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    if (buffer_len > 0) {
        int copy_len = len > buffer_len ? buffer_len : len;
        memcpy(stream, audio_buffer, copy_len);
        memmove(audio_buffer, audio_buffer + copy_len, buffer_len - copy_len);
        buffer_len -= copy_len;
    } else {
        memset(stream, 0, len);
    }
}

int sound_init(int sample_rate, int buffer_size) {
    want.freq = sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = buffer_size;
    want.callback = audio_callback;
    want.userdata = NULL;

    if (SDL_OpenAudio(&want, NULL) < 0) return -1;
    SDL_PauseAudio(0);
    return 0;
}

void sound_play(const void *buffer, int len) {
    if (buffer_len + len > AUDIO_BUFFER_SIZE) return;
    memcpy(audio_buffer + buffer_len, buffer, len);
    buffer_len += len;
}

void sound_shutdown() {
    SDL_CloseAudio();
}