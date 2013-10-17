/*
 * laser-clock.c  Copyright (C) 2013 Kamal Mostafa <kamal@whence.com>

OpenLase boilerplate code:
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
#include "text.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <jack/jack.h>
#include <math.h>


void
draw_circle( float cx, float cy, float r, int nsegments, int color,
	     unsigned int dots_dwell )
{
    	float x, y;
	int i, j;
	int vcount = dots_dwell + 1;

	float x0 = sinf(0);
	float y0 = cosf(0);
	olBegin(OL_LINESTRIP);
	for ( j=vcount; j; j-- )
	    olVertex(x0*r + cx, y0*r + cy, color);
	for ( i=1; i<nsegments; i++ ) {
	    x = sinf(2 * M_PI * (float)i / nsegments);
	    y = cosf(2 * M_PI * (float)i / nsegments);
	    for ( j=vcount; j; j-- )
		olVertex(x*r + cx, y*r + cy, color);
	}
	for ( j=vcount; j; j-- )
	    olVertex(x0*r + cx, y0*r + cy, color);
	olEnd();
}


void
draw_analog_clock( float x, float y, float r, struct tm *tmp,
		unsigned long usec )
{
	float l;

	olPushMatrix3();
	olTranslate3(x, -y, 0);

	draw_circle(0, 0, r, 16, C_WHITE, 0);

	int hh = tmp->tm_hour % 12;
	int mm = tmp->tm_min;
	int ss = tmp->tm_sec;

#  if 0
	// ticking second hand
	x = sinf((ss/60.0) * 2*M_PI);
	y = cosf((ss/60.0) * 2*M_PI);
#  else
	// usec-smoothed second hand
	unsigned long ssu = ss * 1000000 + usec;
	x = sinf((ssu/60000000.0) * 2*M_PI);
	y = cosf((ssu/60000000.0) * 2*M_PI);
#  endif
	l = 0.8;
	//olLine(x*r*l, y*r*l, -x*r*l/8, -y*r*l/8, C_WHITE);
	olLine(0, 0, x*r*l, y*r*l, C_WHITE);

	// ticking minute hand
	x = sinf((mm/60.0) * 2*M_PI);
	y = cosf((mm/60.0) * 2*M_PI);
	l = 0.90;
	olLine(x*r*l, y*r*l, 0, 0, C_WHITE);

	// minute-smoothed hour hand
	unsigned long hhm = hh * 60 + mm;
	x = sinf((hhm/(12.0*60)) * 2*M_PI);
	y = cosf((hhm/(12.0*60)) * 2*M_PI);
	l = 0.4;
	olLine(0, 0, x*r*l, y*r*l, C_WHITE);

#  if 0
	// hour-marker dots
	for ( i=0; i<12; i++ ) {
	    x = sinf((i/12.0) * 2*M_PI);
	    y = cosf((i/12.0) * 2*M_PI);
	    olDot(x*r, y*r, 2, C_WHITE);
	}
#  endif

	olPopMatrix3();
}


void
draw_digital_clock( float x, float y, float fonth, struct tm *tmp )
{
	int i;
	char outstr[40];

	strftime(outstr, sizeof(outstr), "%I:%M:%S", tmp);

	// Why doesn't olGetCharWidth() take a font_height scalar arg
	// like olGetStringWidth() does?
	float w0 = olGetCharWidth(olGetDefaultFont(), '0') * fonth;
	for ( i=0; outstr[i]; i++ ) {
	    char c = outstr[i];
	    float w = olGetCharWidth(olGetDefaultFont(), c) * fonth;
	    olDrawChar(olGetDefaultFont(),
			-(4*w0) + i*w0 + (w0-w)/2,
			y,
			fonth, C_WHITE, c);
	}
}



int main (int argc, char *argv[])
{
	OLRenderParams params;

	memset(&params, 0, sizeof params);
	params.rate = 48000;
	params.on_speed = 2.0/100.0;
	params.off_speed = 2.0/100.0;
	params.start_wait = 8;
	params.start_dwell = 7;				// helps slow turn-on
	params.curve_dwell = 0;
	params.corner_dwell = 8;
	params.curve_angle = cosf(30.0*(M_PI/180.0)); // 30 deg
	params.end_dwell = 3;
	params.end_wait = 10;				// helps slow turn-off
	params.snap = 1/100000.0;
	params.render_flags = RENDER_GRAYSCALE;
	params.render_flags |= RENDER_NOREORDER;
	params.flatness = 0.0000001;			// fonts needs this

	if(olInit(3, 30000) < 0)
		return 1;

	olSetRenderParams(&params);

	float mytime = 0;
	float ftime;

	int frames = 0;

	while(1) {
		olLoadIdentity3();
		olLoadIdentity();

		struct timeval tv;
		gettimeofday(&tv, NULL);
		time_t t = tv.tv_sec;
		struct tm *tmp = localtime(&t);

		draw_analog_clock(0.0, 0.5, 0.5, tmp, tv.tv_usec);
		//draw_analog_clock(0.0, 0.0, 1.0, tmp, tv.tv_usec);

		draw_digital_clock(0.0, -0.59, 0.2, tmp);

		ftime = olRenderFrame(120);
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

