#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


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
			int x, y;
			unsigned w, h, bw, depth;

			if (width < 0 && height < 0)
				return child_ret[i];
			XGetGeometry(dpy, (Drawable) child_ret[i], &root_ret, &x, &y, &w, &h, &bw, &depth);
			if (width != w || height != h) {
				if (((maybe = _window_lookup(dpy, child_ret[i], "", width, height)) != child_ret[i])) {
					printf("Match! %s: %ix%i, depth %i\n", wname, w, h, depth);
					return maybe;
				}
			} else {
				printf("Match! 0x%X %s: %ix%i, depth %i\n", (unsigned) child_ret[i], wname, w, h, depth);
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


static void _make_texture(Display *dpy, Window window) {
	Window root_ret;
	int x, y;
	unsigned w, h, bw, depth;
	XImage *img;


	
	printf("Attempting access to window 0x%X\n", (unsigned) window);
	XGetGeometry(dpy, (Drawable) window, &root_ret, &x, &y, &w, &h, &bw, &depth);
	if (!(img = XGetImage(dpy, (Drawable) window, 0, 0, w, h, ~0, ZPixmap)))
		return (void) printf("Unable to get window image");
	
	FILE *fp = fopen("/tmp/scrdmp.raw", "w");
	fwrite(img->data, img->bytes_per_line, img->height, fp);
	fclose(fp);

	XFree(img);
	return;
}


int main(int argc, char **argv) {
	Display *dpy = XOpenDisplay(NULL);
	Window source_window;
	XSetWindowAttributes xswa;
	
	if ((source_window = _window_lookup(dpy, RootWindow(dpy, 0), argv[1], atoi(argv[2]), atoi(argv[3]))) == RootWindow(dpy, 0))
		return 1;
	xswa.backing_store = Always;
	XChangeWindowAttributes(dpy, source_window, CWBackingStore, &xswa);
	sleep(1);
	_make_texture(dpy, source_window);

	return 0;
}
