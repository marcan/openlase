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
#include <stdlib.h>

#include "trace.h"

struct OLTraceCtx {
	OLTraceParams p;
	uint16_t *tracebuf;

	OLTracePoint *sb;
	OLTracePoint *sbp;
	OLTracePoint *sb_end;
	unsigned int sb_size;

	OLTracePoint *pb;
	OLTracePoint *pbp;
	OLTracePoint *pb_end;
	unsigned int pb_size;
};

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

static inline void add_bufpoint(OLTraceCtx *ctx, icoord x, icoord y)
{
	ctx->pbp->x = x;
	ctx->pbp->y = y;
	ctx->pbp++;
	if (ctx->pbp == ctx->pb_end) {
		unsigned int cur = ctx->pbp - ctx->pb;
		ctx->pb_size *= 2;
		ctx->pb = realloc(ctx->pb, ctx->pb_size * sizeof(*ctx->pb));
		ctx->pbp = ctx->pb + cur;
		ctx->pb_end = ctx->pb + ctx->pb_size;
	}
}

static inline void add_startpoint(OLTraceCtx *ctx, icoord x, icoord y)
{
	ctx->sbp->x = x;
	ctx->sbp->y = y;
	ctx->sbp++;
	if (ctx->sbp == ctx->sb_end) {
		unsigned int cur = ctx->sbp - ctx->sb;
		ctx->sb_size *= 2;
		ctx->sb = realloc(ctx->sb, ctx->sb_size * sizeof(*ctx->sb));
		ctx->sbp = ctx->sb + cur;
		ctx->sb_end = ctx->sb + ctx->sb_size;
	}
}

static int trace_pixels(OLTraceCtx *ctx, uint16_t *buf, int output, icoord *cx, icoord *cy, uint16_t flag)
{
	icoord x = *cx;
	icoord y = *cy;
	icoord s = ctx->p.width;
	unsigned int iters = 0;
	int start = 1;
#ifdef DEBUG
	if (decimate != -1) {
		pc = pc + 47;
		pc %= 160;
	}
#endif
	unsigned int lidx = 0;
	unsigned int dir = 0;
	while (1)
	{
		unsigned int idx = y*s+x;
		if (output)
			add_bufpoint(ctx, x, y);
		iters++;
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
				unsigned int adir = (dir + 1) % 8;
				buf[idx+tdx[adir]+s*tdy[adir]] &= ~flag;
				adir = (dir + 7) % 8;
				buf[idx+tdx[adir]+s*tdy[adir]] &= ~flag;
			}
		}
#ifdef DEBUG
		if (!start)
			dbg[idx][2] = 96+pc;
#endif
		start = 0;
		lidx = idx;
	}
#ifdef DEBUG
/*
	dbg[lidx][0] = 255;
	dbg[lidx][1] = 0;
	dbg[lidx][2] = 0;
*/
#endif
	*cx = x;
	*cy = y;
	return iters;
}

static void alloc_bufs(OLTraceCtx *ctx)
{
	ctx->tracebuf = malloc(ctx->p.width * ctx->p.height * sizeof(*ctx->tracebuf));

	ctx->sb_size = ctx->p.width * 16;
	ctx->sb = malloc(ctx->sb_size * sizeof(*ctx->sb));
	ctx->sbp = ctx->sb;
	ctx->sb_end = ctx->sb + ctx->sb_size;

	ctx->pb_size = ctx->p.width * 16;
	ctx->pb = malloc(ctx->pb_size * sizeof(*ctx->pb));
	ctx->pbp = ctx->pb;
	ctx->pb_end = ctx->pb + ctx->pb_size;
}

static void free_bufs(OLTraceCtx *ctx)
{
	if (ctx->tracebuf)
		free(ctx->tracebuf);
	if (ctx->sb)
		free(ctx->sb);
	if (ctx->pb)
		free(ctx->pb);
}

int olTraceInit(OLTraceCtx **pctx, OLTraceParams *params)
{
	OLTraceCtx *ctx = malloc(sizeof(OLTraceCtx));

	ctx->p = *params;

	alloc_bufs(ctx);

	*pctx = ctx;
	return 0;
}

int olTraceReInit(OLTraceCtx *ctx, OLTraceParams *params)
{
	if (ctx->p.mode != params->mode ||
		ctx->p.width != params->width ||
		ctx->p.height != params->height)
	{
		free_bufs(ctx);
		ctx->p = *params;
		alloc_bufs(ctx);
	} else {
		ctx->p = *params;
	}
	return 0;
}

