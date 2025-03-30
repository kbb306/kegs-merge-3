#ifndef INPUT_BACKEND_H
#define INPUT_BACKEND_H
int input_poll(void (*on_key)(int keycode, int pressed));
#endif