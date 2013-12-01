/*
 * spin-text.c  Copyright (C) 2013 Kamal Mostafa <kamal@whence.com>

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
#include "text.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <jack/jack.h>
#include <math.h>


void
draw_spinning_glyph( float x, float y, float rot, Font *font, float fonth, char c )
{
	float w = olGetCharWidth(font, fonth, c);
	olPushMatrix3();

	olRotate3Y(rot);

	olDrawChar(font,
		    //-(4*w0) + i*w0 + (w0-w)/2,
		    -w/2,
		    y,
		    fonth, C_WHITE, c);

	olPopMatrix3();

}



int main (int argc, char *argv[])
{
	if ( argc < 2 ) {
		fprintf(stderr,
    "usage: spin-text {fontname} ['Text to Render']\n"
    "                  fontname - a Hershey font name (e.g. gothiceng) or 'default'\n"
			);
		return 1;
	}
	char *fontname = argv[1];


	OLRenderParams params;

	memset(&params, 0, sizeof params);
	params.rate = 48000;
	params.on_speed = 2.0/100.0;
	params.off_speed = 2.0/100.0;
	params.start_wait = 8;
	params.start_dwell = 7;				// helps slow turn-on
	params.curve_dwell = 0;
	params.corner_dwell = 8;
	params.curve_angle = cosf(10.0*(M_PI/180.0)); // 10 deg
	params.end_dwell = 3;
	params.end_wait = 10;				// helps slow turn-off
	params.snap = 1/100000.0;
	params.render_flags = RENDER_GRAYSCALE;
	params.render_flags |= RENDER_NOREORDER;
	params.flatness = 0.0000001;			// for default font

	if(olInit(3, 30000) < 0)
		return 1;

	olSetRenderParams(&params);

	float mytime = 0;
	float ftime;

	int frames = 0;

	olFontType fonttype;
	if ( strcasecmp(fontname,"default") == 0 )
		fonttype = OL_FONT_DEFAULT;
	else
		fonttype = OL_FONT_HERSHEY;

	Font *font = olGetFont(fonttype, fontname);
	if ( ! font ) {
	    perror(fontname);
	    return 1;
	}


	float rflip = M_PI;
	float r0 = 0.0;

	int spintime = 100;

	const char *s = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	if ( argc == 3 )
		s = argv[2];

	const char *p = s;
	char c = *p;
	while(1) {
		olLoadIdentity3();
		olLoadIdentity();

		if ( frames % (spintime/2) == (spintime/4) ) {
		    if ( *p == 0 )
			p = s;
		    c = *p++;
		    r0 = ( r0 == 0.0 ) ? rflip : 0.0;
		}

		float rot = (frames % spintime) * M_PI * 2 / spintime - r0;
		draw_spinning_glyph(-1.0, 1.0, rot, font, 1.6, c);

		ftime = olRenderFrame(60);
		frames++;
		mytime += ftime;
		if ( (frames % 10) == 0 ) {
			printf("Frame time: %3.0f msec    FPS: %4.1f\r",
				    ftime*1000, frames/mytime);
			fflush(stdout);
		}
	}

	olShutdown();
	exit (0);
}

