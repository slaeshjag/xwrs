/*
Copyright (c) 2015 Steven Arnow <s@rdw.se>
'glx_window.c' - This file is part of libdangit

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.

	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.

	3. This notice may not be removed or altered from any source
	distribution.
*/

#include <stdint.h>
#include <stdbool.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <stdio.h>

static struct {
	Display	*dpy;
	GLXContext ctx;
	Window	win;
} window;

void __input_init(void *dpy, Window win);

Window _window_init(Display *dpy, int w, int h, int fullscreen) {
	Window rwin, main_win;
	XVisualInfo *xv;
	Colormap cm;
	XSetWindowAttributes xswa;
	GLXContext glc;
	int cd, attrib[16], i = 0;
	XSizeHints sh;

	rwin = DefaultRootWindow(dpy);
	cd = 8;
	attrib[i++] = GLX_RGBA;
	attrib[i++] = GLX_DOUBLEBUFFER;
	attrib[i++] = GLX_RED_SIZE;
	attrib[i++] = cd;
	attrib[i++] = GLX_GREEN_SIZE;
	attrib[i++] = cd;
	attrib[i++] = GLX_BLUE_SIZE;
	attrib[i++] = cd;
	attrib[i++] = None;

	if (!(xv = glXChooseVisual(dpy, 0, attrib))) {
		printf("No visual\n");
		goto badvisual;
	}
	
	cm = XCreateColormap(dpy, rwin, xv->visual, AllocNone);
	xswa.colormap = cm;
	xswa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask;
	
	if (!(main_win = XCreateWindow(dpy, rwin, 0, 0, w, h, 0, xv->depth, InputOutput, xv->visual, CWColormap | CWEventMask, &xswa))) {
		printf("no window\n");
		goto nowindow;
	}

	sh.min_width = sh.max_width = sh.base_width = w;
	sh.min_height = sh.max_height = sh.base_height = h;
	sh.flags = PBaseSize | PMinSize | PMaxSize;
	XSetWMNormalHints(dpy, main_win, &sh);

	XMapWindow(dpy, main_win);
	XStoreName(dpy, main_win, "Fullscreen");
	if (!(glc = glXCreateContext(dpy, xv, NULL, GL_TRUE))) {
		printf("glX was unable to create the OpenGL Context\n");
		goto nocontext;
	}

	glXMakeCurrent(dpy, main_win, glc);

	XFree(xv);
	window.dpy = dpy;
	window.win = main_win;
	window.ctx = glc;
	return main_win;
	
	// Error handlers below
nocontext:
	XDestroyWindow(dpy, main_win); 
nowindow:
	XFreeColormap(dpy, cm);
	XFree(xv);
badvisual:
	return RootWindow(dpy, 0);
}


void _window_buffer_flip() {
	glXSwapBuffers(window.dpy, window.win);
}


void _window_destroy() {
	glXDestroyContext(window.dpy, window.ctx);
	XDestroyWindow(window.dpy, window.win);
	XCloseDisplay(window.dpy);
	return;
}
