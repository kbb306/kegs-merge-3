#ifndef SOUND_BACKEND_H
#define SOUND_BACKEND_H
int sound_init(int sample_rate, int buffer_size);
void sound_play(const void *buffer, int len);
void sound_shutdown();
#endif