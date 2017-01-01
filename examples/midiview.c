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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <jack/jack.h>
#include <alsa/asoundlib.h>
#include <math.h>

int portid;

struct blip {
	int active;
	float ts;
	float age;
	float x, y;
	float r;
	float phase;
	float bright;
	uint32_t color;
	int note;
};

float notespeed = 1.0;
float avgnotes = 0.0;

uint32_t colors[16] = {
	0xFFFFFF,
	0xFF0000,
	0x00FFFF,
	0x0000FF,
	0xFFFF00,
	0x00FF00,
	0xFF00FF,
	0xFF8000,
	0xFFFF80,
	0x80FFFF,
	0xFF80FF,
	0x8080FF,
	0xFF8080,
	0x80FF80,
	0x0080FF,
	0x8000FF
};

#define NUM_BLIPS 32

int cur_blip = 0;
struct blip blips[NUM_BLIPS];

uint16_t channels = 0;

snd_seq_t *open_seq(void) {
	snd_seq_t *seq_handle;

	if (snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
		fprintf(stderr, "Error opening ALSA sequencer.\n");
		return NULL;
	}
	snd_seq_set_client_name(seq_handle, "midiview");
	if ((portid = snd_seq_create_simple_port(seq_handle, "midiview",
		SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
		SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
		fprintf(stderr, "Error creating sequencer port.\n");
		return NULL;
	}
	return seq_handle;
}

void note_on(int note, int velocity, int channel)
{
	float ts = avgnotes;
	if (ts > 20)
		ts = 20;

	blips[cur_blip].active = 1;
	blips[cur_blip].ts = ts;
	int xp = (note) % 12;
	blips[cur_blip].x = (xp / 6.0 - 1.0)* 0.6;
	blips[cur_blip].y = (2*note/127.0)-1.0;
	blips[cur_blip].r = 0.05;
	blips[cur_blip].bright = 0.3+(velocity / 128.0);
	blips[cur_blip].phase = 1;
	blips[cur_blip].note = 1;
	blips[cur_blip].color = colors[channel];

	cur_blip = (cur_blip + 1) % NUM_BLIPS;
}

int midi_action(snd_seq_t *seq_handle) {

	snd_seq_event_t *ev;
	int cnt = 0;

	do {
		snd_seq_event_input(seq_handle, &ev);
		switch (ev->type) {
			case SND_SEQ_EVENT_NOTEON:
				if (channels && !(channels & (1<<ev->data.note.channel)))
					break;
				if (ev->data.note.velocity != 0) {
					note_on(ev->data.note.note, ev->data.note.velocity, ev->data.note.channel);
					cnt++;
				}
			}
		snd_seq_free_event(ev);
	} while (snd_seq_event_input_pending(seq_handle, 0) > 0);
	return cnt;
}

#define CP 0.55228475f

void circle(float x, float y, float r, uint32_t color)
{
	olPushMatrix();
	olPushColor();
	olMultColor(color);
	olTranslate(x, y);
	olRotate((x+y)*123);
	/*olBegin(OL_BEZIERSTRIP);
	olVertex(0, r, color);
	int i;
	for (i=0; i<2; i++) {
		olVertex(CP*r, r, C_WHITE);
		olVertex(r, CP*r, C_WHITE);
		olVertex(r, 0, C_WHITE);
		olVertex(r, -CP*r, C_WHITE);
		olVertex(CP*r, -r, C_WHITE);
		olVertex(0, -r, C_WHITE);
		olVertex(-CP*r, -r, C_WHITE);
		olVertex(-r, -CP*r, C_WHITE);
		olVertex(-r, 0, C_WHITE);
		olVertex(-r, CP*r, C_WHITE);
		olVertex(-CP*r, r, C_WHITE);
		olVertex(0, r, C_WHITE);
	}*/
/*	olVertex(0, r, color);
	olVertex(0, r, color);
	olVertex(r*0.1, r, color);
	olVertex(r*0.1, r, color);*/

	float circum = 2 * M_PI * r;
	int segments = circum / (1/30.0);
	if (segments < 50)
		segments = 50;
	int i;
	olBegin(OL_POINTS);
	olVertex(r, 0, 0);
	for (i=0; i<=(2*segments+10); i++) {
		float w = i * M_PI * 2.0 / segments;
		uint32_t c = C_WHITE;
		if (i > 2*segments)
			c = C_GREY((10-(i-segments)) * 28);
		else if (i < 3)
			c = C_GREY(i * 85);
		olVertex(r*cosf(w), r*sinf(w), c);
	}
	olEnd();
	olPopColor();
	olPopMatrix();
}

int draw(void)
{
	int i;
	int cnt = 0;
	for (i=0; i<NUM_BLIPS; i++) {
		struct blip *b = &blips[i];
		if (b->active) {
			float br = b->bright * b->phase * 255.0;
			br = br > 255 ? 255 : br;
			olPushColor();
			olMultColor(b->color);
			uint32_t col = C_GREY((int)br);
			circle(b->x, b->y, b->r, col);
			olPopColor();
			cnt++;
		}
	}
	return cnt;
}

void animate(float time)
{
	int i;
	float ts = avgnotes;
	if (ts > 20)
		ts = 20;
	for (i=0; i<NUM_BLIPS; i++) {
		struct blip *b = &blips[i];

		if (b->active) {
			//if (b->age < 0.3 && b->ts < ts)
			//	b->ts = ts;
			float bt = time * ts/14;
			b->r *= powf(300.0f, bt);
			//b->r += bt*1.4;
			b->phase -= bt * 2;
			if (b->r > 0.7 || b->phase < 0 || b->bright * b->phase < 0.3)
				b->active = 0;
			b->age += time;
		}
	}
}

int main (int argc, char *argv[])
{

	snd_seq_t *seq_handle;
	int npfd;
	struct pollfd *pfd;

	seq_handle = open_seq();
	npfd = snd_seq_poll_descriptors_count(seq_handle, POLLIN);
	pfd = malloc(npfd * sizeof(*pfd));
	snd_seq_poll_descriptors(seq_handle, pfd, npfd, POLLIN);

	if (argc >= 2) {
		snd_seq_addr_t addr;
		if (snd_seq_parse_address(seq_handle, &addr, argv[1]) == 0) {
			if (snd_seq_connect_from(seq_handle, portid, addr.client, addr.port) == 0) {
				printf("Connected to %s\n", argv[1]);
			}
		}
	}

	int i;
	for (i=2; i<argc; i++)
		channels |= 1<<(atoi(argv[i])-1);

	OLRenderParams params;

	memset(&params, 0, sizeof params);
	params.rate = 48000;
	params.on_speed = 2.0/50.0;
	params.off_speed = 2.0/35.0;
	params.start_wait = 6;
	params.start_dwell = 1;
	params.curve_dwell = 0;
	params.corner_dwell = 0;
	params.curve_angle = cosf(30.0*(M_PI/180.0)); // 30 deg
	params.end_dwell = 0;
	params.end_wait = 7;
	params.flatness = 0.00001;
	params.snap = 1/100000.0;
	params.render_flags = RENDER_GRAYSCALE;

	if(olInit(3, 30000) < 0)
		return 1;

	olSetRenderParams(&params);

	float time = 0;
	float ftime;

	int frames = 0;

	float nps = 0;
	avgnotes = 5;
	
	while(1) {
		olLoadIdentity();

		int drawn = draw();
		if (drawn < 2)
			draw();

		ftime = olRenderFrame(100);

		float t = powf(0.3, ftime);
		avgnotes = avgnotes * t + (nps+2) * (1.0f-t);

		animate(ftime);

		int notes = 0;
		if (poll(pfd, npfd, 0) > 0)
			notes = midi_action(seq_handle);

		int pnotes = (notes+2)/3;

		nps = pnotes / ftime * 1.2;

		frames++;
		time += ftime;
		printf("Frame time: %f, Avg FPS:%f, Cur FPS:%f, %d, nps:%3d, avgnotes:%f\n", ftime, frames/time, 1/ftime, notes, (int)nps, avgnotes);
	}

	olShutdown();
	exit (0);
}

