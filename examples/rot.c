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

#include "libol.h"
#include "ilda.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

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

	IldaFile *ild = olLoadIlda("cbase-logo.ild");

	while(1) {
		olLoadIdentity();
		olScale(1.7,2.0);
		olLoadIdentity3();
		olPerspective(60, 1, 1, 100);
		olTranslate3(0, 0, -3);

		olRotate3Y(time * M_PI * 0.8);

		olDrawIlda3D(ild);

		ftime = olRenderFrame(60);
		frames++;
		time += ftime;
	}

	olShutdown();
	exit (0);
}

