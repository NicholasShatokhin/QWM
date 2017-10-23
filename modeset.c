/*
 * modeset - DRM Modesetting Example
 *
 * Written 2012 by David Herrmann <dh.herrmann@googlemail.com>
 * Dedicated to the Public Domain.
 */

/*
 * DRM Modesetting Howto
 * This document describes the DRM modesetting API. Before we can use the DRM
 * API, we have to include xf86drm.h and xf86drmMode.h. Both are provided by
 * libdrm which every major distribution ships by default. It has no other
 * dependencies and is pretty small.
 *
 * Please ignore all forward-declarations of functions which are used later. I
 * reordered the functions so you can read this document from top to bottom. If
 * you reimplement it, you would probably reorder the functions to avoid all the
 * nasty forward declarations.
 *
 * For easier reading, we ignore all memory-allocation errors of malloc() and
 * friends here. However, we try to correctly handle all other kinds of errors
 * that may occur.
 *
 * All functions and global variables are prefixed with "modeset_*" in this
 * file. So it should be clear whether a function is a local helper or if it is
 * provided by some external library.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <sys/time.h>

struct modeset_dev;
struct modeset_dev {
	struct modeset_dev *next;

	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t size;
	uint32_t handle;
	uint8_t *map;

	drmModeModeInfo mode;
	uint32_t fb;
	uint32_t conn;
	uint32_t crtc;
	drmModeCrtc *saved_crtc;
};

static struct modeset_dev *modeset_list = NULL;

int fd;
uint32_t *ptr;
int main(int argc, char **argv)
{
	int ret;
	const char *card;
	struct modeset_dev *iter;

	/* check which DRM device to open */
	if (argc > 1)
		card = argv[1];
	else
		card = "/dev/dri/card0";

	fprintf(stderr, "using card '%s'\n", card);

	int fd = open("/dev/mem",O_RDWR|O_SYNC);
        if(fd < 0)
        {
                printf("Can't open /dev/mem\n");
                return 1;
        }
        ptr = (uint32_t*)malloc(1024 * 768 * 4);//mmap(0, 1024 * 768 * 4, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x17DF70000);
	mlock(ptr, 1024 * 768 * 4);

	/* draw some colors for 5seconds */
	while(true) {
		modeset_draw();
	}

	ret = 0;

out_close:
	close(fd);
out_return:
	if (ret) {
		errno = -ret;
		fprintf(stderr, "modeset failed with error %d: %m\n", errno);
	} else {
		fprintf(stderr, "exiting\n");
	}
	return ret;
}

/*
 * A short helper function to compute a changing color value. No need to
 * understand it.
 */

static uint8_t next_color(bool *up, uint8_t cur, unsigned int mod)
{
	uint8_t next;

	next = cur + (*up ? 1 : -1) * (rand() % mod);
	if ((*up && next < cur) || (!*up && next > cur)) {
		*up = !*up;
		next = cur;
	}

	return next;
}

/*
 * modeset_draw(): This draws a solid color into all configured framebuffers.
 * Every 100ms the color changes to a slightly different color so we get some
 * kind of smoothly changing color-gradient.
 *
 * The color calculation can be ignored as it is pretty boring. So the
 * interesting stuff is iterating over "modeset_list" and then through all lines
 * and width. We then set each pixel individually to the current color.
 *
 * We do this 50 times as we sleep 100ms after each redraw round. This makes
 * 50*100ms = 5000ms = 5s so it takes about 5seconds to finish this loop.
 *
 * Please note that we draw directly into the framebuffer. This means that you
 * will see flickering as the monitor might refresh while we redraw the screen.
 * To avoid this you would need to use two framebuffers and a call to
 * drmModeSetCrtc() to switch between both buffers.
 * You can also use drmModePageFlip() to do a vsync'ed pageflip. But this is
 * beyond the scope of this document.
 */

void modeset_draw(void)
{
	uint8_t r, g, b, x;
	bool r_up, g_up, b_up;
	unsigned int i, j, k, off;
	struct modeset_dev *iter;
	struct timeval t1, t2;
	double elapsedTime;

	srand(time(NULL));
	r = rand() % 0xff;
	g = rand() % 0xff;
	b = rand() % 0xff;
	r_up = g_up = b_up = true;
	
	uint32_t* base_ptr = ptr;

	r = next_color(&r_up, r, 20);
	g = next_color(&g_up, g, 10);
	b = next_color(&b_up, b, 5);
	x = 0xFF;

	gettimeofday(&t1, NULL);
	for (i = 0; i < 768; ++i) {
		for (int j = 0; j < 1024; ++j) {
			*ptr = 0xFF000000 | (r << 16) | (g << 8) | b;	
			ptr += 1;

		}
	}
	gettimeofday(&t2, NULL);
	elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
	elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
	printf("Loop execution took %lf ms\n", elapsedTime);

	ptr = base_ptr;
	//printf("%x\n r%x g%x b%x, x%x", ptr, *ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3));
}


/*
 * I hope this was a short but easy overview of the DRM modesetting API. The DRM
 * API offers much more capabilities including:
 *  - double-buffering or tripple-buffering (or whatever you want)
 *  - vsync'ed page-flips
 *  - hardware-accelerated rendering (for example via OpenGL)
 *  - output cloning
 *  - graphics-clients plus authentication
 *  - DRM planes/overlays/sprites
 *  - ...
 * If you are interested in these topics, I can currently only redirect you to
 * existing implementations, including:
 *  - plymouth (which uses dumb-buffers like this example; very easy to understand)
 *  - kmscon (which uses libuterm to do this)
 *  - wayland (very sophisticated DRM renderer; hard to understand fully as it
 *             uses more complicated techniques like DRM planes)
 *  - xserver (very hard to understand as it is split across many files/projects)
 *
 * But understanding how modesetting (as described in this document) works, is
 * essential to understand all further DRM topics.
 *
 * Any feedback is welcome. Feel free to use this code freely for your own
 * documentation or projects.
 *
 *  - Hosted on http://github.com/dvdhrm/docs
 *  - Written by David Herrmann <dh.herrmann@googlemail.com>
 */
