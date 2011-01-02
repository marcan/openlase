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
#include <stdint.h>
#include <string.h>

/* metaballs demo */

/* This is the original quick&dirty metaballs tracer. The new one optimized for
 * video is in trace.c. It's still dirty, but no longer that quick. */
#define ABS(a) ((a)<0?(-(a)):(a))

#define OVERDRAW 8

static int trace(int *field, uint8_t *tmp, int thresh, int w, int h, int decimate)
{
	int x, y, cx, cy, px, py, i;
	int iters = 0;
	int objects = 0;
	int sx[OVERDRAW], sy[OVERDRAW];

	memset(tmp, 0, w*h);

	for (y=1; y<h-1; y++) {
		for (x=1; x<w-1;x++) {
			int idx = y*w+x;
			if (field[idx] > thresh && (!(field[idx-w] > thresh)
			                         || !(field[idx+w] > thresh)
			                         || !(field[idx-1] > thresh)
			                         || !(field[idx+1] > thresh))) {
				tmp[idx] = 1;
			}
		}
	}

	int total = h*w;
	int dir = 0;
	int minx = 0, miny = 0;
	int maxx = w-1, maxy = h-1;
	int div = 0;

	px = 0;
	py = 0;
	while (total--)
	{
		if (tmp[py*w+px]) {
			x = cx = px;
			y = cy = py;
			iters = 0;
			olBegin(OL_POINTS);
			while (1)
			{
				int idx = y*w+x;
				if(div==0) {
					if (iters < OVERDRAW) {
						sx[iters] = x;
						sy[iters] = y;
					}
					olVertex(x, y, C_WHITE);
					iters++;
				}
				div = (div+1)%decimate;
				tmp[idx] = 0;
				if (tmp[idx-1]) {
					x--;
				} else if (tmp[idx+1]) {
					x++;
				} else if (tmp[idx-w]) {
					y--;
				} else if (tmp[idx+w]) {
					y++;
				} else if (tmp[idx-w-1]) {
					y--; x--;
				} else if (tmp[idx-w+1]) {
					y--; x++;
				} else if (tmp[idx+w-1]) {
					y++; x--;
				} else if (tmp[idx+w+1]) {
					y++; x++;
				} else {
					break;
				}

			}
			if (iters) {
				objects++;
				if (ABS(cx-x) <= 1 && ABS(cy-y) <= 1) {
					if (iters > OVERDRAW)
						iters = OVERDRAW;
					for (i=0; i<iters; i++)
						olVertex(sx[i], sy[i], C_GREY((int)(255.0 * (OVERDRAW - 1 - i) / (float)OVERDRAW)));
				}
			}
			olEnd();
		}
		switch(dir) {
			case 0:
				px++;
				if (px > maxx) {
					px--; py++; maxx--; dir++;
				}
				break;
			case 1:
				py++;
				if (py > maxy) {
					py--; px--; maxy--; dir++;
				}
				break;
			case 2:
				px--;
				if (px < minx) {
					px++; py--; minx++; dir++;
				}
				break;
			case 3:
				py--;
				if (py < miny) {
					py++; px++; miny++; dir=0;
				}
				break;
		}
	}
	return objects;
}


void metaballs_init(void **ctx, void *arg, OLRenderParams *params)
{
	params->start_wait = 8;
	params->start_dwell = 7;
	params->end_dwell = 7;
}

void metaballs_deinit(void *ctx)
{
}

static int mbuf[192][256];
static uint8_t mtmp[192][256];

static void draw_metaball(float x, float y, float radius)
{
	int cx,cy;
	float px,py;
	int *p = &mbuf[0][0];
	px = -x;
	py = -y / 256.0 * 192.0;

	radius *= 400000.0f;

	for(cy=0; cy<192; cy++) {
		for(cx=0; cx<256;cx++) {
			float d = px*px+py*py;
			*p++ += radius/(d+1);
			px++;
		}
		py++;
		px = -x;
	}
}

void metaballs_render(void *ctx, float time)
{
	float dist1 =  128 + sinf(time * M_PI * 1.0 * 0.8 * 0.8 + 0) * 95;
	float dist2 =  128 + sinf(time * M_PI * 1.2 * 0.8 * 0.8 + 1) * 95;
	float dist3 =  130 + sinf(time * M_PI * 1.3 * 0.8 * 0.8 + 2) * 95;
	float dist4 =  100 + sinf(time * M_PI * 1.4 * 0.8 * 0.8 + 3) * 95;
	float dist5 =  128 + sinf(time * M_PI * 1.5 * 0.5 * 0.8 + 4) * 95;
	float dist6 =  128 + sinf(time * M_PI * 1.6 * 0.5 * 0.8 + 5) * 95;
	float dist7 =  130 + sinf(time * M_PI * 1.7 * 0.5 * 0.8 + 6) * 95;
	float dist8 =  100 + sinf(time * M_PI * 1.8 * 0.5 * 0.8 + 7) * 95;
	float dist9 =  130 + sinf(time * M_PI * 1.9 * 0.8 * 0.8 + 6) * 95;
	float dist10 = 100 + sinf(time * M_PI * 2.0 * 0.5 * 0.8 + 7) * 95;

	memset(mbuf, 0, sizeof mbuf);

	draw_metaball(dist1, dist5, 45);
	draw_metaball(dist2, dist6, 10);
	draw_metaball(dist7, dist3, 30);
	draw_metaball(dist8, dist4, 70);
	draw_metaball(dist9, dist10, 70);

	olPushMatrix();
	olTranslate(-1.0f, -0.75f);
	olScale(2.0f/256.0f, 2.0f/256.0f);

	trace(&mbuf[0][0], &mtmp[0][0], 20000, 256, 192, 1);

	olPopMatrix();
	return;
}
