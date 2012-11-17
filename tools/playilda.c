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
Simple bare-bones ILDA file player. Works directly on JACK. libol does (now)
have ILDA loading and display, but that goes through quite a bit of processing.
This bare-bones player should accurately display ILDA files at the target sample
rate or, if slower, performs simple resampling. Thus, it works well for things
like the ILDA test pattern.

Oh, and it also tries to watch the input file and reload if it changes. Nice
if you make a shell loop to convert SVG to ILD every time the SVG changes (but
use a temp file and rename the ILD at the end, otherwise this breaks horribly
on partial files). Then you can have Inkscape open and every time you save
the laser image updates.
*/

#define _BSD_SOURCE

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <jack/jack.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#if BYTE_ORDER == LITTLE_ENDIAN
static inline uint16_t swapshort(uint16_t v) {
	return (v >> 8) | (v << 8);
}
# define MAGIC 0x41444C49
#else
static inline uint16_t swapshort(uint16_t v) {
	return v;
}
# define MAGIC 0x494C4441
#endif

#include <stdint.h>

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;

jack_port_t *out_x;
jack_port_t *out_y;
jack_port_t *out_z;
jack_port_t *out_r;
jack_port_t *out_g;
jack_port_t *out_b;
jack_port_t *out_w;

nframes_t rate;
nframes_t divider = 1;

nframes_t pointrate = 30000;

int scale = 0;

sample_t size = 1.0;

struct ilda_hdr {
	uint32_t magic;
	uint8_t pad1[3];
	uint8_t format;
	char name[8];
	char company[8];
	uint16_t count;
	uint16_t frameno;
	uint16_t framecount;
	uint8_t scanner;
	uint8_t pad2;
} __attribute__((packed));

struct color {
	uint8_t r, g, b;
};

#define BLANK 0x40
#define LAST 0x80

struct icoord3d {
	int16_t x;
	int16_t y;
	int16_t z;
	uint8_t state;
	uint8_t color;
} __attribute__((packed));

struct icoord2d {
	int16_t x;
	int16_t y;
	uint8_t state;
	uint8_t color;
} __attribute__((packed));

struct coord3d {
	int16_t x;
	int16_t y;
	int16_t z;
	uint8_t state;
	struct color color;
};

struct frame {
	struct coord3d *points;
	int position;
	int count;
};

#define FRAMEBUFS 10
struct frame frames[FRAMEBUFS];

struct frame * volatile curframe;
struct frame *curdframe;

int subpos;

