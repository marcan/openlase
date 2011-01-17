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
#include "video.h"
#include "trace.h"

/* This is a slide version of playvid.c, using avstream.c. */

static AContext *actx;
static VContext *vctx;

static video_params *cfg;

static float vidtime;
static void *tmp;
static int dropframes;
static int inframes;
static int outframes;
static float volume = 0;
static int bg_white = -1;
static AVFrame *frame;

static void moreaudio(float *lb, float *rb, int samples)
{
	audio_readsamples(actx, lb, rb, samples);
	while (samples--) {
		*lb++ *= volume;
		*rb++ *= volume;
	}
}

void videotracer_init(void **ctx, void *arg, OLRenderParams *params)
{
	cfg = arg;
	if (audio_open(&actx, cfg->file) != 0) {
		olLog("Audio open/init failed\n");
		exit(1);
	}

	if (video_open(&vctx, cfg->file) != 0) {
		olLog("Video open/init failed\n");
		exit(1);
	}

	int width = vctx->av.codecctx->width;
	int height = vctx->av.codecctx->height;

	if (cfg->aspect == 0)
		cfg->aspect = width / (float)height;
	if (cfg->framerate == 0)
		cfg->framerate = vctx->av.formatctx->streams[vctx->av.stream]->r_frame_rate.num / (float)vctx->av.formatctx->streams[vctx->av.stream]->r_frame_rate.den;

	tmp = malloc(width * height * 2);
	vidtime = 0;
	dropframes = 0;
	inframes = 0;
	outframes = 0;
	bg_white = -1;
	olSetAudioCallback(moreaudio);

	int maxd = width > height ? width : height;

	if (cfg->min_length)
		params->min_length = cfg->min_length;
	if (cfg->off_speed)
		params->off_speed = cfg->off_speed;
	if (cfg->start_wait)
		params->start_wait = cfg->start_wait;
	if (cfg->end_wait)
		params->end_wait = cfg->end_wait;
	if (cfg->snap_pix)
		params->snap = (cfg->snap_pix*2.0)/(float)maxd;
	if (cfg->min_framerate)
		params->max_framelen = params->rate / cfg->min_framerate;
}

void videotracer_deinit(void *ctx)
{
	audio_close(actx);
	video_close(vctx);
	free(tmp);
	olSetAudioCallback(NULL);
}

#define ASPECT 0.75

void videotracer_render(void *ctx, float time, float state)
{
	float pvol = (1-fabsf(state));
	volume = pvol < 0 ? 0 : 0.8 * pvol * cfg->volume;
	olLog("state=%f pvol=%f\n", state, pvol);

	float iaspect = 1/cfg->aspect;
	int width = vctx->av.codecctx->width;
	int height = vctx->av.codecctx->height;

	if (cfg->aspect > ASPECT) {
		olScale(1, iaspect);
	} else {
		olScale(ASPECT * cfg->aspect, ASPECT);
	}

	olScale(1+cfg->overscan, 1+cfg->overscan);
	olTranslate(-1.0f, 1.0f);
	olScale(2.0f/width, -2.0f/height);

	float frametime = 1.0f/cfg->framerate;

	while ((time+frametime) >= vidtime) {
		if (video_readframe(vctx, &frame) != 1) {
			olLog("Video EOF!\n");
			return;
		}
		if (inframes == 0)
			olLog("Frame stride: %d\n", frame->linesize[0]);

		inframes++;
		if (vidtime < time) {
			vidtime += frametime;
			//olLog("Frame skip!\n");
			dropframes++;
			continue;
		}
		vidtime += frametime;
	}

	int thresh;
	int obj;
	int bsum = 0;
	int c;
	for (c=cfg->edge_off; c<(width-cfg->edge_off); c++) {
		bsum += frame->data[0][c+cfg->edge_off*frame->linesize[0]];
		bsum += frame->data[0][c+(height-cfg->edge_off-1)*frame->linesize[0]];
	}
	for (c=cfg->edge_off; c<(height-cfg->edge_off); c++) {
		bsum += frame->data[0][cfg->edge_off+c*frame->linesize[0]];
		bsum += frame->data[0][(c+1)*frame->linesize[0]-1-cfg->edge_off];
	}
	bsum /= 2 * width * height;
	if (bg_white == -1)
		bg_white = bsum > 128;
	if (bg_white && bsum < cfg->sw_dark)
		bg_white = 0;
	if (!bg_white && bsum > cfg->sw_light)
		bg_white = 1;

	if (bg_white)
		thresh = cfg->thresh_light;
	else
		thresh = cfg->thresh_dark;

	obj = trace(frame->data[0], tmp, thresh, width, height, frame->linesize[0], cfg->decimate);

	outframes++;
	olLog("Video stats: Drift %7.4f, In %4d, Out %4d, Drop %4d, Thr %3d, Bg %3d\n",
		   time-vidtime, inframes, outframes, dropframes, thresh, bsum);
}
