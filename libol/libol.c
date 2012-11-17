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
#include <jack/jack.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;

static jack_port_t *out_x;
static jack_port_t *out_y;
static jack_port_t *out_r;
static jack_port_t *out_g;
static jack_port_t *out_b;
static jack_port_t *out_al;
static jack_port_t *out_ar;

static jack_client_t *client;

typedef struct {
	float x,y;
	uint32_t color;
} Point;

typedef struct {
	int pointcnt;
	Point *points;
} Object;

typedef struct {
	int objcnt;
	int objmax;
	Object *objects;
	int psmax;
	int psnext;
	Point *points;
} Frame;

typedef struct {
	int pmax;
	int pnext;
	Point *points;
	float *audio_l;
	float *audio_r;
} RenderedFrame;

static RenderedFrame *frames;
static nframes_t jack_rate;

static OLFrameInfo last_info;

static Frame wframe;

typedef struct {
	Object *curobj;
	Point last_point;
	Point last_slope;
	Point c1, c2;
	int prim;
	int state;
	int points;
} DrawState;

static DrawState dstate;

static OLRenderParams params;

static Point last_render_point;

static volatile int crbuf;
static volatile int cwbuf;
static int fbufs;
static int buflag;
static int out_point;
static int first_time_full;
static int first_output_frame;

float bbox[2][2];

#define MTX_STACK_DEPTH 16

int mtx2dp = 0;
float mtx2ds[MTX_STACK_DEPTH][3][3];
float mtx2d[3][3];

int mtx3dp = 0;
float mtx3ds[MTX_STACK_DEPTH][4][4];
float mtx3d[4][4];

int coldp = 0;
uint32_t cols[MTX_STACK_DEPTH];
uint32_t curcol;

#define POINT(x, y, color) ((Point){x,y,color})

ShaderFunc vpreshader;
ShaderFunc vshader;
Shader3Func v3shader;
ShaderFunc pshader;

AudioCallbackFunc audiocb;

LogCallbackFunc log_cb;

static uint32_t colmul(uint32_t a, uint32_t b)
{
    uint32_t out = 0;
    out |= ((a&0xff)*(b&0xff)) / 255;
    out |= ((((a>>8)&0xff)*((b>>8)&0xff)) / 255)<<8;
    out |= ((((a>>16)&0xff)*((b>>16)&0xff)) / 255)<<16;
    return out;
}

static Point *ps_alloc(int count)
{
	Point *ret;
	if ((count + wframe.psnext) > wframe.psmax) {
		olLog("Point buffer overflow (temp): need %d points, have %d\n", count + wframe.psnext, wframe.psmax);
		exit(1);
	}
	ret = wframe.points + wframe.psnext;
	wframe.psnext += count;
	return ret;
}

static int bufsize (nframes_t nframes, void *arg)
{
	olLog ("the maximum buffer size is now %u\n", nframes);
	return 0;
}

static int srate (nframes_t nframes, void *arg)
{
	jack_rate = nframes;
	olLog ("Playing back at %u Hz\n", jack_rate);
	return 0;
}

static void jack_shutdown (void *arg)
{
	olLog ("jack_shutdown\n");
}

static int process (nframes_t nframes, void *arg)
{
	sample_t *o_x = (sample_t *) jack_port_get_buffer (out_x, nframes);
	sample_t *o_y = (sample_t *) jack_port_get_buffer (out_y, nframes);
	sample_t *o_r = (sample_t *) jack_port_get_buffer (out_r, nframes);
	sample_t *o_g = (sample_t *) jack_port_get_buffer (out_g, nframes);
	sample_t *o_b = (sample_t *) jack_port_get_buffer (out_b, nframes);
	sample_t *o_al = (sample_t *) jack_port_get_buffer (out_al, nframes);
	sample_t *o_ar = (sample_t *) jack_port_get_buffer (out_ar, nframes);

	if (!first_time_full) {
		//olLog("Dummy frame!\n");
		memset(o_x, 0, nframes * sizeof(sample_t));
		memset(o_y, 0, nframes * sizeof(sample_t));
		memset(o_r, 0, nframes * sizeof(sample_t));
		memset(o_g, 0, nframes * sizeof(sample_t));
		memset(o_b, 0, nframes * sizeof(sample_t));
		memset(o_al, 0, nframes * sizeof(sample_t));
		memset(o_ar, 0, nframes * sizeof(sample_t));
		return 0;
	}

	while(nframes) {
		if (out_point == -1) {
			if (!first_output_frame) {
				//olLog("First frame! %d\n", crbuf);
				first_output_frame = 1;
			} else {
				if ((crbuf+1)%fbufs == cwbuf) {
					//olLog("Duplicated frame! %d\n", crbuf);
				} else {
					crbuf = (crbuf+1)%fbufs;
					//olLog("Normal frame! %d\n", crbuf);
				}
			}
			out_point = 0;
		}
		int count = nframes;
		int left = frames[crbuf].pnext - out_point;
		if (count > left)
			count = left;
		int i;
		for (i=0; i<count; i++) {
			Point *p = &frames[crbuf].points[out_point];
			*o_x++ = p->x;
			*o_y++ = p->y;

			*o_r++ = ((p->color >> 16) & 0xff) / 255.0f;
			*o_g++ = ((p->color >> 8) & 0xff) / 255.0f;
			*o_b++ = (p->color & 0xff) / 255.0f;

			*o_al++ = frames[crbuf].audio_l[out_point];
			*o_ar++ = frames[crbuf].audio_r[out_point];
			out_point++;
			//olLog("%06x %f %f\n", p->x, p->y, p->color);
		}
		if (out_point == frames[crbuf].pnext)
			out_point = -1;
		nframes -= count;
	}
	return 0;
}