struct color palette[256] = {
	{  0,   0,   0}, {255, 255, 255}, {255,   0,   0}, {255, 255,   0},
	{  0, 255,   0}, {  0, 255, 255}, {  0,   0, 255}, {255,   0, 255},
	{255, 128, 128}, {255, 140, 128}, {255, 151, 128}, {255, 163, 128},
	{255, 174, 128}, {255, 186, 128}, {255, 197, 128}, {255, 209, 128},
	{255, 220, 128}, {255, 232, 128}, {255, 243, 128}, {255, 255, 128},
	{243, 255, 128}, {232, 255, 128}, {220, 255, 128}, {209, 255, 128},
	{197, 255, 128}, {186, 255, 128}, {174, 255, 128}, {163, 255, 128},
	{151, 255, 128}, {140, 255, 128}, {128, 255, 128}, {128, 255, 140},
	{128, 255, 151}, {128, 255, 163}, {128, 255, 174}, {128, 255, 186},
	{128, 255, 197}, {128, 255, 209}, {128, 255, 220}, {128, 255, 232},
	{128, 255, 243}, {128, 255, 255}, {128, 243, 255}, {128, 232, 255},
	{128, 220, 255}, {128, 209, 255}, {128, 197, 255}, {128, 186, 255},
	{128, 174, 255}, {128, 163, 255}, {128, 151, 255}, {128, 140, 255},
	{128, 128, 255}, {140, 128, 255}, {151, 128, 255}, {163, 128, 255},
	{174, 128, 255}, {186, 128, 255}, {197, 128, 255}, {209, 128, 255},
	{220, 128, 255}, {232, 128, 255}, {243, 128, 255}, {255, 128, 255},
	{255, 128, 243}, {255, 128, 232}, {255, 128, 220}, {255, 128, 209},
	{255, 128, 197}, {255, 128, 186}, {255, 128, 174}, {255, 128, 163},
	{255, 128, 151}, {255, 128, 140}, {255,   0,   0}, {255,  23,   0},
	{255,  46,   0}, {255,  70,   0}, {255,  93,   0}, {255, 116,   0},
	{255, 139,   0}, {255, 162,   0}, {255, 185,   0}, {255, 209,   0},
	{255, 232,   0}, {255, 255,   0}, {232, 255,   0}, {209, 255,   0},
	{185, 255,   0}, {162, 255,   0}, {139, 255,   0}, {116, 255,   0},
	{ 93, 255,   0}, { 70, 255,   0}, { 46, 255,   0}, { 23, 255,   0},
	{  0, 255,   0}, {  0, 255,  23}, {  0, 255,  46}, {  0, 255,  70},
	{  0, 255,  93}, {  0, 255, 116}, {  0, 255, 139}, {  0, 255, 162},
	{  0, 255, 185}, {  0, 255, 209}, {  0, 255, 232}, {  0, 255, 255},
	{  0, 232, 255}, {  0, 209, 255}, {  0, 185, 255}, {  0, 162, 255},
	{  0, 139, 255}, {  0, 116, 255}, {  0,  93, 255}, {  0,  70, 255},
	{  0,  46, 255}, {  0,  23, 255}, {  0,   0, 255}, { 23,   0, 255},
	{ 46,   0, 255}, { 70,   0, 255}, { 93,   0, 255}, {116,   0, 255},
	{139,   0, 255}, {162,   0, 255}, {185,   0, 255}, {209,   0, 255},
	{232,   0, 255}, {255,   0, 255}, {255,   0, 232}, {255,   0, 209},
	{255,   0, 185}, {255,   0, 162}, {255,   0, 139}, {255,   0, 116},
	{255,   0,  93}, {255,   0,  70}, {255,   0,  46}, {255,   0,  23},
	{128,   0,   0}, {128,  12,   0}, {128,  23,   0}, {128,  35,   0},
	{128,  47,   0}, {128,  58,   0}, {128,  70,   0}, {128,  81,   0},
	{128,  93,   0}, {128, 105,   0}, {128, 116,   0}, {128, 128,   0},
	{116, 128,   0}, {105, 128,   0}, { 93, 128,   0}, { 81, 128,   0},
	{ 70, 128,   0}, { 58, 128,   0}, { 47, 128,   0}, { 35, 128,   0},
	{ 23, 128,   0}, { 12, 128,   0}, {  0, 128,   0}, {  0, 128,  12},
	{  0, 128,  23}, {  0, 128,  35}, {  0, 128,  47}, {  0, 128,  58},
	{  0, 128,  70}, {  0, 128,  81}, {  0, 128,  93}, {  0, 128, 105},
	{  0, 128, 116}, {  0, 128, 128}, {  0, 116, 128}, {  0, 105, 128},
	{  0,  93, 128}, {  0,  81, 128}, {  0,  70, 128}, {  0,  58, 128},
	{  0,  47, 128}, {  0,  35, 128}, {  0,  23, 128}, {  0,  12, 128},
	{  0,   0, 128}, { 12,   0, 128}, { 23,   0, 128}, { 35,   0, 128},
	{ 47,   0, 128}, { 58,   0, 128}, { 70,   0, 128}, { 81,   0, 128},
	{ 93,   0, 128}, {105,   0, 128}, {116,   0, 128}, {128,   0, 128},
	{128,   0, 116}, {128,   0, 105}, {128,   0,  93}, {128,   0,  81},
	{128,   0,  70}, {128,   0,  58}, {128,   0,  47}, {128,   0,  35},
	{128,   0,  23}, {128,   0,  12}, {255, 192, 192}, {255,  64,  64},
	{192,   0,   0}, { 64,   0,   0}, {255, 255, 192}, {255, 255,  64},
	{192, 192,   0}, { 64,  64,   0}, {192, 255, 192}, { 64, 255,  64},
	{  0, 192,   0}, {  0,  64,   0}, {192, 255, 255}, { 64, 255, 255},
	{  0, 192, 192}, {  0,  64,  64}, {192, 192, 255}, { 64,  64, 255},
	{  0,   0, 192}, {  0,   0,  64}, {255, 192, 255}, {255,  64, 255},
	{192,   0, 192}, { 64,   0,  64}, {255,  96,  96}, {255, 255, 255},
	{245, 245, 245}, {235, 235, 235}, {224, 224, 224}, {213, 213, 213},
	{203, 203, 203}, {192, 192, 192}, {181, 181, 181}, {171, 171, 171},
	{160, 160, 160}, {149, 149, 149}, {139, 139, 139}, {128, 128, 128},
	{117, 117, 117}, {107, 107, 107}, { 96,  96,  96}, { 85,  85,  85},
	{ 75,  75,  75}, { 64,  64,  64}, { 53,  53,  53}, { 43,  43,  43},
	{ 32,  32,  32}, { 21,  21,  21}, { 11,  11,  11}, {  0,   0,   0},
};

