#ifndef GLX_WINDOW_H_
#define	GLX_WINDOW_H_

#include <X11/X.h>

Window _window_init(Display *dpy, int w, int h, int fullscreen);
void _window_buffer_flip();
void _window_destroy();

#endif
