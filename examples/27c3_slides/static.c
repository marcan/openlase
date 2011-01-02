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
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void showilda_init(void **ctx, void *param)
{
	*ctx = olLoadIlda(param);
	if (!*ctx)
		olLog("Failed to load %s\n", (char*)param);
}

void showilda_deinit(void *ctx)
{
	if (ctx)
		olFreeIlda(ctx);
}

void showilda_render(void *ctx, float time)
{
	olLoadIdentity();
	olDrawIlda(ctx);
}

int points_left = 0;
int include_dark_points = 0;

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
	if (points_left) {
		points_left--;
		save_x = *x;
		save_y = *y;
		save_color = *color;
		points_dot = 200;
	} else {
		*x = save_x;
		*y = save_y;
		if (points_dot) {
			*color = C_WHITE;
			points_dot--;
		} else {
			*color = C_BLACK;
		}
	}
}

void paintilda_init(void **ctx, void *param, OLRenderParams *params)
{
	*ctx = olLoadIlda(param);
	if (!*ctx)
		olLog("Failed to load %s\n", (char*)param);
	params->render_flags |= RENDER_NOREVERSE;
}

void paintilda_deinit(void *ctx)
{
	if (ctx)
		olFreeIlda(ctx);
}

void paintilda_render(void *ctx, float time)
{
	include_dark_points = 1;
	points_left = time * 300;
	olLoadIdentity();
	olSetPixelShader(cutoff);
	olDrawIlda(ctx);
	olSetPixelShader(NULL);
}


void urls_init(void **ctx, void *param, OLRenderParams *params)
{
	params->on_speed = 2.0/100.0;
	params->off_speed = 2.0/50.0;
	params->start_wait = 7;
	params->start_dwell = 0;
	params->curve_dwell = 0;
	params->corner_dwell = 8;
	params->end_dwell = 0;
	params->end_wait = 7;
	params->flatness = 0.000005;
	params->render_flags |= RENDER_NOREORDER;
}

void urls_deinit(void *ctx)
{
}

void urls_render(void *ctx, float time)
{
	olLoadIdentity();
	olLoadIdentity3();
	olDrawString(olGetDefaultFont(), -0.99, 0.1, 0.25, C_WHITE, "marcansoft.com/openlase");
}

void libol_init(void **ctx, void *param, OLRenderParams *params)
{
	params->on_speed = 2.0/100.0;
	params->off_speed = 2.0/50.0;
	params->start_wait = 15;
	params->start_dwell = 5;
	params->curve_dwell = 0;
	params->corner_dwell = 5;
	params->end_dwell = 5;
	params->end_wait = 2;
	params->flatness = 0.000005;
	params->render_flags |= RENDER_NOREORDER;
}

void libol_deinit(void *ctx)
{
}

void libol_render(void *ctx, float time)
{
	olLoadIdentity();
	olLoadIdentity3();
	olDrawString(olGetDefaultFont(), -0.45, 0.45, 0.2, C_WHITE, "olBegin()");
	olDrawString(olGetDefaultFont(), -0.50, 0.3, 0.8, C_WHITE, "libol");
	olDrawString(olGetDefaultFont(), -0.45, -0.3, 0.2, C_WHITE, "olEnd()");
}
