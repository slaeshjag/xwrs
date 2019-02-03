#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <GL/gl.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "glx_window.h"

static float xscaling, yscaling;
static int off_x, off_y;
static float off_rx, off_ry;

static int native_w, native_h;
static int root_w, root_h;

static void _event_thread(Display *dpy, Window target) {
	XEvent xev;
	while (XPending(dpy)) {
		XNextEvent(dpy, &xev);
		if (XFilterEvent(&xev, None))
			continue;
		XSendEvent(dpy, target, True, KeyPressMask | KeyReleaseMask, &xev);
	}
}


static void _calc_scaling() {
	float mag_x, mag_y;

	mag_x = (float) root_w / native_w;
	mag_y = (float) root_h / native_h;
	if (mag_x > mag_y)
		mag_x = mag_y;
	if (mag_y > mag_x)
		mag_y = mag_x;
	off_x = (root_w - (mag_x * native_w)) / 2;
	off_y = (root_h - (mag_y * native_h)) / 2;
	off_rx = -(-1 + ((float) off_x / root_w)*2);
	off_ry = -(-1 + ((float) off_y / root_h)*2);
	printf("target_w=%i, target_h=%i, off_x=%i, off_y=%i, off_rx=%f, off_ry=%f\n", mag_x*native_w, mag_y*native_h, off_x, off_y, off_rx, off_ry);
	printf("mag_x=%f, mag_y=%f, native_w=%i, native_h=%i\n", mag_x, mag_y, native_w, native_h);
}


static void _window_geometry(Display *dpy, Window win, int *ww, int *wh, int *wb) {
	int x, y;
	unsigned w, h, bw, depth;
	Window root_ret;

	XGetGeometry(dpy, (Drawable) win, &root_ret, &x, &y, &w, &h, &bw, &depth);
	*ww = w;
	*wh = h;
	if (wb)
		*wb = bw;

	return;
}


/* Look for a window where within_name is a substring, and if widith/height > 0, one of that size */
static Window _window_lookup(Display *dpy, Window parent, char *within_name, int width, int height) {
	int i;
	unsigned int nchild;
	Window root_ret, parent_ret, *child_ret, maybe;
	char *wname;

	if (!XQueryTree(dpy, parent, &root_ret, &parent_ret, &child_ret, &nchild))
		return parent;
	for (i = 0; i < nchild; i++) {
		XFetchName(dpy, child_ret[i], &wname);

		if (!*within_name || (wname && strstr(wname, within_name))) {
			unsigned w, h;

			if (width < 0 && height < 0)
				return child_ret[i];
			_window_geometry(dpy, child_ret[i], &w, &h, NULL);
			if (width != w || height != h) {
				if (((maybe = _window_lookup(dpy, child_ret[i], "", width, height)) != child_ret[i]))
					return maybe;
			} else {
				printf("Match! 0x%X %s: %ix%i, depth %i\n", (unsigned) child_ret[i], wname, w, h);
				return child_ret[i];
			}
		} else {
			if ((maybe = _window_lookup(dpy, child_ret[i], within_name, width, height)) != child_ret[i])
				return maybe;
		}

		XFree(wname);
	}

	XFree(child_ret);
	return parent;
}


static unsigned int _init_texture(int w, int h, int sw, int sh) {
	unsigned int texture;
	void *data;

	glEnable(GL_TEXTURE_2D);
	glClearColor(0, 0, 0, 0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glViewport(0, 0, sw, sh);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	data = calloc(1, 4*w*h);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	free(data);

	return texture;
}


static void _make_texture(Display *dpy, Window window, unsigned int texture) {
	Window root_ret;
	int x, y;
	unsigned w, h, bw, depth;
	XImage *img;


	
	//printf("Attempting access to window 0x%X\n", (unsigned) window);
	XGetGeometry(dpy, (Drawable) window, &root_ret, &x, &y, &w, &h, &bw, &depth);
	if (!(img = XGetImage(dpy, (Drawable) window, 0, 0, w, h, ~0, ZPixmap)))
		printf("Unable to get window image"), _window_destroy();

	//printf("Bytes per line: %i (/4=%i)\n", img->bytes_per_line, img->bytes_per_line/4);

	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img->width, img->height, GL_BGRA, GL_UNSIGNED_BYTE, img->data);

	XFree(img->data);
	XFree(img);
	return;
}


static void _draw_texture(unsigned int texture) {
	float vertex[12] = { -off_rx, -off_ry, off_rx, -off_ry, off_rx, off_ry, off_rx, off_ry, -off_rx, off_ry, -off_rx, -off_ry };
	float texcoord[12] = { 0, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1 };

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	
	glBindTexture(GL_TEXTURE_2D, texture);
	glVertexPointer(2, GL_FLOAT, 8, vertex);
	glTexCoordPointer(2, GL_FLOAT, 8, texcoord);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	
	_window_buffer_flip();
	glClear(GL_COLOR_BUFFER_BIT);
}


int main(int argc, char **argv) {
	Display *dpy = XOpenDisplay(NULL);
	Window source_window, dest_window;
	XSetWindowAttributes xswa;
	unsigned int texture;
	int w, h, sw, sh, wb;
	
	w = atoi(argv[2]);
	h = atoi(argv[3]);
	
	if ((source_window = _window_lookup(dpy, RootWindow(dpy, 0), argv[1], w, h)) == RootWindow(dpy, 0))
		return 1;
	
	xswa.backing_store = Always;
	XChangeWindowAttributes(dpy, source_window, CWBackingStore, &xswa);
	_window_geometry(dpy, source_window, &native_w, &native_h,  NULL);
	_window_geometry(dpy, RootWindow(dpy, 0), &root_w, &root_h, NULL);

	sw = w*3;
	sh = h*3;

	dest_window = _window_init(dpy, root_w, root_h, 0);
	_window_geometry(dpy, dest_window, &sw, &sh, &wb);
	XMoveWindow(dpy, dest_window, -wb, -wb);
	texture = _init_texture(w, h, root_w, root_h);
	_calc_scaling();
	sleep(1);
	for (;;) {
		_event_thread(dpy, source_window);
		_make_texture(dpy, source_window, texture);
		_draw_texture(texture);
	}

	return 0;
}
