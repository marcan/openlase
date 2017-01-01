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

#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
# define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
#endif

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
	int got_frame;
	int input_samples;
	int total = 0;
	AVFrame *a_frame;
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

			a_frame = av_frame_alloc();
			a_frame->nb_samples = AVCODEC_MAX_AUDIO_FRAME_SIZE;
			ctx->av.codecctx->get_buffer2(ctx->av.codecctx, a_frame, 0);

			avcodec_decode_audio4(ctx->av.codecctx, a_frame, &got_frame, &packet);
			if (!got_frame) {
				olLog("Error while decoding audio frame\n");
				memset(lb, 0, samples*sizeof(float));
				memset(rb, 0, samples*sizeof(float));
				return -1;
			}

			input_samples = a_frame->nb_samples;

			ctx->buffered_samples = avresample_convert(ctx->resampler,
				(uint8_t **)&ctx->oabuf, 0, AVCODEC_MAX_AUDIO_FRAME_SIZE,
				a_frame->data, a_frame->linesize[0], input_samples);

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

int video_open(VContext **octx, char *file, float start_pos)
{
	int i;
	VContext *ctx;

	if (!inited)
		avstream_init();
	
	ctx = malloc(sizeof(VContext));
	memset(ctx, 0, sizeof(*ctx));

	if (avformat_open_input(&ctx->av.formatctx, file, NULL, NULL) != 0)
		goto error;

	if (avformat_find_stream_info(ctx->av.formatctx, NULL) < 0)
		goto error;

	int stream=-1;
	for (i=0; i<ctx->av.formatctx->nb_streams; i++) {
		if (ctx->av.formatctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
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

	if (avcodec_open2(ctx->av.codecctx, ctx->av.codec, NULL) < 0)
		goto error;

	ctx->frame = av_frame_alloc();

	if (start_pos) {
		av_seek_frame(ctx->av.formatctx, -1, (int64_t)(start_pos * AV_TIME_BASE), 0);
		avcodec_flush_buffers(ctx->av.codecctx);
	}

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
//	av_close_input_file(ctx->av.formatctx);
	free(ctx);

	return 0;
}

int audio_open(AContext **octx, char *file, float start_pos)
{
	int i;
	AContext *ctx;

	if (!inited)
		avstream_init();

	ctx = malloc(sizeof(AContext));
	memset(ctx, 0, sizeof(*ctx));
	ctx->oabuf = malloc(AUDIO_BUF * sizeof(float));

	if (avformat_open_input(&ctx->av.formatctx, file, NULL, NULL) != 0)
		goto error;

	if (avformat_find_stream_info(ctx->av.formatctx, NULL) < 0)
		goto error;

	int stream=-1;
	for (i=0; i<ctx->av.formatctx->nb_streams; i++) {
		if (ctx->av.formatctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO) {
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

	if (avcodec_open2(ctx->av.codecctx, ctx->av.codec, NULL) < 0)
		goto error;

	ctx->resampler = avresample_alloc_context();
	av_opt_set_int(ctx->resampler, "in_channel_layout", ctx->av.codecctx->channel_layout, 0);
	av_opt_set_int(ctx->resampler, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(ctx->resampler, "in_sample_rate", ctx->av.codecctx->sample_rate, 0);
	av_opt_set_int(ctx->resampler, "out_sample_rate", 48000, 0);
	av_opt_set_int(ctx->resampler, "in_sample_fmt", ctx->av.codecctx->sample_fmt, 0);
	av_opt_set_int(ctx->resampler, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
	if (avresample_open(ctx->resampler))
		return -1;

	if (!ctx->resampler)
		goto error;

	ctx->buffered_samples = 0;

	if (start_pos) {
		av_seek_frame(ctx->av.formatctx, -1, (int64_t)(start_pos * AV_TIME_BASE), 0);
		avcodec_flush_buffers(ctx->av.codecctx);
	}

	*octx = ctx;
	return 0;
error:
	if (ctx->oabuf)
		free(ctx->oabuf);
	// todo: cleanup avcodec stuff...
	free(ctx);
	*octx = NULL;
	return -1;
}

int audio_close(AContext *ctx)
{
	avcodec_close(ctx->av.codecctx);
	avresample_close(ctx->resampler);
//	av_close_input_file(ctx->av.formatctx);
	free(ctx->oabuf);
	free(ctx);
	return 0;
}
