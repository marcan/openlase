/*
        OpenLase - a realtime laser graphics toolkit

Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 or version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/* Slides for the 27C3 OpenLase Lightning Talk */

/* NOTE: You need to edit this file with the circlescope audio file (the Super
 * Mario Bros music in the talk) and the tracer video file (a crop of the
 * Bad Apple, see http://www.youtube.com/watch?v=G3C-VevI36s ). You can of
 * course use any other files, though for the tracer you'll likely have to
 * tweak the vparms depending on its complexity. */

#include "libol.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <curses.h>

// 4:3 aspect presentation
#define ASPECT 0.75

typedef void (*init_cb)(void **context, void *arg, OLRenderParams *params);
typedef void (*render_cb)(void *context, float stime, float state);
typedef void (*deinit_cb)(void *context);

typedef struct {
	render_cb render;
	init_cb init;
	deinit_cb deinit;
	void *arg;
	void *context;
} slide_t;

#include "video.h"

video_params vparms = {
	.file = "/home/marcansoft/dwhelper/bad_apple_crop.flv",
	.thresh_dark = 60,
	.thresh_light = 160,
	.sw_dark = 100,
	.sw_light = 160,
	.decimate = 2,
	.volume = 0.5,

	// these just override render parameters
	.min_length = 6,
	.start_wait = 8,
	.end_wait = 3,
	.off_speed = 2.0/24,
	.snap_pix = 4,
	.min_framerate = 29,
};

#define DECLARE_SLIDE(name) \
	void name##_render(void *, float, float); \
	void name##_init(void **, void *, OLRenderParams *); \
	void name##_deinit(void *)

#define SLIDE(name, param) {name##_render, name##_init, name##_deinit, param}

DECLARE_SLIDE(pong);
DECLARE_SLIDE(videotracer);
DECLARE_SLIDE(cubes);
DECLARE_SLIDE(metaballs);
DECLARE_SLIDE(showilda);
DECLARE_SLIDE(paintilda);
DECLARE_SLIDE(diag);
DECLARE_SLIDE(urls);
DECLARE_SLIDE(libol);
DECLARE_SLIDE(circlescope);

slide_t slides[] = {
	SLIDE(paintilda, "openlase-logo.ild"),
	SLIDE(diag, NULL),
	SLIDE(showilda, "jack-logo.ild"),
	SLIDE(showilda, "output.ild"),
	SLIDE(libol, NULL),
	SLIDE(cubes, NULL),
	SLIDE(pong, NULL),
	SLIDE(circlescope, "/home/marcansoft/media/midi/smb1.mp3"),
	SLIDE(metaballs, NULL),
	SLIDE(videotracer, &vparms),
	SLIDE(showilda, "27c3-logo.ild"),
	SLIDE(urls, NULL),
};

OLRenderParams master_params;

#define NUM_SLIDES ((int)(sizeof(slides)/sizeof(slides[0])))

WINDOW *cur_win;

void logger(const char *msg)
{
	wprintw(cur_win, "%s", msg);
	wrefresh(cur_win);
}

/*
 Since we can't directly pipe 2D OpenLase output back into the 3D renderer,
 abuse olTransformVertex3 to perform a second perspective transform.
*/
float cubepos = 0;

// distance from observer to front face of cube (determines FOV)
#define FACEDEPTH 3

void shader_cube(float *x, float *y, uint32_t *color)
{
	olPushMatrix3();
	olLoadIdentity3();

	// Shrink down the entire final image a bit, to avoid cutting off the cube's
	// left and right corners while it's rotating. On a bitmap display (PC) this
	// doesn't matter, but on the laser without shading it looks bad if the edge
	// suddenly scissors off.
	olScale3( 0.9, 0.9, 0.9);

	// This transform sets it up so that it's identity at the cube's front face
	// (z = -4)
	olFrustum(-1, 1, -1, 1, FACEDEPTH, 100);

	// Now figure out the z-position depending on the rotation of the cube, to
	// make the frontmost edge always be full-height.
	float theta = cubepos < 0 ? -cubepos : cubepos;
	float zdelta = -1 - FACEDEPTH + 1 * (sinf(M_PI/4) - sinf(M_PI/4 + theta));
	olTranslate3(0, 0, zdelta);

	// Perform the rotation
	olRotate3Y(cubepos);

	// And finally transform the incoming vertex
	float z = 1;
	olTransformVertex3(x, y, &z);

	olPopMatrix3();
}

#define TRANSITION_TIME 0.75f

float gtime = 0;
int frames = 0;

float cur_stime = 0;
int cur_slide = 0;
float next_stime = 0;
int next_slide = -1;
float trans_time = 0;

void go(int delta)
{
	if (next_slide != -1) {
		//olLog("WARNING: attempted to change slide while transition active\n");
		return;
	}
	next_slide = (cur_slide + NUM_SLIDES + delta) % NUM_SLIDES;
	next_stime = 0;
	trans_time = 0;
	OLRenderParams params = master_params;
	if (slides[next_slide].init) {
		slides[next_slide].init(&slides[next_slide].context, slides[next_slide].arg, &params);
	}
	olSetRenderParams(&params);
}

// this is a function of FACEDEPTH above
#define VANISH 1.33
#define ST_VANISH 1.45

