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
object start/end points near the edges of the screen (less visible).
*/

#include "libol.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "trace.h"

#define ABS(a) ((a)<0?(-(a)):(a))

#define OVERDRAW 6

//#define DEBUG

#ifdef DEBUG
static uint8_t dbg[640*480*16][3];
static int pc = 0;
static int tframe = 0;
#endif

static const int tdx[8] = { 1,  1,  0, -1, -1, -1,  0,  1 };
static const int tdy[8] = { 0, -1, -1, -1,  0,  1,  1,  1 };

static const int tdx2[16] = { 2,  2,  2,  1,  0, -1, -2, -2, -2, -2, -2, -1,  0,  1,  2,  2 };
static const int tdy2[16] = { 0, -1, -2, -2, -2, -2, -2, -1,  0,  1,  2,  2,  2,  2,  2,  1 };

static int trace_pixels(uint8_t *buf, int s, int decimate, int *cx, int *cy, int flag)
{
	int i;
	int x = *cx;
	int y = *cy;
	int iters = 0;
	int start = 1;
	int div = 0;
#ifdef DEBUG
	if (decimate != -1) {
		pc = pc + 47;
		pc %= 160;
	}
#endif
	int lidx = 0;
	int dir = 0;
	int bufx[OVERDRAW], bufy[OVERDRAW];

	while (1)
	{
		int idx = y*s+x;
		if (decimate != -1) {
			if (div == 0) {
				if (iters < OVERDRAW) {
					bufx[iters] = x;
					bufy[iters] = y;
				}
				olVertex(x, y, C_WHITE);
				iters++;
			}
			div = (div+1)%decimate;
		} else {
			iters++;
		}
		buf[idx] &= ~flag;
		if (start) {
			// just pick any direction the first time
			for (dir=0; dir<8; dir++) {
				int dx = tdx[dir];
				int dy = tdy[dir];
				if (buf[idx+dx+s*dy] & flag) {
					x += dx;
					y += dy;
					break;
				}
			}
			if (dir >= 8)
				break;
		} else if (buf[2*idx-lidx] & flag) {
			// can we keep going in the same direction?
			x += tdx[dir];
			y += tdy[dir];
		} else {
			// no, check for lowest angle path
			int ddir, ndir;
			for (ddir=1; ddir<=4; ddir++) {
				ndir = (dir + ddir) % 8;
				if (buf[idx+tdx[ndir]+s*tdy[ndir]] & flag) {
					dir = ndir;
					x += tdx[ndir];
					y += tdy[ndir];
					break;
				}
				ndir = (8 + dir - ddir) % 8;
				if (buf[idx+tdx[ndir]+s*tdy[ndir]] & flag) {
					dir = ndir;
					x += tdx[ndir];
					y += tdy[ndir];
					break;
				}
			}
			if (ddir > 4) {
				// now try the distance-2 neighborhood, can we skip a pixel?
				for (ddir=0; ddir<=8; ddir++) {
					ndir = (2*dir + ddir) % 16;
					if (buf[idx+tdx2[ndir]+s*tdy2[ndir]] & flag) {
						dir = (dir + ddir/2) % 8;
						x += tdx2[ndir];
						y += tdy2[ndir];
						break;
					}
					ndir = (16 + 2*dir - ddir) % 16;
					if (buf[idx+tdx2[ndir]+s*tdy2[ndir]] & flag) {
						dir = (8 + dir - ddir/2) % 8;
						x += tdx2[ndir];
						y += tdy2[ndir];
						break;
					}
				}
				if (ddir > 8) {
					lidx = idx;
					break;
				}
			}
		}
		if (!start) {
			// when moving diagonally, clear out some adjacent pixels
			// this deals with double-thickness diagonals
			if (dir & 1) {
				int adir = (dir + 1) % 8;
				buf[idx+tdx[adir]+s*tdy[adir]] &= ~flag;
				adir = (dir + 7) % 8;
				buf[idx+tdx[adir]+s*tdy[adir]] &= ~flag;
			}
		}
#ifdef DEBUG
		if (decimate != -1) {
			if (!start)
				dbg[idx][2] = 96+pc;
		}
#endif
		start = 0;
		lidx = idx;
	}
#ifdef DEBUG
/*
	if (decimate != -1) {
		dbg[lidx][0] = 255;
		dbg[lidx][1] = 0;
		dbg[lidx][2] = 0;
	}
*/
#endif
	if (decimate != -1 && iters) {
		if (ABS(*cx-x) <= 2 && ABS(*cy-y) <= 2 && iters > 10) {
			if (iters > OVERDRAW)
				iters = OVERDRAW;
			for (i=0; i<iters; i++)
				olVertex(bufx[i], bufy[i], C_GREY((int)(255.0 * (OVERDRAW - 1 - i) / (float)OVERDRAW)));
		}
	}
	*cx = x;
	*cy = y;
	return iters;
}

int trace(uint8_t *field, uint8_t *tmp, uint8_t thresh, int w, int h, int s, int decimate)
{
	int x, y;
	int objects = 0;

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

	int scandir = 0;
	int total = h*w;
	int minx = 0, miny = 0;
	int maxx = w-1, maxy = h-1;

	x = 0;
	y = 0;
	while (total--)
	{
		int flg = 1;
		while (tmp[y*s+x] & 128) {
			int tx = x, ty = y;
			if (flg != 64)
				flg <<= 1;
			trace_pixels(tmp, s, -1, &tx, &ty, flg);
#ifdef DEBUG
			int sx = tx, sy = ty;
#endif
			olBegin(OL_POINTS);
			if (trace_pixels(tmp, s, decimate, &tx, &ty, 255))
				objects++;
			olEnd();

#ifdef DEBUG
			dbg[y*s+x][0] = 255;
			dbg[y*s+x][1] = 255;
			dbg[y*s+x][2] = 0;

			dbg[sy*s+sx][0] = 0;
			dbg[sy*s+sx][1] = 255;
			dbg[sy*s+sx][2] = 255;

			dbg[ty*s+tx][0] = 255;
			dbg[ty*s+tx][1] = 0;
			dbg[ty*s+tx][2] = 0;
			printf("%d: %d,%d %d,%d %d,%d\n", tframe, x, y, sx, sy, tx, ty);
#endif
		}
		switch(scandir) {
			case 0:
				x++;
				if (x > maxx) {
					x--; y++; maxx--; scandir++;
				}
				break;
			case 1:
				y++;
				if (y > maxy) {
					y--; x--; maxy--; scandir++;
				}
				break;
			case 2:
				x--;
				if (x < minx) {
					x++; y--; minx++; scandir++;
				}
				break;
			case 3:
				y--;
				if (y < miny) {
					y++; x++; miny++; scandir=0;
				}
				break;
		}
	}
#ifdef DEBUG
	char name[64];
	sprintf(name, "dump%05d.bin", tframe++);
	FILE *f = fopen(name,"wb");
	fwrite(dbg, h, s*3, f);
	fclose(f);
#endif
	//printf("Dup'd: %d\n", dpcnt);
	return objects;
}

