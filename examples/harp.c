/*
        OpenLase - a realtime laser graphics toolkit

Copyright (C) 2009-2010 Hector Martin "marcan" <hector@marcansoft.com>

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

// This is the laser harp projection, which is just a simple OpenLase app */

#include "libol.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

int offs[12] = {
	0,1,0,1,0,0,1,0,1,0,1,0
};
float pos[12] = {
	0,0.5,1,1.5,2,3,3.5,4,4.5,5,5.5,6
};

int main (int argc, char *argv[])
{
	OLRenderParams params;

	memset(&params, 0, sizeof params);
	params.rate = 48000;
	params.on_speed = 2.0/100.0;
	params.off_speed = 2.0/30.0;
	params.start_wait = 4;
	params.start_dwell = 3;
	params.curve_dwell = 0;
	params.corner_dwell = 4;
	params.curve_angle = cosf(30.0*(M_PI/180.0)); // 30 deg
	params.end_dwell = 2;
	params.end_wait = 5;
	params.snap = 1/100000.0;
	params.render_flags = RENDER_GRAYSCALE;

	if(olInit(3, 30000) < 0)
		return 1;

	olSetRenderParams(&params);

	float time = 0;
	float ftime;
	int i;

	int frames = 0;

	int points = 25;

	while(1) {
		olLoadIdentity();

		int total_y = (points / 12.0 * 7.0 + 1);

		for (i=0; i<points; i++) {
			int octave = i/12;
			float y = (octave * 7 + pos[i%12]) * 2.0 / (total_y) - 1.0;
			float x = offs[i%12] * 0.1;
			olDot(x, y, 30, C_WHITE);
		}

		ftime = olRenderFrame(60);
		frames++;
		time += ftime;
		printf("Frame time: %f, FPS:%f\n", ftime, frames/time);
	}

	olShutdown();
	exit (0);
}