int olInit(int buffer_count, int max_points)
{
	int i;
	char jack_client_name[] = "libol";
	jack_status_t jack_status;
	jack_options_t  jack_options = JackNullOption;		
	
	if (buffer_count < 2)
		return -1;

	memset(&dstate, 0, sizeof(dstate));
	memset(&last_render_point, 0, sizeof(last_render_point));

	buflag = buffer_count;
	fbufs = buffer_count+1;

	cwbuf = 0;
	crbuf = 0;
	out_point = -1;
	first_time_full = 0;
	first_output_frame = 0;
	memset(&wframe, 0, sizeof(Frame));
	wframe.objmax = 16;
	wframe.objects = malloc(wframe.objmax * sizeof(Object));
	wframe.psmax = max_points;
	wframe.points = malloc(wframe.psmax * sizeof(Point));
	frames = malloc(fbufs * sizeof(RenderedFrame));
	for (i=0; i<fbufs; i++) {
		memset(&frames[i], 0, sizeof(RenderedFrame));
		frames[i].pmax = max_points;
		frames[i].points = malloc(frames[i].pmax * sizeof(Point));
		frames[i].audio_l = malloc(frames[i].pmax * sizeof(float));
		frames[i].audio_r = malloc(frames[i].pmax * sizeof(float));
	}

	if ((client = jack_client_open(jack_client_name, jack_options, &jack_status)) == 0) {
		olLog ("jack server not running?\n");
		return -1;
	}

	jack_set_process_callback (client, process, 0);
	jack_set_buffer_size_callback (client, bufsize, 0);
	jack_set_sample_rate_callback (client, srate, 0);
	jack_on_shutdown (client, jack_shutdown, 0);

	out_x = jack_port_register (client, "out_x", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_y = jack_port_register (client, "out_y", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_r = jack_port_register (client, "out_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_g = jack_port_register (client, "out_g", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_b = jack_port_register (client, "out_b", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_al = jack_port_register (client, "out_al", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_ar = jack_port_register (client, "out_ar", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	if (jack_activate (client)) {
		olLog ("cannot activate client");
		return -1;
	}

	olLoadIdentity();
	for(i=0; i<MTX_STACK_DEPTH; i++)
		olPushMatrix();
	mtx2dp = 0;

	olLoadIdentity3();
	for(i=0; i<MTX_STACK_DEPTH; i++)
		olPushMatrix3();
	mtx3dp = 0;

	olResetColor();
	for(i=0; i<MTX_STACK_DEPTH; i++)
		olPushColor();
	coldp = 0;

	bbox[0][0] = -1;
	bbox[0][1] = -1;
	bbox[1][0] = 1;
	bbox[1][1] = 1;

	vpreshader = NULL;
	vshader = NULL;
	v3shader = NULL;
	pshader = NULL;
	audiocb = NULL;

	return 0;
}

void olSetRenderParams(OLRenderParams *sp)
{
	params = *sp;
}

void olGetRenderParams(OLRenderParams *sp)
{
	*sp = params;
}

void olShutdown(void)
{
	jack_client_close (client);
}

void olBegin(int prim)
{
	if (dstate.curobj)
		return;
	if (wframe.objmax == wframe.objcnt) {
		wframe.objmax *= 2;
		wframe.objects = realloc(wframe.objects, wframe.objmax * sizeof(Object));
	}
	dstate.curobj = wframe.objects + wframe.objcnt;
	memset(dstate.curobj, 0, sizeof(Object));
	dstate.curobj->points = wframe.points + wframe.psnext;
	dstate.prim = prim;
	dstate.state = 0;
	dstate.points = 0;
}

static int near(Point a, Point b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return sqrtf(dx*dx+dy*dy) <= params.snap;
}

static void addpoint(float x, float y, uint32_t color)
{
	Point *pnt = ps_alloc(1);
	pnt->x = x;
	pnt->y = y;
	pnt->color = color;
	dstate.curobj->pointcnt++;
}

static int get_dwell(float x, float y)
{
	if (dstate.points == 1) {
		return params.start_dwell;
	} else {
		float ex = dstate.last_point.x;
		float ey = dstate.last_point.y;
		float ecx = dstate.last_slope.x;
		float ecy = dstate.last_slope.y;
		float sx = ex;
		float sy = ey;
		float scx = x;
		float scy = y;
		float dex = ecx-ex;
		float dey = ecy-ey;
		float dsx = sx-scx;
		float dsy = sy-scy;
		float dot = dex*dsx + dey*dsy;
		float lens = sqrtf(dex*dex+dey*dey) * sqrtf(dsx*dsx+dsy*dsy);
		//olLog("%f,%f -> %f,%f -> %f,%f\n", ecx,ecy,ex,ey,x,y);
		if (lens == 0) {
			//olLog("deg cor\n");
			return params.corner_dwell;
		} else {
			dot = dot / lens;
			if (dot > params.curve_angle) {
				//olLog("curve\n");
				return params.curve_dwell;
			} else {
				//olLog("cor\n");
				return params.corner_dwell;
			}
		}
	}
}

static void line_to(float x, float y, uint32_t color)
{
	int dwell, i;
	//olLog("points: %d %d\n", dstate.points, dstate.curobj->pointcnt	);
	if (dstate.points == 0) {
		addpoint(x,y,color);
		dstate.points++;
		dstate.last_point = POINT(x,y,color);
		return;
	}
	dwell = get_dwell(x, y);
	Point last = dstate.last_point;
	for (i=0; i<dwell; i++)
		addpoint(last.x,last.y,last.color);
	float dx = x - last.x;
	float dy = y - last.y;
	float distance = fmaxf(fabsf(dx),fabsf(dy));
	int points = ceilf(distance/params.on_speed);
	for (i=1; i<=points; i++) {
		addpoint(last.x + (dx/(float)points) * i, last.y + (dy/(float)points) * i, color);
	}
	dstate.last_slope = dstate.last_point;
	dstate.last_point = POINT(x,y,color);
	dstate.points++;
}

static void recurse_bezier(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t color, int depth)
{
	float x0 = dstate.last_point.x;
	float y0 = dstate.last_point.y;
	int subdivide = 0;

	if (depth > 100) {
		olLog("Bezier recurse error: %f,%f %f,%f %f,%f %f,%f\n", x0, y0, x1, y1, x2, y2, x3, y3);
		return;
	}
	
	float dx = x3-x0;
	float dy = y3-y0;
	float distance = fmaxf(fabsf(dx),fabsf(dy));
	if (distance > params.on_speed) {
		subdivide = 1;
	} else {
		float ux = (3.0*x1 - 2.0*x0 - x3); ux = ux * ux;
		float uy = (3.0*y1 - 2.0*y0 - y3); uy = uy * uy;
		float vx = (3.0*x2 - 2.0*x3 - x0); vx = vx * vx;
		float vy = (3.0*y2 - 2.0*y3 - y0); vy = vy * vy;
		if (ux < vx)
			ux = vx;
		if (uy < vy)
			uy = vy;
		if ((ux+uy) > params.flatness)
			subdivide = 1;
	}

	if (subdivide) {
		//de Casteljau at t=0.5
		float mcx = (x1 + x2) * 0.5;
		float mcy = (y1 + y2) * 0.5;
		float ax1 = (x0 + x1) * 0.5;
		float ay1 = (y0 + y1) * 0.5;
		float ax2 = (ax1 + mcx) * 0.5;
		float ay2 = (ay1 + mcy) * 0.5;
		float bx2 = (x2 + x3) * 0.5;
		float by2 = (y2 + y3) * 0.5;
		float bx1 = (bx2 + mcx) * 0.5;
		float by1 = (by2 + mcy) * 0.5;
		float xm = (ax2 + bx1) * 0.5;
		float ym = (ay2 + by1) * 0.5;
		recurse_bezier(ax1, ay1, ax2, ay2, xm, ym, color, depth+1);
		recurse_bezier(bx1, by1, bx2, by2, x3, y3, color, depth+1);
	} else {
		addpoint(x3, y3, color);
		dstate.last_point = POINT(x3,y3,color);
	}
}

static void bezier_to(float x, float y, uint32_t color)
{
	int dwell, i;

	if (dstate.points == 0) {
		addpoint(x,y,color);
		dstate.points++;
		dstate.last_point = POINT(x,y,color);
		return;
	}

	switch(dstate.state) {
		case 0:
			dstate.c1 = POINT(x,y,color);
			dstate.state++;
			return;
		case 1:
			dstate.c2 = POINT(x,y,color);
			dstate.state++;
			return;
		case 2:
			break;
	}

	if (near(dstate.last_point, dstate.c1))
		dwell = get_dwell(dstate.c2.x, dstate.c2.y);
	else
		dwell = get_dwell(dstate.c1.x, dstate.c1.y);

	Point last = dstate.last_point;
	for (i=0; i<dwell; i++)
		addpoint(last.x,last.y,last.color);

	recurse_bezier(dstate.c1.x, dstate.c1.y, dstate.c2.x, dstate.c2.y, x, y, color, 0);

	dstate.last_point = POINT(x,y,color);
	if (near(dstate.c2, dstate.last_point))
		dstate.last_slope = dstate.c1;
	else
		dstate.last_slope = dstate.c2;
	dstate.points++;
	dstate.state = 0;
}


static void point_to(float x, float y, uint32_t color)
{
	int i;
	addpoint(x,y,color);
	if (dstate.points == 0)
		for (i=0; i<params.start_dwell; i++)
			addpoint(x,y,color);

	dstate.points++;
	return;
}

void olVertex(float x, float y, uint32_t color)
{
	if (!dstate.curobj)
		return;

	float nx, ny;

	if(vpreshader)
		vpreshader(&x, &y, &color);

	color = colmul(color,curcol);

	nx = mtx2d[0][0] * x + mtx2d[0][1] * y + mtx2d[0][2];
	ny = mtx2d[1][0] * x + mtx2d[1][1] * y + mtx2d[1][2];

	if(vshader)
		vshader(&nx, &ny, &color);

	switch (dstate.prim) {
		case OL_LINESTRIP:
			line_to(nx,ny,color);
			break;
		case OL_BEZIERSTRIP:
			bezier_to(nx,ny,color);
			break;
		case OL_POINTS:
			point_to(nx,ny,color);
			break;
	}
}

void olEnd(void)
{
	int i;
	if (!dstate.curobj)
		return;
	if (dstate.points < 2) {
		dstate.curobj = NULL;
		return;
	}
	Point *last = dstate.curobj->points + dstate.curobj->pointcnt - 1;
	for (i=0; i<params.end_dwell; i++)
		addpoint(last->x,last->y,last->color);

	if(pshader) {
		for (i=0; i<dstate.curobj->pointcnt; i++) {
			pshader(&dstate.curobj->points[i].x, &dstate.curobj->points[i].y, &dstate.curobj->points[i].color);
		}
	}

	int nl=0,nr=0,nu=0,nd=0;
	for (i=0; i<dstate.curobj->pointcnt; i++) {
		if (!dstate.curobj->points[i].color)
			continue;
		if (dstate.curobj->points[i].x > -1)
			nl = 1;
		if (dstate.curobj->points[i].x < 1)
			nr = 1;
		if (dstate.curobj->points[i].y > -1)
			nd = 1;
		if (dstate.curobj->points[i].y < 1)
			nu = 1;

		if (nl && nr && nu && nd)
			break;
	}
	if (nl && nr && nu && nd)
		wframe.objcnt++;
	dstate.curobj = NULL;
}

static void chkpts(int count)
{
	if (frames[cwbuf].pnext + count > frames[cwbuf].pmax) {
		olLog("Point buffer overflow (final): need %d points, have %d\n",
				count + frames[cwbuf].pnext, frames[cwbuf].pmax);
		exit(1);
	}
}

static void addrndpoint(float x, float y, uint32_t color)
{
	frames[cwbuf].points[frames[cwbuf].pnext].x = x;
	frames[cwbuf].points[frames[cwbuf].pnext].y = y;
	frames[cwbuf].points[frames[cwbuf].pnext].color = color;
	frames[cwbuf].pnext++;
}

static void render_object(Object *obj)
{
	int i,j;
	Point *start = &obj->points[0];
	Point *end = &obj->points[obj->pointcnt-1];
	float dx = start->x - last_render_point.x;
	float dy = start->y - last_render_point.y;
	float distance = fmaxf(fabsf(dx),fabsf(dy));
	int points = ceilf(distance/params.off_speed);
	chkpts(2 * (obj->pointcnt + params.start_wait + params.end_wait + points));
	Point *out_start = NULL;
	int skip_out_start_wait = 0;

	Point *ip = obj->points;
	for (i=0; i<obj->pointcnt; i++, ip++) {
		if (ip->x < bbox[0][0] || ip->x > bbox[1][0] ||
			ip->y < bbox[0][1] || ip->y > bbox[1][1])
			continue;
		if (ip->color != C_BLACK)
			break;
	}
	if (i == obj->pointcnt) // null object
		return;

	if (start->x < bbox[0][0] || start->x > bbox[1][0] ||
		start->y < bbox[0][1] || start->y > bbox[1][1]) {
		out_start = &last_render_point;
		skip_out_start_wait = 1;
	} else if (distance > params.snap) {
		for (i=0; i<points; i++) {
			addrndpoint(last_render_point.x + (dx/(float)points) * i,
						last_render_point.y + (dy/(float)points) * i,
						C_BLACK);
		}
		for (i=0; i<params.start_wait; i++) {
			addrndpoint(start->x, start->y, C_BLACK);
		}
	}
	Point *op = &frames[cwbuf].points[frames[cwbuf].pnext];
	ip = obj->points;
	for (i=0; i<obj->pointcnt; i++, ip++) {
		int inside = 1;
		if (ip->x < bbox[0][0] || ip->x > bbox[1][0] ||
			ip->y < bbox[0][1] || ip->y > bbox[1][1])
			inside = 0;
		if (!out_start) {
			if (inside) {
				*op++ = *ip;
				frames[cwbuf].pnext++;
			} else {
				out_start = ip;
				last_render_point = *ip;
			}
		} else if (inside) {
			if (!skip_out_start_wait) {
				for (j=0; j<params.end_wait; j++) {
					*op = *out_start;
					op->color = C_BLACK;
					op++;
					frames[cwbuf].pnext++;
				}
			}
			skip_out_start_wait = 0;
			float dx = ip->x - out_start->x;
			float dy = ip->y - out_start->y;
			float distance = fmaxf(fabsf(dx),fabsf(dy));
			int points = ceilf(distance/params.off_speed);
			if (distance > params.snap) {
				for (j=0; j<points; j++) {
					op->x = out_start->x + (dx/(float)points) * j;
					op->y = out_start->y + (dy/(float)points) * j;
					op->color = C_BLACK;
					op++;
					frames[cwbuf].pnext++;
				}
				for (j=0; j<params.start_wait; j++) {
					*op = *ip;
					op->color = C_BLACK;
					op++;
					frames[cwbuf].pnext++;
				}
			}
			*op++ = *ip;
			frames[cwbuf].pnext++;
			out_start = NULL;
		}
	}
	if(!out_start) {
		for (i=0; i<params.end_wait; i++) {
			addrndpoint(end->x, end->y, C_BLACK);
		}
		last_render_point = *end;
	} else {
		for (i=0; i<params.end_wait; i++) {
			addrndpoint(out_start->x, out_start->y, C_BLACK);
		}
		last_render_point = *out_start;
	}
}

float olRenderFrame(int max_fps)
{
	int i;
	int count = 0;

	int min_points = params.rate / max_fps;

	memset(&last_info, 0, sizeof(last_info));

	while (((cwbuf+1)%fbufs) == crbuf) {
		//olLog("Waiting %d %d\n", cwbuf, crbuf);
		usleep(1000);
		first_time_full = 1;
	}
	frames[cwbuf].pnext=0;
	int cnt = wframe.objcnt;
	float dclosest = 0;
	int clinv = 0;

	if (!(params.render_flags & RENDER_NOREORDER)) {
		Point closest_to = {-1,-1,0}; // first look for the object nearest the topleft
		while(cnt) {
			Object *closest = NULL;
			for (i=0; i<wframe.objcnt; i++) {
				if (!wframe.objects[i].pointcnt)
					continue;
				if (wframe.objects[i].pointcnt < params.min_length)
					continue;
				float dx = wframe.objects[i].points[0].x - closest_to.x;
				float dy = wframe.objects[i].points[0].y - closest_to.y;
				dx = wframe.objects[i].points[0].x + 1;
				dy = wframe.objects[i].points[0].y + 1;
				float distance = fmaxf(fabsf(dx),fabsf(dy)) + 0.01*(fabsf(dx)+fabsf(dy));
				if (!closest || distance < dclosest) {
					closest = &wframe.objects[i];
					clinv = 0;
					dclosest = distance;
				}
				if (!(params.render_flags & RENDER_NOREVERSE)) {
					dx = wframe.objects[i].points[wframe.objects[i].pointcnt-1].x - closest_to.x;
					dy = wframe.objects[i].points[wframe.objects[i].pointcnt-1].y - closest_to.y;
					distance = fmaxf(fabsf(dx),fabsf(dy)) + 0.01*(fabsf(dx)+fabsf(dy));
					if(!closest || distance < dclosest) {
						closest = &wframe.objects[i];
						clinv = 1;
						dclosest = distance;
					}
				}
			}
			if (!closest)
				break;
			if (clinv) {
				Point *pt = closest->points;
				int cnt = closest->pointcnt;
				for (i=0; i<cnt/2; i++) {
					Point tmp = pt[i];
					pt[i] = pt[cnt-i-1];
					pt[cnt-i-1] = tmp;
				}
			}
			//olLog("%d (%d) (nearest to %f,%f)\n", closest - wframe.objects, closest->pointcnt, closest_to.x, closest_to.y);
			render_object(closest);
			//olLog("[%d] ", frames[cwbuf].pnext);
			//olLog("[LRP:%f %f]\n", last_render_point.x, last_render_point.y);
			closest->pointcnt = 0;
			closest_to = last_render_point;
			cnt--;
			last_info.objects++;
		}
		//olLog("\n");
	} else {
		for (i=0; i<wframe.objcnt; i++) {
			if (wframe.objects[i].pointcnt < params.min_length)
				continue;
			render_object(&wframe.objects[i]);
		}
	}
	wframe.psnext = 0;
	wframe.objcnt = 0;
	count = frames[cwbuf].pnext;
	last_info.points = count;

	if (params.max_framelen && count > params.max_framelen)
	{
		int in_count = count;
		int out_count = params.max_framelen;
		chkpts(count);

		Point *pin = frames[cwbuf].points;
		Point *pout = &pin[in_count];

		float pos = 0;
		float delta = count / (float)out_count;

		count = 0;
		while (pos < (in_count - 1)) {
			int ipos = pos;
			float rest = pos - ipos;

			pout->x = pin[ipos].x * (1-rest) + pin[ipos+1].x * rest;
			pout->y = pin[ipos].y * (1-rest) + pin[ipos+1].y * rest;

			if (pin[ipos].color == C_BLACK || pin[ipos+1].color == C_BLACK) {
				pout->color = C_BLACK;
				pos += 1;
				last_info.resampled_blacks++;
			} else {
				pout->color = pin[ipos].color;
				pos += delta;
			}

			pout++;
			count++;
		}

		memcpy(pin, &pin[in_count], count * sizeof(*pin));
		frames[cwbuf].pnext = count;
		chkpts(0);
		last_info.resampled_points = count;
	}

	float last_x, last_y;
	if (count) {
		last_x = frames[cwbuf].points[count-1].x;
		last_y = frames[cwbuf].points[count-1].y;
	} else {
		last_x = last_y = 0;
	}
	while(count < min_points) {
		frames[cwbuf].points[count].x = last_x;
		frames[cwbuf].points[count].y = last_y;
		frames[cwbuf].points[count].color = C_BLACK;
		count++;
		last_info.padding_points++;
	}
	frames[cwbuf].pnext = count;

	if (audiocb) {
		audiocb(frames[cwbuf].audio_l, frames[cwbuf].audio_r, count);
	} else {
		memset(frames[cwbuf].audio_l, 0, sizeof(float)*count);
		memset(frames[cwbuf].audio_r, 0, sizeof(float)*count);
	}

	//olLog("Rendered frame! %d\n", cwbuf);
	cwbuf = (cwbuf + 1) % fbufs;

	return count / (float)params.rate;
}

void olLoadIdentity(void)
{
	static const float identity[3][3] = {
		{1,0,0},
		{0,1,0},
		{0,0,1}
	};
	memcpy(&mtx2d[0][0], &identity[0][0], sizeof(mtx2d));
}

void olRotate(float theta)
{
	float rot[9] = {
		cosf(theta),-sinf(theta),0,
		sinf(theta),cosf(theta),0,
		0,0,1,
	};
	olMultMatrix(rot);
}

void olTranslate(float x, float y)
{
	float trans[9] = {
		1,0,0,
		0,1,0,
		x,y,1,
	};
	olMultMatrix(trans);
}


void olScale(float sx, float sy)
{
	float scale[9] = {
		sx,0,0,
		0,sy,0,
		0,0,1,
	};
	olMultMatrix(scale);
}

void olMultMatrix(float m[9])
{
	float new[3][3];

	new[0][0] = mtx2d[0][0]*m[0] + mtx2d[0][1]*m[1] + mtx2d[0][2]*m[2];
	new[0][1] = mtx2d[0][0]*m[3] + mtx2d[0][1]*m[4] + mtx2d[0][2]*m[5];
	new[0][2] = mtx2d[0][0]*m[6] + mtx2d[0][1]*m[7] + mtx2d[0][2]*m[8];
	new[1][0] = mtx2d[1][0]*m[0] + mtx2d[1][1]*m[1] + mtx2d[1][2]*m[2];
	new[1][1] = mtx2d[1][0]*m[3] + mtx2d[1][1]*m[4] + mtx2d[1][2]*m[5];
	new[1][2] = mtx2d[1][0]*m[6] + mtx2d[1][1]*m[7] + mtx2d[1][2]*m[8];
	new[2][0] = mtx2d[2][0]*m[0] + mtx2d[2][1]*m[1] + mtx2d[2][2]*m[2];
	new[2][1] = mtx2d[2][0]*m[3] + mtx2d[2][1]*m[4] + mtx2d[2][2]*m[5];
	new[2][2] = mtx2d[2][0]*m[6] + mtx2d[2][1]*m[7] + mtx2d[2][2]*m[8];

	memcpy(&mtx2d[0][0], &new[0][0], sizeof(mtx2d));
}

void olPushMatrix(void)
{
	memcpy(&mtx2ds[mtx2dp][0][0], &mtx2d[0][0], sizeof(mtx2d));
	mtx2dp++;
}

void olPopMatrix(void)
{
	mtx2dp--;
	memcpy(&mtx2d[0][0], &mtx2ds[mtx2dp][0][0], sizeof(mtx2d));
}

void olLoadIdentity3(void)
{
	static const float identity[4][4] = {
		{1,0,0,0},
		{0,1,0,0},
		{0,0,1,0},
		{0,0,0,1},
	};
	memcpy(&mtx3d[0][0], &identity[0][0], sizeof(mtx3d));
}

void olRotate3X(float theta)
{
	float rot[16] = {
		1,0,0,0,
		0,cosf(theta),sinf(theta),0,
		0,-sinf(theta),cosf(theta),0,
		0,0,0,1
	};
	olMultMatrix3(rot);
}

void olRotate3Y(float theta)
{
	float rot[16] = {
		cosf(theta),0,-sinf(theta),0,
		0,1,0,0,
		sinf(theta),0,cosf(theta),0,
		0,0,0,1
	};
	olMultMatrix3(rot);
}

void olRotate3Z(float theta)
{
	float rot[16] = {
		cosf(theta), sinf(theta), 0, 0,
		-sinf(theta), cosf(theta), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	olMultMatrix3(rot);
}

void olTranslate3(float x, float y, float z)
{
	float trans[16] = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		x,y,z,1,
	};
	olMultMatrix3(trans);
}


void olScale3(float sx, float sy, float sz)
{
	float trans[16] = {
		sx,0,0,0,
		0,sy,0,0,
		0,0,sz,0,
		0,0,0,1,
	};
	olMultMatrix3(trans);
}

void olMultMatrix3(float m[16])
{
	float new[4][4];

	new[0][0] = mtx3d[0][0]*m[ 0] + mtx3d[0][1]*m[ 1] + mtx3d[0][2]*m[ 2] + mtx3d[0][3]*m[ 3];
	new[0][1] = mtx3d[0][0]*m[ 4] + mtx3d[0][1]*m[ 5] + mtx3d[0][2]*m[ 6] + mtx3d[0][3]*m[ 7];
	new[0][2] = mtx3d[0][0]*m[ 8] + mtx3d[0][1]*m[ 9] + mtx3d[0][2]*m[10] + mtx3d[0][3]*m[11];
	new[0][3] = mtx3d[0][0]*m[12] + mtx3d[0][1]*m[13] + mtx3d[0][2]*m[14] + mtx3d[0][3]*m[15];
	new[1][0] = mtx3d[1][0]*m[ 0] + mtx3d[1][1]*m[ 1] + mtx3d[1][2]*m[ 2] + mtx3d[1][3]*m[ 3];
	new[1][1] = mtx3d[1][0]*m[ 4] + mtx3d[1][1]*m[ 5] + mtx3d[1][2]*m[ 6] + mtx3d[1][3]*m[ 7];
	new[1][2] = mtx3d[1][0]*m[ 8] + mtx3d[1][1]*m[ 9] + mtx3d[1][2]*m[10] + mtx3d[1][3]*m[11];
	new[1][3] = mtx3d[1][0]*m[12] + mtx3d[1][1]*m[13] + mtx3d[1][2]*m[14] + mtx3d[1][3]*m[15];
	new[2][0] = mtx3d[2][0]*m[ 0] + mtx3d[2][1]*m[ 1] + mtx3d[2][2]*m[ 2] + mtx3d[2][3]*m[ 3];
	new[2][1] = mtx3d[2][0]*m[ 4] + mtx3d[2][1]*m[ 5] + mtx3d[2][2]*m[ 6] + mtx3d[2][3]*m[ 7];
	new[2][2] = mtx3d[2][0]*m[ 8] + mtx3d[2][1]*m[ 9] + mtx3d[2][2]*m[10] + mtx3d[2][3]*m[11];
	new[2][3] = mtx3d[2][0]*m[12] + mtx3d[2][1]*m[13] + mtx3d[2][2]*m[14] + mtx3d[2][3]*m[15];
	new[3][0] = mtx3d[3][0]*m[ 0] + mtx3d[3][1]*m[ 1] + mtx3d[3][2]*m[ 2] + mtx3d[3][3]*m[ 3];
	new[3][1] = mtx3d[3][0]*m[ 4] + mtx3d[3][1]*m[ 5] + mtx3d[3][2]*m[ 6] + mtx3d[3][3]*m[ 7];
	new[3][2] = mtx3d[3][0]*m[ 8] + mtx3d[3][1]*m[ 9] + mtx3d[3][2]*m[10] + mtx3d[3][3]*m[11];
	new[3][3] = mtx3d[3][0]*m[12] + mtx3d[3][1]*m[13] + mtx3d[3][2]*m[14] + mtx3d[3][3]*m[15];

	memcpy(&mtx3d[0][0], &new[0][0], sizeof(mtx3d));
}

void olPushMatrix3(void)
{
	memcpy(&mtx3ds[mtx3dp][0][0], &mtx3d[0][0], sizeof(mtx3d));
	mtx3dp++;
}

void olPopMatrix3(void)
{
	mtx3dp--;
	memcpy(&mtx3d[0][0], &mtx3ds[mtx3dp][0][0], sizeof(mtx3d));
}

void olTransformVertex3(float *x, float *y, float *z)
{
	float px;
	float py;
	float pz;
	float pw;

	px = mtx3d[0][0]**x + mtx3d[0][1]**y + mtx3d[0][2]**z + mtx3d[0][3];
	py = mtx3d[1][0]**x + mtx3d[1][1]**y + mtx3d[1][2]**z + mtx3d[1][3];
	pz = mtx3d[2][0]**x + mtx3d[2][1]**y + mtx3d[2][2]**z + mtx3d[2][3];
	pw = mtx3d[3][0]**x + mtx3d[3][1]**y + mtx3d[3][2]**z + mtx3d[3][3];

	px /= pw;
	py /= pw;
	pz /= pw;

	*x = px;
	*y = py;
	*z = pz;
}

void olVertex3(float x, float y, float z, uint32_t color)
{
	if(v3shader)
		v3shader(&x, &y, &z, &color);
	olTransformVertex3(&x, &y, &z);
	olVertex(x, y, color);
}

void olRect(float x1, float y1, float x2, float y2, uint32_t color)
{
	olBegin(OL_LINESTRIP);
	olVertex(x1,y1,color);
	olVertex(x1,y2,color);
	olVertex(x2,y2,color);
	olVertex(x2,y1,color);
	olVertex(x1,y1,color);
	olEnd();
}

void olLine(float x1, float y1, float x2, float y2, uint32_t color)
{
	olBegin(OL_LINESTRIP);
	olVertex(x1,y1,color);
	olVertex(x2,y2,color);
	olEnd();
}


void olDot(float x, float y, int samples, uint32_t color)
{
	int i;
	olBegin(OL_POINTS);
	for (i = 0; i < samples; i++)
		olVertex(x,y,color);
	olEnd();
}

void olResetColor(void)
{
	curcol = C_WHITE;
}

void olMultColor(uint32_t color)
{
	curcol = colmul(curcol, color);
}

void olPushColor(void)
{
	cols[coldp] = curcol;
	coldp++;
}

void olPopColor(void)
{
	coldp--;
	curcol = cols[coldp];
}


void olSetVertexPreShader(ShaderFunc f)
{
	vpreshader = f;
}
void olSetVertexShader(ShaderFunc f)
{
	vshader = f;
}
void olSetVertex3Shader(Shader3Func f)
{
	v3shader = f;
}

void olSetPixelShader(ShaderFunc f)
{
	pshader = f;
}

void olSetAudioCallback(AudioCallbackFunc f)
{
	audiocb = f;
}

void olFrustum (float l, float r, float b, float t, float n, float f)
{
	float m[16] = {
		(2*n)/(r-l),  0,            0,               0,
		0,            (2*n)/(t-b),  0,               0,
		(r+l)/(r-l),  (t+b)/(t-b),  -(f+n)/(f-n),   -1,
		0,            0,            (-2*f*n)/(f-n),  0,
	};

	olMultMatrix3(m);
}

void olPerspective (float fovy, float aspect, float zNear, float zFar)
{
	float xmin, xmax, ymin, ymax;

	ymax = zNear * tanf(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	olFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}

void olSetScissor (float x0, float y0, float x1, float y1)
{
	bbox[0][0] = x0;
	bbox[0][1] = y0;
	bbox[1][0] = x1;
	bbox[1][1] = y1;
}

void olGetFrameInfo(OLFrameInfo *info)
{
	*info = last_info;
}

void olLog(const char *fmt, ...)
{
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, 1024, fmt, ap);
	va_end(ap);
	buf[1023] = 0;

	if (log_cb)
		log_cb(buf);
	else
		printf("%s", buf);
}

void olSetLogCallback(LogCallbackFunc f)
{
	log_cb = f;
}