#define MONOCHROME

int process (nframes_t nframes, void *arg)
{
	struct frame *frame = curdframe;

	sample_t *o_x = (sample_t *) jack_port_get_buffer (out_x, nframes);
	sample_t *o_y = (sample_t *) jack_port_get_buffer (out_y, nframes);
	sample_t *o_z = (sample_t *) jack_port_get_buffer (out_z, nframes);
	sample_t *o_r = (sample_t *) jack_port_get_buffer (out_r, nframes);
	sample_t *o_g = (sample_t *) jack_port_get_buffer (out_g, nframes);
	sample_t *o_b = (sample_t *) jack_port_get_buffer (out_b, nframes);
	sample_t *o_w = (sample_t *) jack_port_get_buffer (out_w, nframes);

	nframes_t frm;
	for (frm = 0; frm < nframes; frm++) {
		struct coord3d *c = &frame->points[frame->position];
		*o_x++ = (c->x / 32768.0) * size;
		*o_y++ = (c->y / 32768.0) * size;
		*o_z++ = (c->z / 32768.0) * size;
		if(c->state & BLANK) {
			*o_r++ = *o_g++ = *o_b++ = *o_w++ = 0.0;
		} else {
#ifdef MONOCHROME
			*o_r++ = *o_g++ = *o_b++ = *o_w++ = 1.0;
#else
			*o_r = c->color.r / 255.0;
			*o_g = c->color.g / 255.0;
			*o_b = c->color.b / 255.0;
			*o_w++ = 0.333 * *o_r++ + 0.333 * *o_g++ + 0.333 * *o_b++;
			//*o_w++ = 0.2126 * *o_r++ + 0.7152 * *o_g++ + 0.0722 * *o_b++;
#endif
		}
		subpos++;
		if (subpos == divider) {
			subpos = 0;
			if (c->state & LAST)
				frame->position = 0;
			else
				frame->position = (frame->position + 1) % frame->count;
		}
		if (frame->position == 0) {
			if(curdframe != curframe) {
				printf("Frame update\n");
				curdframe = curframe;
			}
		}
	}

	return 0;
}

int bufsize (nframes_t nframes, void *arg)
{
	printf ("the maximum buffer size is now %u\n", nframes);
	return 0;
}

int srate (nframes_t nframes, void *arg)
{
	rate = nframes;
	printf ("Playing back at %u pps\n", rate / divider);
	return 0;
}

void jack_shutdown (void *arg)
{
	exit (1);
}

int clamp(int v)
{
	if(v > 32767) {
		printf("warn: val is %d\n", v);
		return 32767;
	}
	if(v < -32768) {
		printf("warn: val is %d\n", v);
		return -32768;
	}
	return v;
}

