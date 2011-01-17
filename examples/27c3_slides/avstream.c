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

/* Simple wrapper around libavcodec to provide audio/video streams */

static int inited = 0;

#include "libol.h"
#include "avstream.h"

#define AUDIO_BUF AVCODEC_MAX_AUDIO_FRAME_SIZE

static void dolog(void *foo, int level, const char *fmt, va_list ap)
{
	char buf[1024];
	vsnprintf(buf, 1024, fmt, ap);
	buf[1023] = 0;
	olLog("%s", buf);
}

static void avstream_init(void)
{
	av_log_set_callback(dolog);
	av_register_all();
	inited = 1;
}

int video_readframe(VContext *ctx, AVFrame **oFrame)
{
	static AVPacket packet;
	int bytesDecoded;
	int frameFinished = 0;

	while (!frameFinished) {
		do {
			if(av_read_frame(ctx->av.formatctx, &packet)<0) {
				*oFrame = ctx->frame;
				return 0;
			}
		} while(packet.stream_index!=ctx->av.stream);

		bytesDecoded=avcodec_decode_video2(ctx->av.codecctx, ctx->frame, &frameFinished, &packet);
		if (bytesDecoded < 0)
		{
			olLog("Error while decoding video frame\n");
			*oFrame = ctx->frame;
			return 0;
		}
		if (bytesDecoded != packet.size) {
			olLog("Multiframe packets not supported (%d != %d)\n", bytesDecoded, packet.size);
			exit(1);
		}
	}
	*oFrame = ctx->frame;
	return 1;
}

int audio_readsamples(AContext *ctx, float *lb, float *rb, int samples)
{
	AVPacket packet;
	int bytes, bytesDecoded;
	int input_samples;
	int total = 0;
	while (samples)
	{
		if (!ctx->buffered_samples) {
			do {
				if(av_read_frame(ctx->av.formatctx, &packet)<0) {
					memset(lb, 0, samples*sizeof(float));
					memset(rb, 0, samples*sizeof(float));
					return total;
				}
			} while(packet.stream_index!=ctx->av.stream);

			bytes = AUDIO_BUF * sizeof(short);

			bytesDecoded = avcodec_decode_audio3(ctx->av.codecctx, ctx->iabuf, &bytes, &packet);
			if(bytesDecoded < 0)
			{
				olLog("Error while decoding audio frame\n");
				memset(lb, 0, samples*sizeof(float));
				memset(rb, 0, samples*sizeof(float));
				return -1;
			}

			input_samples = bytes / (sizeof(short)*ctx->av.codecctx->channels);

			ctx->buffered_samples = audio_resample(ctx->resampler, (void*)ctx->oabuf, ctx->iabuf, input_samples);
			ctx->poabuf = ctx->oabuf;
		}

		*lb++ = *ctx->poabuf++;
		*rb++ = *ctx->poabuf++;
		ctx->buffered_samples--;
		samples--;
		total++;
	}
	return total;
}

int video_open(VContext **octx, char *file)
{
	int i;
	VContext *ctx;

	if (!inited)
		avstream_init();
	
	ctx = malloc(sizeof(VContext));

	if (av_open_input_file(&ctx->av.formatctx, file, NULL, 0, NULL) != 0)
		goto error;

	if (av_find_stream_info(ctx->av.formatctx) < 0)
		goto error;

	dump_format(ctx->av.formatctx, 0, file, 0);

	int stream=-1;
	for (i=0; i<ctx->av.formatctx->nb_streams; i++) {
		if (ctx->av.formatctx->streams[i]->codec->codec_type==CODEC_TYPE_VIDEO) {
			stream=i;
			break;
		}
	}
	if (stream==-1)
		goto error;

	ctx->av.stream = stream;
	ctx->av.codecctx = ctx->av.formatctx->streams[stream]->codec;
	ctx->av.codec = avcodec_find_decoder(ctx->av.codecctx->codec_id);

	if (ctx->av.codec == NULL)
		goto error;

	if (avcodec_open(ctx->av.codecctx, ctx->av.codec) < 0)
		goto error;

	ctx->frame = avcodec_alloc_frame();

	*octx = ctx;
	return 0;

error:
	// todo: cleanup avcodec stuff...
	free(ctx);
	*octx = NULL;
	return -1;
}

int video_close(VContext *ctx)
{
	av_free(ctx->frame);
	avcodec_close(ctx->av.codecctx);
	av_close_input_file(ctx->av.formatctx);
	free(ctx);

	return 0;
}

int audio_open(AContext **octx, char *file)
{
	int i;
	AContext *ctx;

	if (!inited)
		avstream_init();

	ctx = malloc(sizeof(AContext));
	memset(ctx, 0, sizeof(*ctx));
	ctx->oabuf = malloc(AUDIO_BUF * sizeof(float));
	ctx->iabuf = malloc(AUDIO_BUF * sizeof(short));

	if (av_open_input_file(&ctx->av.formatctx, file, NULL, 0, NULL) != 0)
		goto error;

	if (av_find_stream_info(ctx->av.formatctx) < 0)
		goto error;

	dump_format(ctx->av.formatctx, 0, file, 0);

	int stream=-1;
	for (i=0; i<ctx->av.formatctx->nb_streams; i++) {
		if (ctx->av.formatctx->streams[i]->codec->codec_type==CODEC_TYPE_AUDIO) {
			stream=i;
			break;
		}
	}
	if (stream==-1)
		goto error;

	ctx->av.stream = stream;
	ctx->av.codecctx = ctx->av.formatctx->streams[stream]->codec;
	ctx->av.codec = avcodec_find_decoder(ctx->av.codecctx->codec_id);

	if (ctx->av.codec == NULL)
		goto error;

	if (avcodec_open(ctx->av.codecctx, ctx->av.codec) < 0)
		goto error;

	ctx->resampler = av_audio_resample_init(2, ctx->av.codecctx->channels,
										   48000, ctx->av.codecctx->sample_rate,
										   SAMPLE_FMT_FLT, ctx->av.codecctx->sample_fmt,
										   16, 10, 0, 0.8);

	if (!ctx->resampler)
		goto error;

	ctx->buffered_samples = 0;

	*octx = ctx;
	return 0;
error:
	if (ctx->oabuf)
		free(ctx->oabuf);
	if (ctx->iabuf)
		free(ctx->iabuf);
	// todo: cleanup avcodec stuff...
	free(ctx);
	*octx = NULL;
	return -1;
}

int audio_close(AContext *ctx)
{
	avcodec_close(ctx->av.codecctx);
	audio_resample_close(ctx->resampler);
	av_close_input_file(ctx->av.formatctx);
	free(ctx->oabuf);
	free(ctx->iabuf);
	free(ctx);
	return 0;
}
