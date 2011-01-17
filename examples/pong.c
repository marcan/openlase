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

#include "libol.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <jack/jack.h>
#include <math.h>

#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DIGW 0.06
#define DIGH 0.12

/*
Pong! Input is yet another hack. It's meant to be used with one or two PS3
controllers connected via event devices. Dealing with permissions is left as
an exercise to the reader. Will probably work with other similar input devices.

Also, the digit code dates to before I had fonts going. However, the default
font currently has no digits, so it has a reason to exist for now.
*/

void digit(float x, float y, int d, uint32_t color)
{
	olPushMatrix();
	olTranslate(x,y);
	olScale(DIGW*0.8,DIGH*0.4);
	olBegin(OL_LINESTRIP);
	switch(d) {
		case 0:
			olVertex(0,0,color);
			olVertex(0,2,color);
			olVertex(1,2,color);
			olVertex(1,0,color);
			olVertex(0,0,color);
			break;
		case 1:
			olVertex(0.5,0,color);
			olVertex(0.5,2,color);
			break;
		case 2:
			olVertex(0,0,color);
			olVertex(1,0,color);
			olVertex(1,1,color);
			olVertex(0,1,color);
			olVertex(0,2,color);
			olVertex(1,2,color);
			break;
		case 3:
			olVertex(0,0,color);
			olVertex(1,0,color);
			olVertex(1,1,color);
			olVertex(0,1,color);
			olVertex(1,1,C_BLACK);
			olVertex(1,2,color);
			olVertex(0,2,color);
			break;
		case 4:
			olVertex(0,0,color);
			olVertex(0,1,color);
			olVertex(1,1,color);
			olVertex(1,0,C_BLACK);
			olVertex(1,2,color);
			break;
		case 5:
			olVertex(1,0,color);
			olVertex(0,0,color);
			olVertex(0,1,color);
			olVertex(1,1,color);
			olVertex(1,2,color);
			olVertex(0,2,color);
			break;
		case 6:
			olVertex(1,0,color);
			olVertex(0,0,color);
			olVertex(0,2,color);
			olVertex(1,2,color);
			olVertex(1,1,color);
			olVertex(0,1,color);
			break;
		case 7:
			olVertex(0,0,color);
			olVertex(1,0,color);
			olVertex(1,2,color);
			break;
		case 8:
			olVertex(0,0,color);
			olVertex(0,2,color);
			olVertex(1,2,color);
			olVertex(1,0,color);
			olVertex(0,0,color);
			olVertex(0,1,C_BLACK);
			olVertex(1,1,color);
			break;
		case 9:
			olVertex(1,1,color);
			olVertex(0,1,color);
			olVertex(0,0,color);
			olVertex(1,0,color);
			olVertex(1,2,color);
			break;
	}
	olEnd();
	olPopMatrix();
}