int loadild(const char *fname, struct frame *frame)
{
	int i;
	FILE *ild = fopen(fname, "rb");

	int minx = 65536, maxx = -65536;
	int miny = 65536, maxy = -65536;
	int minz = 65536, maxz = -65536;

	if (!ild) {
		fprintf(stderr, "cannot open %s\n", fname);
		return -1;
	}

	frame->count = 0;
	memset(frame, 0, sizeof(struct frame));

	while(!frame->count) {

		struct ilda_hdr hdr;

		if (fread(&hdr, sizeof(hdr), 1, ild) != 1) {
			fprintf(stderr, "error while reading file\n");
			return -1;
		}

		if (hdr.magic != MAGIC) {
			fprintf(stderr, "Invalid magic 0x%08x\n", hdr.magic);
			return -1;
		}

		hdr.count = swapshort(hdr.count);
		hdr.frameno = swapshort(hdr.frameno);
		hdr.framecount = swapshort(hdr.framecount);

		switch (hdr.format) {
		case 0:
			printf("Got 3D frame, %d points\n", hdr.count);
			frame->points = malloc(sizeof(struct coord3d) * hdr.count);
			struct icoord3d *tmp3d = malloc(sizeof(struct icoord3d) * hdr.count);
			if (fread(tmp3d, sizeof(struct icoord3d), hdr.count, ild) != hdr.count) {
				fprintf(stderr, "error while reading frame\n");
				return -1;
			}
			for(i=0; i<hdr.count; i++) {
				frame->points[i].x = swapshort(tmp3d[i].x);
				frame->points[i].y = swapshort(tmp3d[i].y);
				frame->points[i].z = swapshort(tmp3d[i].z);
				frame->points[i].state = tmp3d[i].state;
				frame->points[i].color = palette[tmp3d[i].color];
			}
			free(tmp3d);
			frame->count = hdr.count;
			break;
		case 1:
			printf("Got 2D frame, %d points\n", hdr.count);
			frame->points = malloc(sizeof(struct coord3d) * hdr.count);
			struct icoord2d *tmp2d = malloc(sizeof(struct icoord2d) * hdr.count);
			if (fread(tmp2d, sizeof(struct icoord2d), hdr.count, ild) != hdr.count) {
				fprintf(stderr, "error while reading frame\n");
				return -1;
			}
			for(i=0; i<hdr.count; i++) {
				frame->points[i].x = swapshort(tmp2d[i].x);
				frame->points[i].y = swapshort(tmp2d[i].y);
				frame->points[i].z = 0;
				frame->points[i].state = tmp2d[i].state;
				frame->points[i].color = palette[tmp2d[i].color];
			}
			free(tmp2d);
			frame->count = hdr.count;
			break;
		case 2:
			printf("Got color palette section, %d entries\n", hdr.count);
			if (fread(palette, 3, hdr.count, ild) != hdr.count) {
				fprintf(stderr, "error while reading palette\n");
				return -1;
			}
			break;
		}
	}

	fclose(ild);

	if (scale) {
		for(i=0; i<frame->count; i++) {
			if(frame->points[i].x > maxx)
				maxx = frame->points[i].x;
			if(frame->points[i].y > maxy)
				maxy = frame->points[i].y;
			if(frame->points[i].z > maxz)
				maxz = frame->points[i].z;
			if(frame->points[i].x < minx)
				minx = frame->points[i].x;
			if(frame->points[i].y < miny)
				miny = frame->points[i].y;
			if(frame->points[i].z < minz)
				minz = frame->points[i].z;
		}

		int dx, dy, dz, dmax;
		int cx, cy, cz;
		dx = maxx-minx;
		dy = maxy-miny;
		dz = maxz-minz;
		cx = (minx+maxx)/2;
		cy = (miny+maxy)/2;
		cz = (minz+maxz)/2;

		dmax = dx;
		if (dy > dmax)
			dmax = dy;
		if (dz > dmax)
			dmax = dz;

		printf("X range: %d .. %d (%d) (c:%d)\n", minx, maxx, dx, cx);
		printf("Y range: %d .. %d (%d) (c:%d)\n", miny, maxy, dy, cy);
		printf("Z range: %d .. %d (%d) (c:%d)\n", minz, maxz, dz, cz);

		printf("Scaling by %d\n", dmax);

		for(i=0; i<frame->count; i++) {
			frame->points[i].x -= cx;
			frame->points[i].y -= cy;
			frame->points[i].z -= cz;

			frame->points[i].x = clamp((int)frame->points[i].x * 32767 / (dmax/2+1));
			frame->points[i].y = clamp((int)frame->points[i].y * 32767 / (dmax/2+1));
			frame->points[i].z = clamp((int)frame->points[i].z * 32767 / (dmax/2+1));
		}

	}

	printf("Resampling %d -> %d\n", pointrate, rate);

	if (pointrate > rate) {
		printf("Downsampling not implemented!\n");
		return -1;
	}

	int ocount = frame->count * rate / pointrate;
	struct coord3d *opoints = malloc(sizeof(struct coord3d) * ocount);

	float mul = (float)frame->count / (float)ocount;

	for(i=0; i<ocount; i++) {
		float pos = i*mul;
		int lpos = pos;
		int rpos = pos+1;
		if (rpos >= frame->count)
			rpos = lpos;
		float off = pos - lpos;
		opoints[i].x = frame->points[lpos].x * (1-off) + frame->points[rpos].x * off;
		opoints[i].y = frame->points[lpos].y * (1-off) + frame->points[rpos].y * off;
		opoints[i].z = frame->points[lpos].z * (1-off) + frame->points[rpos].z * off;
		opoints[i].color.r = frame->points[lpos].color.r * (1-off) + frame->points[rpos].color.r * off;
		opoints[i].color.g = frame->points[lpos].color.g * (1-off) + frame->points[rpos].color.g * off;
		opoints[i].color.b = frame->points[lpos].color.b * (1-off) + frame->points[rpos].color.b * off;
		opoints[i].state = frame->points[lpos].state | frame->points[rpos].state;
	}

	free(frame->points);
	frame->points = opoints;
	frame->count = ocount;

	return 0;
}

