/*
 * show-text.c  Copyright (C) 2013 Kamal Mostafa <kamal@whence.com>

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
draw_text_frame( Font *font, float height, char *text )
{
	float width;
	olLoadIdentity3();
	olLoadIdentity();
	width = olGetStringWidth(font, height, text);
	olDrawString(font, -width/2, height/2, height, C_WHITE, text);
}


int main (int argc, char *argv[])
{
	if ( argc < 2 || argc > 3) {
		fprintf(stderr,
    "usage: show-text {fontname} ['Text to Render']\n"
    "                  fontname - a Hershey font name (e.g. gothiceng) or 'default'\n"
			);
		return 1;
	}
	char *fontname = argv[1];
	char *text = NULL;

	if ( argc == 3 )
	    text = argv[2];


	OLRenderParams params;

	memset(&params, 0, sizeof params);
	params.rate = 48000;
	params.on_speed = 1.0/100.0;
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


	float height = 0.3;
	if ( text ) {

		draw_text_frame(font, height, text);
		olRenderFrame(120);

		printf("Hit Enter to quit...\n");
		fflush(stdout);
		getchar();

	} else {

	    char buf[1024];
	    while ( fgets(buf, sizeof(buf), stdin) ) {
		int n = strlen(buf);
		if ( n == 0 )
		    continue;
		if ( buf[n-1] == '\n' )
		    buf[--n] = 0;
		draw_text_frame(font, height, buf);
		olRenderFrame(120);
		usleep(n*250000);
	    }

	}

	olShutdown();
	exit (0);
}

