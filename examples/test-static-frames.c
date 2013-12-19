/*
 * test-static-frames.c  Copyright (C) 2013 Kamal Mostafa <kamal@whence.com>

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


void
drawChevron()
{
    int color = C_WHITE;
    olBegin(OL_LINESTRIP);
    olVertex3( -0.2, 0, 0, color);
    olVertex3( 0, +0.2, 0, color);
    olVertex3( +0.2, 0, 0, color);
    olEnd();
}



int main (int argc, char *argv[])
{
	if ( argc != 1 ) {
		fprintf(stderr, "usage: test-static-frames\n");
		return 1;
	}


	OLRenderParams params;

	memset(&params, 0, sizeof params);
	params.rate = 48000;
	params.on_speed = 2.0/100.0;
	params.off_speed = 2.0/100.0;
	params.start_wait = 100;
	params.start_dwell = 7;				// helps slow turn-on
	params.curve_dwell = 0;
	params.corner_dwell = 24;
	params.curve_angle = cosf(10.0*(M_PI/180.0)); // 10 deg
	params.end_dwell = 12;
	params.end_wait = 10;				// helps slow turn-off
	params.snap = 1/100000.0;
	params.render_flags = RENDER_GRAYSCALE;
	params.render_flags |= RENDER_NOREORDER;

	if(olInit(3, 30000) < 0)
		return 1;

	olSetRenderParams(&params);


	int i;
	float ftime;

	for ( i=0; i<5; i++ ) {
	    olLoadIdentity3();
	    olLoadIdentity();
	    olTranslate3(-0.5, 0.3 + -0.2 * i, 0);
	    drawChevron();

	    ftime = olRenderFrame(0);
	    printf("Frame time: %3.0f msec    FPS: %4.1f\n",
		    ftime*1000, 1/ftime);
	    fflush(stdout);

	    sleep(1);
	}

	for ( i=0; i<5; i++ ) {
	    olLoadIdentity3();
	    olLoadIdentity();
	    olTranslate3(+0.5, 0.3 + -0.2 * i, 0);
	    drawChevron();

	    ftime = olRenderFrame(0);
	    printf("Frame time: %3.0f msec    FPS: %4.1f\n",
		    ftime*1000, 1/ftime);
	    fflush(stdout);

	    sleep(1);
	}


	sleep(5);

	olShutdown();
	exit (0);
}