void olTraceFree(OLTraceResult *result)
{
	if (!result)
		return;
	if (result->objects) {
		if (result->objects[0].points)
			free(result->objects[0].points);
		free(result->objects);
	}
}

void olTraceDeinit(OLTraceCtx *ctx)
{
	if (!ctx)
		return;

	free_bufs(ctx);
	free(ctx);
}

static void find_edges_thresh(OLTraceCtx *ctx, uint8_t *src, unsigned int stride)
{
	unsigned int thresh = ctx->p.threshold;
	icoord x, y, w, h;
	w = ctx->p.width;
	h = ctx->p.height;

	for (y=2; y<h-2; y++) {
		for (x=2; x<w-2;x++) {
			int idx = y*stride+x;
			int oidx = y*w+x;
			if (src[idx] > thresh && (!(src[idx-stride] > thresh)
			                         || !(src[idx-1] > thresh))) {
				ctx->tracebuf[oidx] = 0xFFFF;
				add_startpoint(ctx, x, y);
#ifdef DEBUG
				dbg[oidx][0] = 64;
				dbg[oidx][1] = 64;
				dbg[oidx][2] = 64;
#endif
			}
			if (src[idx] <= thresh && (!(src[idx-stride] <= thresh)
			                         || !(src[idx-1] <= thresh))) {
				ctx->tracebuf[oidx] = 0xFFFF;
				add_startpoint(ctx, x, y);
#ifdef DEBUG
				dbg[oidx][0] = 64;
				dbg[oidx][1] = 64;
				dbg[oidx][2] = 64;
#endif
			}
		}
	}
}

int olTrace(OLTraceCtx *ctx, uint8_t *src, unsigned int stride, OLTraceResult *result)
{
	icoord x, y;
	unsigned int objects = 0;
	icoord w = ctx->p.width;
	icoord h = ctx->p.height;

	memset(ctx->tracebuf, 0, w*h*2);
#ifdef DEBUG
	memset(dbg, 0, 3*w*h);
#endif

	ctx->sbp = ctx->sb;
	ctx->pbp = ctx->pb;

	find_edges_thresh(ctx, src, stride);

	OLTracePoint *ps = ctx->sb;
	while (ps != ctx->sbp) {
		x = ps->x;
		y = ps->y;
		ps++;
		uint16_t flg = 1;
		while (ctx->tracebuf[y*w+x] & 0x8000) {
			icoord tx = x, ty = y;
			if (flg != 64)
				flg <<= 1;
			trace_pixels(ctx, ctx->tracebuf, 0, &tx, &ty, flg);
#ifdef DEBUG
			icoord sx = tx, sy = ty;
#endif
			if (trace_pixels(ctx, ctx->tracebuf, 1, &tx, &ty, 0xFFFF)) {
				ctx->pbp[-1].x |= 1<<31;
				objects++;
			}

#ifdef DEBUG
			dbg[y*w+x][0] = 255;
			dbg[y*w+x][1] = 255;
			dbg[y*w+x][2] = 0;

			dbg[sy*w+sx][0] = 0;
			dbg[sy*w+sx][1] = 255;
			dbg[sy*w+sx][2] = 255;

			dbg[ty*w+tx][0] = 255;
			dbg[ty*w+tx][1] = 0;
			dbg[ty*w+tx][2] = 0;
			printf("%d: %d,%d %d,%d %d,%d\n", tframe, x, y, sx, sy, tx, ty);
#endif
		}
	}
#ifdef DEBUG
	char name[64];
	sprintf(name, "dump%05d.bin", tframe++);
	FILE *f = fopen(name,"wb");
	fwrite(dbg, h, w*3, f);
	fclose(f);
#endif
	//printf("Dup'd: %d\n", dpcnt);

	result->count = objects;
	if (objects == 0) {
		result->objects = NULL;
	} else {
		OLTracePoint *p, *q;
		q = malloc((ctx->pbp - ctx->pb) * sizeof(*q));
		result->objects = malloc(objects * sizeof(*result->objects));
		OLTraceObject *o = result->objects;
		o->count = 0;
		o->points = q;
		for (p = ctx->pb; p != ctx->pbp; p++) {
			q->x = p->x & ~(1<<31);
			q->y = p->y;
			q++;
			o->count++;
			if (p->x & (1<<31)) {
				o++;
				if ((p+1) != ctx->pbp) {
					o->count = 0;
					o->points = q;
				}
			}
		}
	}

	return objects;
}
