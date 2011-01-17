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
#include "ilda.h"
#include "text.h"
#include <math.h>
#include <stdio.h>

/* cubes demo */

void cubes_init(void **ctx, void *arg, OLRenderParams *params)
{
}

void cubes_deinit(void *ctx)
{
}

void cubes_render(void *ctx, float time)
{
	int i;

	olLoadIdentity3();
	olLoadIdentity();
	olPerspective(60, 1, 1, 100);
	olTranslate3(0, 0, -3);

	for(i=0; i<2; i++) {
		olScale3(0.6, 0.6, 0.6);

		olRotate3Z(time * M_PI * 0.1);
		olRotate3Y(time * M_PI * 0.8);
		olRotate3X(time * M_PI * 0.73);

		olBegin(OL_LINESTRIP);
		olVertex3(-1, -1, -1, C_WHITE);
		olVertex3( 1, -1, -1, C_WHITE);
		olVertex3( 1,  1, -1, C_WHITE);
		olVertex3(-1,  1, -1, C_WHITE);
		olVertex3(-1, -1, -1, C_WHITE);
		olVertex3(-1, -1,  1, C_WHITE);
		olEnd();

		olBegin(OL_LINESTRIP);
		olVertex3( 1,  1,  1, C_WHITE);
		olVertex3(-1,  1,  1, C_WHITE);
		olVertex3(-1, -1,  1, C_WHITE);
		olVertex3( 1, -1,  1, C_WHITE);
		olVertex3( 1,  1,  1, C_WHITE);
		olVertex3( 1,  1, -1, C_WHITE);
		olEnd();

		olBegin(OL_LINESTRIP);
		olVertex3( 1, -1, -1, C_WHITE);
		olVertex3( 1, -1,  1, C_WHITE);
		olEnd();

		olBegin(OL_LINESTRIP);
		olVertex3(-1,  1,  1, C_WHITE);
		olVertex3(-1,  1, -1, C_WHITE);
		olEnd();
	}
}

/* laser scanner diagram thing */
/* yes this is ugly stuff */

static int points_left = 0;
static int point_step = 1;
static int delay = 0;
static int delay_ctr = 0;
static int include_dark_points = 0;

static void cutoff(float *x, float *y, uint32_t *color)
{
	static float save_x, save_y;
	static uint32_t save_color;
	static int points_dot = 0;
	if ((!include_dark_points) && (*color == C_BLACK)) {
		if (points_left)
			return;
		*x = save_x;
		*y = save_y;
	}
	if ((points_left-delay) > 0) {
		points_left -= point_step;
		save_x = *x;
		save_y = *y;
		save_color = *color;
		points_dot = 200;
	} else {
		if (points_left > delay_ctr && points_left < delay && (delay - points_left) > 20)
			points_dot = 0;
		*x = save_x;
		*y = save_y;
		if (points_dot) {
			*color = C_WHITE;
			points_dot--;
		} else {
			*color = C_BLACK;
		}
	}
	if (delay > delay_ctr)
		delay_ctr += point_step;
}

void diag_init(void **ctx, void *arg, OLRenderParams *params)
{
	*ctx = olLoadIlda("27c3-logo.ild");
	if (!*ctx)
		olLog("Failed to load 27c3-logo.ild\n");

	params->off_speed = 2.0/25.0;
	params->start_wait = 15;
}

void diag_deinit(void *ctx)
{
	if (ctx)
		olFreeIlda(ctx);
}

static void draw_galvo(void)
{
	point_step = 10;

	olBegin(OL_LINESTRIP);
	olVertex3(-5, -10, -5, C_WHITE);
	olVertex3(-5, 10, -5, C_WHITE);
	olVertex3(5, 10, 5, C_WHITE);
	olVertex3(5, -10, 5, C_WHITE);
	olVertex3(-5, -10, -5, C_WHITE);
	olEnd();

	olBegin(OL_LINESTRIP);
	olVertex3(2, -10, 2, C_WHITE);
	olVertex3(2, -20, 0, C_WHITE);
	olEnd();
	olBegin(OL_LINESTRIP);
	olVertex3(-2, -10, -2, C_WHITE);
	olVertex3(-2, -20, 0, C_WHITE);
	olEnd();
	olBegin(OL_LINESTRIP);
	olVertex3(-9, -50, 0, C_WHITE);
	olVertex3(-9, -20, 0, C_WHITE);
	olVertex3(9, -20, 0, C_WHITE);
	olVertex3(9, -50, 0, C_WHITE);
	olEnd();
	olBegin(OL_LINESTRIP);
	olVertex3(-20, -50, 0, C_WHITE);
	olVertex3(-20, -70, 0, C_WHITE);
	olVertex3(20, -70, 0, C_WHITE);
	olVertex3(20, -50, 0, C_WHITE);
	olVertex3(-20, -50, 0, C_WHITE);
	olEnd();

	point_step = 4;
	olDrawString(olGetDefaultFont(), -15, -55, 14, C_WHITE, "Galvo");

}

typedef struct {
	float time;
	float x, y, z;
	float rx, ry, rz;
} campt;

