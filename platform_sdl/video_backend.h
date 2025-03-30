#ifndef VIDEO_BACKEND_H
#define VIDEO_BACKEND_H
int video_init(int width, int height);
void video_update(const void *pixels);
void video_shutdown();
uint32_t* video_get_framebuffer();
#endif