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

/*
This is a horrible ad-hoc tracing algorithm. Originally it was a bit simpler,
and it was merely used to trace metaballs and fire for the LASE demo. However,
I plugged it into a video stream and it worked too well, so I ended up adding
lots of incremental hacks and improvements.

It's still highly primitive, though. It works by turning the input image into
a 1bpp black and white image (with a threshold), performing edge detection,
then walking those edges and as we clear them. There are quite a few hacks along
the way to try to improve particular situations, such as clearing neighboring
pixels to deal with double-wide edges produced under some circumstances, and
stuff like decimation and overdraw.

If you're wondering about the switch() at the end: in order to find stuff to
trace, it walks the image in a spiral from the outside in, to try to put the
object start/end points near the edges of the screen (less visible). Yes, this
is highly suboptimal for anything but closed objects since quite often it
catches them in the middle and breaks them into two objects. I'm kind of hoping
libol catches most of those and merges them, though. Maybe.
*/

#include "libol.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "trace.h"

#define ABS(a) ((a)<0?(-(a)):(a))

#define OVERDRAW 8

//#define DEBUG

#ifdef DEBUG
uint8_t dbg[640*480*16][3];
#endif

int trace(uint8_t *field, uint8_t *tmp, uint8_t thresh, int w, int h, int s, int decimate)
{
	int x, y, cx, cy, px, py, lx, ly, i;
	int iters = 0;
	int objects = 0;

	int sx[OVERDRAW], sy[OVERDRAW];

	memset(tmp, 0, s*h);
#ifdef DEBUG
	memset(dbg, 0, 3*s*h);
#endif

	for (y=2; y<h-2; y++) {
		for (x=2; x<w-2;x++) {
			int idx = y*s+x;
			if (field[idx] > thresh && (!(field[idx-s] > thresh)
			                         || !(field[idx-1] > thresh))) {
				tmp[idx] = 255;
#ifdef DEBUG
				dbg[idx][0] = 64;
				dbg[idx][1] = 64;
				dbg[idx][2] = 64;
#endif
			}
			if (field[idx] <= thresh && (!(field[idx-s] <= thresh)
			                         || !(field[idx-1] <= thresh))) {
				tmp[idx] = 255;
#ifdef DEBUG
				dbg[idx][0] = 64;
				dbg[idx][1] = 64;
				dbg[idx][2] = 64;
#endif
			}
		}
	}

	int total = h*w;
	int dir = 0;
	int minx = 0, miny = 0;
	int maxx = w-1, maxy = h-1;
	int ldir = 0;
	int lsdir = 0;

	int div = 0;
	int dpcnt = 0;

	int start = 0;

	int pc = 0;

	px = 0;
	py = 0;
	while (total--)
	{
		if (tmp[py*s+px]) {
			x = lx = cx = px;
			y = ly = cy = py;
			iters = 0;
			olBegin(OL_POINTS);
			start = 1;
			pc = pc + 37;
			pc %= 128;
			int lidx = y*s+x;
			while (1)
			{
				int idx = y*s+x;
#ifdef DEBUG
				dbg[idx][1] = 128+pc;
#endif
				int ddir = (ldir - lsdir + 8) % 8;
				if (ddir > 4)
					ddir = 8 - ddir;
				if(div==0) {
					if (iters < OVERDRAW) {
						sx[iters] = x;
						sy[iters] = y;
					}
					olVertex(x, y, C_WHITE);
					/*if (ddir>3) {
						olVertex(x, y, C_WHITE);
						dpcnt++;
					}*/
					iters++;
				}
				div = (div+1)%decimate;
				lsdir = ldir;
				tmp[idx] = 0;
				//printf("Dlt: %d\n", idx-lidx);
				//dbg[idx][2] = 0;
				if (tmp[2*idx-lidx]) {
					x = 2*x-lx;
					y = 2*y-ly;
#ifdef DEBUG
					dbg[2*idx-lidx][2] += 64;
#endif
				} else if ((ldir == 4 || ldir == 6) && tmp[idx-s-1]) {
					y--; x--;
					ldir = 5;
				} else if ((ldir == 2 || ldir == 4) && tmp[idx-s+1]) {
					y--; x++;
					ldir = 3;
				} else if ((ldir == 6 || ldir == 0) && tmp[idx+s-1]) {
					y++; x--;
					ldir = 7;
				} else if ((ldir == 0 || ldir == 2) && tmp[idx+s+1]) {
					y++; x++;
					ldir = 1;
				} else if ((ldir == 5 || ldir == 7) && tmp[idx-1]) {
					x--;
					ldir = 6;
				} else if ((ldir == 1 || ldir == 3) && tmp[idx+1]) {
					x++;
					ldir = 2;
				} else if ((ldir == 3 || ldir == 5) && tmp[idx-s]) {
					y--;
					ldir = 4;
				} else if ((ldir == 1 || ldir == 7) && tmp[idx+s]) {
					y++;
					ldir = 0;
				} else if (tmp[idx-s-1]) {
					y--; x--;
					ldir = 5;
				} else if (tmp[idx-s+1]) {
					y--; x++;
					ldir = 3;
				} else if (tmp[idx+s-1]) {
					y++; x--;
					ldir = 7;
				} else if (tmp[idx+s+1]) {
					y++; x++;
					ldir = 1;
				} else if (tmp[idx-1]) {
					x--;
					ldir = 6;
				} else if (tmp[idx+1]) {
					x++;
					ldir = 2;
				} else if (tmp[idx-s]) {
					y--;
					ldir = 4;
				} else if (tmp[idx+s]) {
					y++;
					ldir = 0;
				} else {
					break;
				}
				if (!start) {
					if (ldir & 1) {
						tmp[idx-1] = 0;
						tmp[idx+1] = 0;
						tmp[idx-s] = 0;
						tmp[idx+s] = 0;
					} else if (ldir == 0) {
						tmp[idx-1-s] = 0;
						tmp[idx+1-s] = 0;
					} else if (ldir == 2) {
						tmp[idx-1-s] = 0;
						tmp[idx-1+s] = 0;
					} else if (ldir == 4) {
						tmp[idx-1+s] = 0;
						tmp[idx+1+s] = 0;
					} else if (ldir == 6) {
						tmp[idx+1-s] = 0;
						tmp[idx+1+s] = 0;
					}
				}
				start = 0;
				tmp[y*s+x] = 255;
				lidx = idx;
				lx = x;
				ly = y;
			}
#ifdef DEBUG
			dbg[py*s+px][0] = 255;
			dbg[py*s+px][1] = 255;
			dbg[py*s+px][2] = 255;
#endif
			if (iters) {
				objects++;
				if (ABS(cx-x) <= 1 && ABS(cy-y) <= 1 && iters > 10) {
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
#ifdef DEBUG
	FILE *f = fopen("dump.bin","wb");
	fwrite(dbg, h, s*3, f);
	fclose(f);
#endif
	//printf("Dup'd: %d\n", dpcnt);
	return objects;
}