static campt campoints[] = {
	{0, -140, 0, -120, 0, 0, 0},
	{3.0, -140, 0, -120, 0, 0, 0},
	{4.5, -100, 0, 0, 0, 0, 0},
	{6.5, 10, -20, -70, 0, 0, 0},
	{13, 10, -20, -70, 0, 0, 0},
	{16, 50, 50, 10, -M_PI/2, 0, -0.5},
	{17, 0, 140, 0, -M_PI/2, 0, 0},
	{18, 0, 310, 0, -M_PI/2, 0, 0},
};

#define LOGO_START 14
#define FADE_START 16
#define FADE_END 17

#define NUM_CAMPOINTS (sizeof(campoints)/sizeof(campoints[0]))

void diag_render(void *ctx, float time)
{
	int i;

	delay = 0;
	delay_ctr = 0;

	//time += 10;

	//time -= 1;
	if (time < 0)
		time = 0;

	int spl = time * 1000;
	points_left = spl;

	int maincolor = 255;
	if (time > FADE_START)
		maincolor = (FADE_END - time) * 255;

	olSetPixelShader(NULL);
	olLoadIdentity3();
	olLoadIdentity();

	point_step = 10;

	olFrustum(-1, 1, -1, 1, 3, 100);
	olTranslate3(0, 0, -3);
	olScale3(0.01, 0.01, 0.01);

	campt *p = &campoints[0];

	for (i=0; i<NUM_CAMPOINTS; i++) {
		if (campoints[i].time > time)
			break;
		p = &campoints[i];
	}

	campt dest;

	dest = *p;

	if (i == NUM_CAMPOINTS) {
		dest = *p;
	} else {
		float subpos = (time - p[0].time) / (p[1].time - p[0].time);
		subpos = (sinf((subpos - 0.5) * M_PI) + 1)/2;
		olLog("Subpos: %f\n", subpos);
		dest.x = p[0].x * (1-subpos) + p[1].x * subpos;
		dest.y = p[0].y * (1-subpos) + p[1].y * subpos;
		dest.z = p[0].z * (1-subpos) + p[1].z * subpos;
		dest.rx = p[0].rx * (1-subpos) + p[1].rx * subpos;
		dest.ry = p[0].ry * (1-subpos) + p[1].ry * subpos;
		dest.rz = p[0].rz * (1-subpos) + p[1].rz * subpos;
	}

	olLog("Camera @ %f: %f %f %f %f %f %f\n", time, dest.x, dest.y, dest.z, dest.rx, dest.ry, dest.rz);

	olRotate3X(dest.rx);
	olRotate3Y(dest.ry);
	olRotate3Z(dest.rz);
	olTranslate3(-dest.x, -dest.y, -dest.z);

	int logo_points = (time - LOGO_START) * 200;
	if (logo_points < 0)
		logo_points = 0;

	IldaFile *ild = ctx;
	int logo_idx = logo_points;
	if (logo_idx >= ild->count)
		logo_idx = ild->count - 1;

	if (time < FADE_END) {
		olPushColor();
		olMultColor(C_GREY(maincolor < 0 ? 0 : maincolor));
		olSetPixelShader(cutoff);

		olBegin(OL_LINESTRIP);
		olVertex3(-150, 10, 0, C_WHITE);
		olVertex3(-190, 10, 0, C_WHITE);
		olVertex3(-190, -10, 0, C_WHITE);
		olVertex3(-150, -10, 0, C_WHITE);
		olVertex3(-150, 10, 0, C_WHITE);
		olEnd();
		olBegin(OL_LINESTRIP);
		olVertex3(-150, 7, 0, C_WHITE);
		olVertex3(-140, 7, 0, C_WHITE);
		olVertex3(-140, -7, 0, C_WHITE);
		olVertex3(-150, -7, 0, C_WHITE);
		olEnd();

		point_step = 4;

		olDrawString(olGetDefaultFont(), -185, 6, 14, C_WHITE, "Laser");

		point_step = 20;
		delay += 1000;

		float px = 50 * ild->points[logo_idx].x;
		float py = 50 * ild->points[logo_idx].y;

		olBegin(OL_LINESTRIP);
		olVertex3(-140, 0, 0, C_WHITE);
		olVertex3(0, 0, 0, C_WHITE);
		olVertex3(px / 20, 0, 15, C_WHITE);
		olVertex3(px, 140, py, C_WHITE);
		olEnd();

		draw_galvo();
		olPushMatrix3();
		olTranslate3(0, 0, 15);
		olRotate3Z(M_PI/2);
		delay += 200;
		draw_galvo();
		olPopMatrix3();

		delay += 1000;

		point_step = 20;

		olPopColor();

	}

	olSetPixelShader(NULL);
	point_step = 10;
	olBegin(OL_LINESTRIP);
	olVertex3(-50, 140, 37.5, C_WHITE);
	olVertex3(50, 140, 37.5, C_WHITE);
	olVertex3(50, 140, -37.5, C_WHITE);
	olVertex3(-50, 140, -37.5, C_WHITE);
	olVertex3(-50, 140, 37.5, C_WHITE);
	olEnd();

	olTranslate3(0, 140, 0);
	olScale3(50, 50, 50);
	olRotate3X(M_PI/2);

	if (logo_points > 0) {
		delay = delay_ctr = 0;
		points_left = logo_points;
		point_step = 1;
		olSetPixelShader(cutoff);
		olDrawIlda3D(ctx);
	}
	olSetPixelShader(NULL);
}