int main (int argc, char *argv[])
{
	int frameno = 0;
	char **argvp = &argv[1];
	char *fname;
	jack_client_t *client;
	char jack_client_name[] = "playilda";
	jack_status_t jack_status;	
	jack_options_t  jack_options = JackNullOption;	
	struct stat st1, st2;

	if (argc > 2 && !strcmp(argvp[0],"-s")) {
		scale = 1;
		argc--;
		argvp++;
	}

	if (argc < 2) {
		fprintf(stderr, "usage: %s [-s] filename.ild [rate]\n", argv[0]);
		return 1;
	}

	fname = *argvp;

	if (argc > 2) {
		pointrate = atoi(argvp[1]);
	}

	if ((client = jack_client_open(jack_client_name, jack_options, &jack_status)) == 0) {
		fprintf (stderr, "jack server not running?\n");
		return 1;
	}

	jack_set_process_callback (client, process, 0);
	jack_set_buffer_size_callback (client, bufsize, 0);
	jack_set_sample_rate_callback (client, srate, 0);
	jack_on_shutdown (client, jack_shutdown, 0);

	out_x = jack_port_register (client, "out_x", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_y = jack_port_register (client, "out_y", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_z = jack_port_register (client, "out_z", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_r = jack_port_register (client, "out_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_g = jack_port_register (client, "out_g", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_b = jack_port_register (client, "out_b", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_w = jack_port_register (client, "out_w", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	memset(frames, 0, sizeof(frames));

	curframe = curdframe = &frames[frameno];
	if (loadild(fname, curframe) < 0)
	{
		return 1;
	}

	stat(fname, &st1);

	subpos = 0;

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		return 1;
	}

	while (1) {
		stat(fname, &st2);
		if(st1.st_mtim.tv_sec != st2.st_mtim.tv_sec || st1.st_mtim.tv_nsec != st2.st_mtim.tv_nsec) {
			frameno = (frameno+1)%FRAMEBUFS;
			printf("Loading new frame to slot %d\n", frameno);
			if(frames[frameno].points)
				free(frames[frameno].points);
			loadild(fname, &frames[frameno]);
			printf("New frame loaded\n");
			curframe = &frames[frameno];
			memcpy(&st1, &st2, sizeof(st1));
		}
		usleep(50000);
	}
	jack_client_close (client);
	exit (0);
}

