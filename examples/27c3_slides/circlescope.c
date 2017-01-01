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
#include "ilda.h"
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avstream.h"

/* This is a libol version of the circlescope, rendering audio from a file.
 * Instead of taking audio from JACK, this uses the avstream.h stuff. Since this
 * is based on libol, the audio and video are no longer sample-synchronous
 * (as the image is just one object in the frame) so we drop/dupe samples as
 * needed */

static AContext *actx;
static int got_samples = 0;
static uint64_t sample_counter;

#define DRAW_SAMPLES 1024

static float lbuf[DRAW_SAMPLES];
static float rbuf[DRAW_SAMPLES];

static float volume = 0.8;

void circlescope_init(void **ctx, void *arg, OLRenderParams *params)
{
	if (audio_open(&actx, arg, 0) != 0) {
		olLog("Audio open/init failed\n");
		exit(1);
	}

	got_samples = 0;
	sample_counter = 0;
}

void circlescope_deinit(void *ctx)
{
	audio_close(actx);
	olSetAudioCallback(NULL);
}

static void moreaudio(float *lb, float *rb, int samples)
{
	int left = samples;
	float *plb = lb, *prb = rb;

	memset(lb, 0, sizeof(float)*samples);
	memset(rb, 0, sizeof(float)*samples);

	//audio_readleft(actx, lb, rb, left);
	if (got_samples == 0) {
		olLog("No audio buffer?\n");
		return;
	}


	int copy = samples > got_samples ? got_samples : samples;

	memcpy(lb, lbuf, copy * sizeof(float));
	memcpy(rb, rbuf, copy * sizeof(float));
	got_samples -= copy;
	left -= copy;
	plb += copy;
	prb += copy;

	if (got_samples) {
		olLog("Had %d audio samples leftover...\n", got_samples);
		memmove(lbuf, &lbuf[copy], got_samples * sizeof(float));
		memmove(rbuf, &rbuf[copy], got_samples * sizeof(float));
	}

	if (left) {
		olLog("Needed %d extra audio samples\n", left);
		if (audio_readsamples(actx, plb, prb, left) < 1) {
			olLog("No more audio!\n");
		}
	}

	// unset the callback, it will be set by the render function again
	olSetAudioCallback(NULL);

	sample_counter += samples;

	// adjust volume
	int i;
	for (i=0; i<samples; i++) {
		lb[i] *= volume;
		rb[i] *= volume;
	}
}

#define MAX(a,b) (((a)<(b))?(b):(a))
#define MIN(a,b) (((a)>(b))?(b):(a))

float max_size = 0.75f;
float min_size = 0.2f;
float boost = 1.7;

void circlescope_render(void *ctx, float time, float state)
{
	float pvol = (1-fabsf(state)*1.3);
	volume = pvol < 0 ? 0 : 0.8 * pvol;
	
	olLoadIdentity();

	if (got_samples == DRAW_SAMPLES) {
		olLog("Already got a full buffer?\n");
	} else {
		if (audio_readsamples(actx, &lbuf[got_samples], &rbuf[got_samples], DRAW_SAMPLES - got_samples) < 1) {
			olLog("No more audio!\n");
		}
		got_samples = DRAW_SAMPLES;
	}

	int i;
	
	olBegin(OL_POINTS);
	for (i=0; i<DRAW_SAMPLES; i++) {
		double w = 523.251131f / 4.0f * (2*M_PI) / 48000;
		double pos = (sample_counter+i) * w;

		float val = (lbuf[i] + rbuf[i]) / 2 * boost;
		val = MAX(MIN(val,1.0f),-1.0f);
		val = val * 0.5f + 0.5f;
		val *= (max_size - min_size);
		val += min_size;

		olVertex(cosf(pos) * val, sinf(pos) * val, C_WHITE);
	}
	olEnd();
		

	olSetAudioCallback(moreaudio);
}
