#include "libol.h"
#include <math.h>
#include <string.h>

#include "trace.h"

#define ABS(a) ((a)<0?(-(a)):(a))

#define OVERDRAW 8

int trace(int *field, uint8_t *tmp, int thresh, int w, int h, int decimate)
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

