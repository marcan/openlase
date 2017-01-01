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

#ifndef AVSTREAM_H
#define AVSTREAM_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavresample/avresample.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

struct avfctx {
	AVFormatContext *formatctx;
	AVCodecContext *codecctx;
	AVCodec *codec;
	int stream;
};

struct _acontext {
	struct avfctx av;
	AVAudioResampleContext *resampler;
	float *oabuf;
	short *iabuf;
	int buffered_samples;
	float *poabuf;
};

struct _vcontext {
	struct avfctx av;
	AVFrame *frame;
};

typedef struct _acontext AContext;
typedef struct _vcontext VContext;

int video_open(VContext **ctx, char *file, float start_pos);
int video_readframe(VContext *ctx, AVFrame **oFrame);
int video_close(VContext *ctx);

int audio_open(AContext **ctx, char *file, float start_pos);
int audio_readsamples(AContext *ctx, float *lb, float *rb, int samples);
int audio_close(AContext *ctx);

#endif