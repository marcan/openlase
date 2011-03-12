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
#include <malloc.h>

#include "trace.h"

struct OLTraceCtx {
	OLTraceParams p;
	icoord aw, ah;
	uint16_t *k;
	unsigned int ksize, kpad;
	uint8_t *bibuf, *btbuf, *sibuf;
	int16_t *stbuf, *sxbuf, *sybuf;
	uint32_t *smbuf;

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

void ol_conv_sse2(uint8_t *src, uint8_t *dst, size_t w, size_t h, uint16_t *kern, size_t ksize);
void ol_sobel_sse2_gx_v(uint8_t *src, int16_t *dst, size_t w, size_t h);
void ol_sobel_sse2_gx_h(int16_t *src, int16_t *dst, size_t w, size_t h);
void ol_sobel_sse2_gy_v(uint8_t *src, int16_t *dst, size_t w, size_t h);
void ol_sobel_sse2_gy_h(int16_t *src, int16_t *dst, size_t w, size_t h);
void ol_transpose_2x8x8(uint8_t *p, size_t stride);
void ol_transpose_8x8w(int16_t *p, size_t stride);

typedef void (*sobel_v_fn)(uint8_t *src, int16_t *dst, size_t w, size_t h);
typedef void (*sobel_h_fn)(int16_t *src, int16_t *dst, size_t w, size_t h);

static void alloc_bufs(OLTraceCtx *ctx)
{
	ctx->aw = (ctx->p.width+15) & ~15;
	ctx->ah = (ctx->p.height+15) & ~15;

	ctx->ksize = ((int)round(ctx->p.sigma * 6 + 1)) | 1;

	if (ctx->ksize <= 1) {
		ctx->ksize = 0;
		ctx->k = NULL;
		ctx->kpad = 0;
		ctx->bibuf = NULL;
		ctx->btbuf = NULL;
		ctx->sibuf = NULL;
	} else {
	    ctx->k = memalign(64, 16 * ctx->ksize);
		ctx->kpad = ctx->ksize / 2;

		ctx->bibuf = memalign(64, ctx->aw * (ctx->ah + 2 * ctx->kpad));
		ctx->btbuf = memalign(64, ctx->ah * (ctx->aw + 2 * ctx->kpad));
		ctx->sibuf = memalign(64, ctx->aw * (ctx->ah + 2));
	}

	if (ctx->p.mode == OL_TRACE_CANNY) {
		if (!ctx->sibuf)
			ctx->sibuf = memalign(64, ctx->aw * (ctx->ah + 2));
		ctx->stbuf = memalign(64, sizeof(*ctx->stbuf) * ctx->ah * (ctx->aw + 2));
		ctx->sxbuf = memalign(64, sizeof(*ctx->sxbuf) * ctx->aw * ctx->ah);
		ctx->sybuf = memalign(64, sizeof(*ctx->sybuf) * ctx->aw * ctx->ah);
		ctx->smbuf = memalign(64, sizeof(*ctx->smbuf) * ctx->aw * ctx->ah);
	} else {
		ctx->stbuf = NULL;
		ctx->sxbuf = NULL;
		ctx->sybuf = NULL;
		ctx->smbuf = NULL;
	}

	ctx->tracebuf = malloc(ctx->p.width * ctx->p.height * sizeof(*ctx->tracebuf));
	memset(ctx->tracebuf, 0, ctx->p.width * ctx->p.height * sizeof(*ctx->tracebuf));	

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
	if (ctx->k)
		free(ctx->k);
	if (ctx->bibuf)
		free(ctx->bibuf);
	if (ctx->btbuf)
		free(ctx->btbuf);
	if (ctx->sibuf)
		free(ctx->sibuf);
	if (ctx->stbuf)
		free(ctx->stbuf);
	if (ctx->sxbuf)
		free(ctx->sxbuf);
	if (ctx->sybuf)
		free(ctx->sybuf);
	if (ctx->smbuf)
		free(ctx->smbuf);
}

static void init_blur(OLTraceCtx *ctx)
{
	if (ctx->ksize) {
		double scale = -0.5/(ctx->p.sigma*ctx->p.sigma);

		int i, j;
		double *pk = malloc(sizeof(double) * ctx->ksize);
		double sum = 0;
		for (i = 0; i < ctx->ksize; i++) {
			double x = i - (ctx->ksize-1)*0.5;
			pk[i] = exp(scale*x*x);
			sum += pk[i];
		}

		sum = 1/sum;

		for (i = 0; i < ctx->ksize; i++) {
			uint32_t val;
			val = 0x10000 * pk[i] * sum;
			if (val > 0xffff)
				val = 0xffff;
			for (j = 0; j < 8; j++) {
				ctx->k[i*8+j] = val;
			}
		}

		free(pk);
	}
}

void perform_blur(OLTraceCtx *ctx, uint8_t *src, icoord stride)
{
	unsigned int x, y, z;
	uint8_t *p, *q;

	p = ctx->bibuf;
	for (y = 0; y < ctx->kpad; y++) {
		memcpy(p, src, ctx->p.width);
		p += ctx->aw;
	}
	for (y = 0; y < ctx->p.height; y++) {
		memcpy(p, src, ctx->p.width);
		src += stride;
		p += ctx->aw;
	}
	src -= stride;
	for (y = 0; y < ctx->kpad; y++) {
		memcpy(p, src, ctx->p.width);
		p += ctx->aw;
	}

	p = ctx->bibuf;
	q = ctx->btbuf + ctx->ah * ctx->kpad;
	for (x = 0; x < ctx->aw; x += 16) {
		uint8_t *r = p;
		uint8_t *s = q;
		for (y = 0; y < ctx->p.height; y += 8) {
			uint8_t *t = s;
			for (z = 0; z < 4; z++) {
				ol_conv_sse2(r, t, ctx->aw, ctx->ah, ctx->k, ctx->ksize);
				r += 2*ctx->aw;
				t += 2*ctx->ah;
			}
			if (y&8) {
				ol_transpose_2x8x8(s-8, ctx->ah);
				ol_transpose_2x8x8(t-8, ctx->ah);
			}
			s += 8;
		}
		p += 16;
		q += 16 * ctx->ah;
	}
	p = ctx->btbuf;
	q = ctx->btbuf + ctx->ah * ctx->kpad;
	for (y = 0; y < ctx->kpad; y++) {
		memcpy(p, q, ctx->ah);
		p += ctx->ah;
	}
	p = ctx->btbuf + ctx->ah * (ctx->kpad + ctx->p.width);
	q = p - ctx->ah;
	for (y = 0; y < ctx->kpad; y++) {
		memcpy(p, q, ctx->ah);
		p += ctx->ah;
	}

	p = ctx->btbuf;
	q = ctx->sibuf + ctx->aw;
	for (x = 0; x < ctx->ah; x += 16) {
		uint8_t *r = p;
		uint8_t *s = q;
		for (y = 0; y < ctx->p.width; y += 8) {
			uint8_t *t = s;
			for (z = 0; z < 4; z++) {
				ol_conv_sse2(r, t, ctx->ah, ctx->aw, ctx->k, ctx->ksize);
				r += 2*ctx->ah;
				t += 2*ctx->aw;
			}
			if (y&8) {
				ol_transpose_2x8x8(s-8, ctx->aw);
				ol_transpose_2x8x8(t-8, ctx->aw);
			}
			s += 8;
		}
		p += 16;
		q += 16 * ctx->aw;
	}
}

static void perform_sobel(OLTraceCtx *ctx, sobel_v_fn vfn, sobel_h_fn hfn, int16_t *obuf)
{
	icoord x, y;
	uint8_t *bp;
	int16_t *p, *q;

	bp = ctx->sibuf + ctx->aw;
	memcpy(ctx->sibuf, bp, ctx->aw);
	bp += ctx->aw * ctx->p.height;
	memcpy(bp, bp - ctx->aw, ctx->aw);

	bp = ctx->sibuf;
	q = ctx->stbuf + ctx->ah;
	for (x = 0; x < ctx->aw; x += 16) {
		uint8_t *r = bp;
		int16_t *s = q;
		for (y = 0; y < ctx->p.height; y += 8) {
			vfn(r, s, ctx->aw, ctx->ah);
			ol_transpose_8x8w(s, 2*ctx->ah);
			ol_transpose_8x8w(s + 8*ctx->ah, 2*ctx->ah);
			r += 8*ctx->aw;
			s += 8;
		}
		bp += 16;
		q += 16 * ctx->ah;
	}
	p = ctx->stbuf + ctx->ah;
	memcpy(ctx->stbuf, p, 2 * ctx->ah);
	p += ctx->ah * ctx->p.width;
	memcpy(p, p - ctx->ah, 2 * ctx->ah);

	p = ctx->stbuf;
	q = obuf;
	for (x = 0; x < ctx->ah; x += 16) {
		int16_t *r = p;
		int16_t *s = q;
		for (y = 0; y < ctx->p.width; y += 8) {
			hfn(r, s, ctx->ah, ctx->aw);
			ol_transpose_8x8w(s, 2*ctx->aw);
			ol_transpose_8x8w(s + 8*ctx->aw, 2*ctx->aw);
			r += 8*ctx->ah;
			s += 8;
		}
		p += 16;
		q += 16 * ctx->aw;
	}
}

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

#define ABS(x) (((x)<0)?-(x):(x))

static void find_edges_canny(OLTraceCtx *ctx, uint8_t *src, unsigned int stride)
{
	icoord x, y;
	
	unsigned int high_t = ctx->p.threshold;
	unsigned int low_t = ctx->p.threshold2;
	if (low_t == 0)
		low_t = high_t;
	if (low_t > high_t) {
		unsigned int tmp = low_t;
		low_t = high_t;
		high_t = tmp;
	}
	
	if (src != ctx->sibuf) {
		uint8_t *p = ctx->sibuf + ctx->aw;
		for (y = 0; y < ctx->p.height; y++) {
			memcpy(p, src, ctx->p.width);
			src += stride;
			p += ctx->aw;
		}
	}

	perform_sobel(ctx, ol_sobel_sse2_gx_v, ol_sobel_sse2_gx_h, ctx->sxbuf);
	perform_sobel(ctx, ol_sobel_sse2_gy_v, ol_sobel_sse2_gy_h, ctx->sybuf);

	uint32_t *pm = ctx->smbuf;
	int16_t *px = ctx->sxbuf;
	int16_t *py = ctx->sybuf;
	unsigned int count = ctx->aw * ctx->p.height;

	while (count--) {
		*pm++ = ABS(*px) + ABS(*py);
		px++;
		py++;
	}

#define TAN45 0.41421356
#define ITAN45 ((int32_t)(TAN45*0x10000))

	int s = ctx->aw;

	for (y = 2; y < (ctx->p.height-2); y++) {
		uint16_t *pt = ctx->tracebuf + y*ctx->p.width + 2;
		px = ctx->sxbuf + y*ctx->aw + 2;
		py = ctx->sybuf + y*ctx->aw + 2;
		pm = ctx->smbuf + y*ctx->aw + 2;
		for (x = 1; x < (ctx->p.width-2); x++) {
			uint32_t gm = *pm;
			if (gm > low_t)  {
				int16_t gx = *px;
				int16_t gy = *py;
				int16_t sign = gx ^ gy;
				gx = ABS(gx);
				gy = ABS(gy);
				int32_t kgy = ITAN45*gy;
				if ((gx<<16) < kgy) {
					// horizontal edge [-]
					if (gm > pm[-s] && gm > pm[s]) {
						*pt = 0xffff;
						if (gm > high_t)
							add_startpoint(ctx, x, y);
					}
				} else if ((gx<<16) > (kgy+(gy<<17))) {
					// vertical edge [|]
					if (gm > pm[-1] && gm > pm[1]) {
						*pt = 0xffff;
						if (gm > high_t)
							add_startpoint(ctx, x, y);
					}
				} else if (sign < 0) {
					// diagonal edge [\]
					if (gm > pm[1-s] && gm > pm[s-1]) {
						*pt = 0xffff;
						if (gm > high_t)
							add_startpoint(ctx, x, y);
					}
				} else {
					// diagonal edge [/]
					if (gm > pm[-1-s] && gm > pm[s+1]) {
						*pt = 0xffff;
						if (gm > high_t)
							add_startpoint(ctx, x, y);
					}
				}
			}
			px++;
			py++;
			pm++;
			pt++;
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

	uint8_t *pbuf = src;
	if (ctx->ksize) {
		perform_blur(ctx, pbuf, stride);
		pbuf = ctx->sibuf;
		stride = ctx->aw;
	}
	switch (ctx->p.mode) {
		case OL_TRACE_THRESHOLD:
			find_edges_thresh(ctx, pbuf, stride);
			break;
		case OL_TRACE_CANNY:
			find_edges_canny(ctx, pbuf, stride);
			break;
	}

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


int olTraceInit(OLTraceCtx **pctx, OLTraceParams *params)
{
	OLTraceCtx *ctx = malloc(sizeof(OLTraceCtx));

	ctx->p = *params;

	alloc_bufs(ctx);
	init_blur(ctx);

	*pctx = ctx;
	return 0;
}

int olTraceReInit(OLTraceCtx *ctx, OLTraceParams *params)
{
	unsigned int new_ksize = ((unsigned int)round(params->sigma * 6 + 1)) | 1;

	if (ctx->p.mode != params->mode ||
		ctx->p.width != params->width ||
		ctx->p.height != params->height ||
		ctx->ksize != new_ksize)
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