void render_slide(int num, float stime, int bcolor, float state)
{
	int scolor = (VANISH - fabsf(cubepos)) / VANISH * 400;
	if (fabsf(cubepos) < VANISH) {
		olSetPixelShader(NULL);
		olSetVertex3Shader(NULL);
		olLoadIdentity();
		olLoadIdentity3();
		olResetColor();
		if (scolor < 255)
			olMultColor(C_GREY(scolor));
		if (bcolor > 0) {
			// make sure the corners of the slide-rect are always nice and sharp
			// even when the whole frame gets resampled to maintain framerate
			OLRenderParams old, params;
			olGetRenderParams(&old);
			params = old;
			params.start_dwell = 40;
			params.corner_dwell = 40;
			params.end_dwell = 40;
			olSetRenderParams(&params);
			olRect(-1, -ASPECT, 1, ASPECT, C_GREY(bcolor > 255 ? 255 : bcolor));
			olSetRenderParams(&old);
		}

		slides[num].render(slides[num].context, stime, state * ST_VANISH);
	}
}

void render_slides(void)
{
	int bcolor = 0;

	if (next_slide == -1) {
		cubepos = 0;
		render_slide(cur_slide, cur_stime, bcolor, 0);
	} else if (trans_time >= TRANSITION_TIME) {
		if (slides[cur_slide].deinit)
			slides[cur_slide].deinit(slides[cur_slide].context);
		cur_slide = next_slide;
		cur_stime = next_stime;
		cubepos = 0;
		render_slide(cur_slide, cur_stime, bcolor, 0);
		next_slide = -1;
	} else {
		float raw_subpos = trans_time / TRANSITION_TIME;
		// apply a sine function to make it accelerate/delecerate smoothly
		float subpos = (sinf((raw_subpos - 0.5) * M_PI) + 1)/2;

		if (subpos < 0.5)
			bcolor = subpos * 1200;
		else
			bcolor = (1-subpos) * 1200;

		bcolor = bcolor > 0 ? bcolor + 0.3 : 0;

		if (next_slide >= cur_slide)
			cubepos = -subpos * M_PI/2;
		else
			cubepos = subpos * M_PI/2;
		render_slide(cur_slide, cur_stime, bcolor, raw_subpos);

		if (next_slide >= cur_slide)
			cubepos = (1-subpos) * M_PI/2;
		else
			cubepos = (subpos-1) * M_PI/2;
		render_slide(next_slide, next_stime, bcolor, raw_subpos-1);
	}

	float ftime = olRenderFrame(100);
	frames++;
	gtime += ftime;
	cur_stime += ftime;
	next_stime += ftime;
	trans_time += ftime;

	OLFrameInfo info;
	olGetFrameInfo(&info);
	mvprintw(3, 0, "Frame time: %f, Cur FPS: %f, Avg FPS: %f, Objects: %3d, Points: %d", ftime, 1/ftime, frames/gtime, info.objects, info.points);
	if (info.resampled_points)
		printw(", Rp: %4d, Bp: %4d", info.resampled_points, info.resampled_blacks);
	if (info.padding_points)
		printw(", Pad %4d", info.padding_points);
	clrtoeol();
	refresh();
}

WINDOW *misc_win, *render_win;

void reset_term(void)
{
	delwin(misc_win);
	delwin(render_win);
	endwin();
}

#define WINSTART 5

int main (int argc, char *argv[])
{
	initscr();
	cbreak();
	curs_set(0);
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);

	int wh = (LINES - WINSTART)/2;
	
	attron(A_BOLD);
	render_win = newwin(wh-1, COLS, WINSTART + 1, 0);
	scrollok(render_win, TRUE);
	mvprintw(WINSTART, 0, "Render messages:");

	misc_win = newwin(wh-1, COLS, WINSTART + wh + 1, 0);
	scrollok(misc_win, TRUE);
	mvprintw(WINSTART + wh, 0, "Misc messages:");
	attroff(A_BOLD);

	mvprintw(0, 0, "Keys:  NEXT: -> or space  PREV: <-  QUIT: q\n");

	refresh();

	cur_win = misc_win;
	olSetLogCallback(logger);

	memset(&master_params, 0, sizeof master_params);
	master_params.rate = 48000;
	master_params.on_speed = 2.0/100.0;
	master_params.off_speed = 2.0/55.0;
	master_params.start_wait = 12;
	master_params.start_dwell = 3;
	master_params.curve_dwell = 0;
	master_params.corner_dwell = 8;
	master_params.curve_angle = cosf(30.0*(M_PI/180.0)); // 30 deg
	master_params.end_dwell = 3;
	master_params.end_wait = 7;
	master_params.snap = 1/100000.0;
	master_params.max_framelen = master_params.rate / 30;
	master_params.flatness = 0.000001;
	master_params.render_flags = RENDER_GRAYSCALE;

	if(olInit(4, 100000) < 0)
		return 1;

	atexit(reset_term);

	olSetVertexShader(shader_cube);

	olSetScissor (-1, -ASPECT, 1, ASPECT);

	//olLog("\e[6H");
	OLRenderParams params = master_params;
	if (slides[0].init)
		slides[0].init(&slides[0].context, slides[0].arg, &params);
	olSetRenderParams(&params);

	int done = 0;
	while (!done) {
		if (next_slide == -1) {
			mvprintw(1, 0, "Slide %d/%d", cur_slide+1, NUM_SLIDES);
		} else {
			mvprintw(1, 0, "Slide %d/%d -> %d/%d", cur_slide+1, NUM_SLIDES, next_slide+1, NUM_SLIDES);
		}
		clrtoeol();
		refresh();

		fd_set rfds;
		struct timeval tv;
		tv.tv_sec = tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);

		while(1) {
			int ch = getch();
			if (ch == ERR)
				break;
			switch (ch) {
				case KEY_LEFT:
					go(-1);
					break;
				case KEY_RIGHT:
				case ' ':
					go(1);
					break;
				case 'q':
					done = 1;
			}
			
		}

		cur_win = render_win;
		render_slides();
		cur_win = misc_win;
	}

	olShutdown();
	exit (0);
}

