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

void HSBToRGB(
    unsigned int inHue, unsigned int inSaturation, unsigned int inBrightness,
    unsigned int *oR, unsigned int *oG, unsigned int *oB )
{
    if (inSaturation == 0)
    {
        // achromatic (grey)
        *oR = *oG = *oB = inBrightness;
    }
    else
    {
        unsigned int scaledHue = (inHue * 6);
        unsigned int sector = scaledHue >> 8; // sector 0 to 5 around the color wheel
        unsigned int offsetInSector = scaledHue - (sector << 8);	// position within the sector
        unsigned int p = (inBrightness * ( 255 - inSaturation )) >> 8;
        unsigned int q = (inBrightness * ( 255 - ((inSaturation * offsetInSector) >> 8) )) >> 8;
        unsigned int t = (inBrightness * ( 255 - ((inSaturation * ( 255 - offsetInSector )) >> 8) )) >> 8;

        switch( sector ) {
        case 0:
            *oR = inBrightness;
            *oG = t;
            *oB = p;
            break;
        case 1:
            *oR = q;
            *oG = inBrightness;
            *oB = p;
            break;
        case 2:
            *oR = p;
            *oG = inBrightness;
            *oB = t;
            break;
        case 3:
            *oR = p;
            *oG = q;
            *oB = inBrightness;
            break;
        case 4:
            *oR = t;
            *oG = p;
            *oB = inBrightness;
            break;
        default:    // case 5:
            *oR = inBrightness;
            *oG = p;
            *oB = q;
            break;
        }
    }
}

/*
Simple demonstration, shows two cubes in perspective 3D.
*/

int main (int argc, char *argv[])
{
	OLRenderParams params;

	memset(&params, 0, sizeof params);
	params.rate = 48000;
	params.on_speed = 2.0/100.0;
	params.off_speed = 2.0/20.0;
	params.start_wait = 8;
	params.start_dwell = 3;
	params.curve_dwell = 0;
	params.corner_dwell = 8;
	params.curve_angle = cosf(30.0*(M_PI/180.0)); // 30 deg
	params.end_dwell = 3;
	params.end_wait = 7;
	params.snap = 1/100000.0;
	params.render_flags = RENDER_GRAYSCALE;

	if(olInit(3, 30000) < 0)
		return 1;

	olSetRenderParams(&params);

	float time = 0;
	float ftime;
	int i,j;

	int frames = 0;

	int hue = 0;
	int saturation = 255;
	int brightness = 255;

	while(1) {
		olLoadIdentity3();
		olLoadIdentity();
		olPerspective(60, 1, 1, 100);
		olTranslate3(0, 0, -3);

		hue = (hue + 1) % 255;

		unsigned int r,g,b;
		HSBToRGB(hue, saturation, brightness, &r, &g, &b);

		int color = (b) + (g<<8) + (r<<16);

		for(i=0; i<2; i++) {
			olScale3(0.6, 0.6, 0.6);

			olRotate3Z(time * M_PI * 0.1);
			olRotate3Y(time * M_PI * 0.8);
			olRotate3X(time * M_PI * 0.73);

			olBegin(OL_LINESTRIP);
			olVertex3(-1, -1, -1, color);
			olVertex3( 1, -1, -1, color);
			olVertex3( 1,  1, -1, color);
			olVertex3(-1,  1, -1, color);
			olVertex3(-1, -1, -1, color);
			olVertex3(-1, -1,  1, color);
			olEnd();

			olBegin(OL_LINESTRIP);
			olVertex3( 1,  1,  1, color);
			olVertex3(-1,  1,  1, color);
			olVertex3(-1, -1,  1, color);
			olVertex3( 1, -1,  1, color);
			olVertex3( 1,  1,  1, color);
			olVertex3( 1,  1, -1, color);
			olEnd();

			olBegin(OL_LINESTRIP);
			olVertex3( 1, -1, -1, color);
			olVertex3( 1, -1,  1, color);
			olEnd();

			olBegin(OL_LINESTRIP);
			olVertex3(-1,  1,  1, color);
			olVertex3(-1,  1, -1, color);
			olEnd();

		}

		ftime = olRenderFrame(60);
		frames++;
		time += ftime;
		printf("Frame time: %f, FPS:%f\n", ftime, frames/time);
	}

	olShutdown();
	exit (0);
}