int main (int argc, char *argv[])
{
	OLRenderParams params;

	int in1_fd;
	int in2_fd;

	in1_fd = open(argv[1], O_RDONLY | O_NONBLOCK);
	in2_fd = open(argv[2], O_RDONLY | O_NONBLOCK);

	memset(&params, 0, sizeof params);
	params.rate = 48000;
	params.on_speed = 2.0/100.0;
	params.off_speed = 2.0/20.0;
	params.start_wait = 12;
	params.start_dwell = 3;
	params.curve_dwell = 0;
	params.corner_dwell = 12;
	params.curve_angle = cosf(30.0*(M_PI/180.0)); // 30 deg
	params.end_dwell = 3;
	params.end_wait = 10;
	params.snap = 1/100000.0;
	params.render_flags = RENDER_GRAYSCALE;

	if(olInit(3, 30000) < 0)
		return 1;
	olSetRenderParams(&params);

	float time = 0;
	float ftime;

	int frames = 0;

#define TOP 0.2
#define BOTTOM 0.8
#define LEFT 0.07
#define RIGHT 0.93
#define WIDTH (RIGHT-LEFT)
#define HEIGHT (BOTTOM-TOP)
#define PH 0.12
#define PW 0.03
#define BW 0.02
#define BH 0.02

	float p1 = (HEIGHT-PH)/2;
	float p2 = (HEIGHT-PH)/2;
	float bx = 0;
	float by = HEIGHT/2;
	float bdx = 0.4;
	float bdy = 0.2;

	int score1 = 0;
	int score2 = 0;

	float b1pos = 0, b2pos = 0;
	float eb1pos,eb2pos;

	while(1) {
		while(1) {
			struct input_event ev;
			if (read(in1_fd, &ev, sizeof(ev)) != sizeof(ev))
				break;
			if (ev.type != EV_ABS)
				continue;
			if (ev.code == ABS_Y)
				b1pos = (ev.value - 128) / 255.0;
			else if (ev.code == 5)
				b2pos = (ev.value - 128) / 255.0;
		}
		while(1) {
			struct input_event ev;
			if (read(in2_fd, &ev, sizeof(ev)) != sizeof(ev))
				break;
			if (ev.type != EV_ABS)
				continue;
			if (ev.code == ABS_Y)
				b2pos = (ev.value - 128) / 255.0;
		}

		if (b1pos > 0.1)
			eb1pos = b1pos - 0.1;
		else if (b1pos < -0.1)
			eb1pos = b1pos + 0.1;
		else
			eb1pos = 0;

		if (b2pos > 0.1)
			eb2pos = b2pos - 0.1;
		else if (b2pos < -0.1)
			eb2pos = b2pos + 0.1;
		else
			eb2pos = 0;

		eb1pos *= 2;
		eb2pos *= 2;

		printf("ebpos:%f %f\n", eb1pos, eb2pos);


		olLoadIdentity();
		olTranslate(-1,1);
		olScale(2,-2);
		// window is now 0.0-1.0 X and Y, Y going down)
		olRect(0, TOP, 1, BOTTOM, C_WHITE);
		olRect(LEFT-PW, p1+TOP, LEFT, p1+TOP+PH, C_WHITE);
		olRect(RIGHT, p2+TOP, RIGHT+PW, p2+TOP+PH, C_WHITE);
		olRect(LEFT+bx, TOP+by, LEFT+bx+BW, TOP+by+BW, C_WHITE);
		olLine((LEFT+RIGHT)/2, TOP, (LEFT+RIGHT)/2, BOTTOM, C_GREY(50));

		olTranslate(0,0.08);
		if (score1 >= 100)
			digit(0,0,score1/100,C_WHITE);
		if (score1 >= 10)
			digit(DIGW,0,score1/10%10,C_WHITE);
		digit(2*DIGW,0,score1%10,C_WHITE);

		if (score2 >= 100)
			digit(1-3*DIGW,0,score2/100,C_WHITE);
		if (score2 >= 10)
			digit(1-2*DIGW,0,score2/10%10,C_WHITE);
		digit(1-1*DIGW,0,score2%10,C_WHITE);

		ftime = olRenderFrame(60);

		bx += ftime*bdx;
		by += ftime*bdy;

		if (by > HEIGHT - BH) {
			bdy = -bdy;
			by += 2*ftime*bdy;
		} else if (by < 0) {
			bdy = -bdy;
			by += 2*ftime*bdy;
		}

		p1 += ftime*eb1pos;
		if (p1 < 0)
			p1 = 0;
		if (p1 > HEIGHT-PH)
			p1 = HEIGHT-PH;


		p2 += ftime*eb2pos;
		if (p2 < 0)
			p2 = 0;
		if (p2 > HEIGHT-PH)
			p2 = HEIGHT-PH;

		if (bx < 0) {
			if (by < p1-BH || by > p1+PH) {
				if (bx < -BW) {
					by = p2 + PH/2 - BH/2;
					bx = WIDTH - BW;
					bdx = -0.4;
					bdy = 0.2;
					score2++;
				}
			} else {
				bdx = -bdx;
				bx += 2*ftime*bdx;
			}
		} else if (bx > WIDTH - BW) {
			if (by < p2-BH || by > p2+PH) {
				if (bx > WIDTH) {
					by = p1 + PH/2 - BH/2;
					bx = 0;
					bdx = 0.4;
					bdy = 0.2;
					score1++;
				}
			} else {
				bdx = -bdx;
				bx += 2*ftime*bdx;
			}
		}

		bdx *= powf(1.1, ftime);

		frames++;
		time += ftime;
		printf("Frame time: %f, FPS:%f\n", ftime, frames/time);
	}

	olShutdown();
	exit (0);
}

