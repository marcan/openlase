/*
        OpenLase - a realtime laser graphics toolkit

Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>
Copyright (C) 2013 Sergiusz "q3k" Bazański <q3k@q3k.org>

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
#include "trace.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <jack/jack.h>
#include <math.h>
#include <pthread.h>

#include "qplayvid.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavresample/avresample.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#define OL_FRAMES_BUF 5
#define VIDEO_BUF 32

#define SAMPLE_RATE 48000
#define AUDIO_BUF 3

#define dprintf printf

#ifndef AVCODEC_MAX_AUDIO_FRAME_SIZE
# define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
#endif

typedef struct {
	uint8_t *data;
	size_t stride;
	size_t data_size;
	int32_t seekid;
	double pts;
} VideoFrame;

typedef struct {
	int16_t l, r;
	int32_t seekid;
	double pts;
} AudioSample;

typedef enum {
	STOP,
	PAUSE,
	PLAY,
} DisplayMode;

struct PlayerCtx {
	int exit;
	DisplayMode display_mode;
	pthread_t decoder_thread;
	pthread_t display_thread;
	pthread_mutex_t display_mode_mutex;
	PlayerEventCb ev_cb;

	PlayerSettings settings;
	int settings_changed;
	pthread_mutex_t settings_mutex;
	int skip_frame;

	AVFormatContext *fmt_ctx;
	int audio_idx;
	int video_idx;
	pthread_mutex_t seek_mutex;
	pthread_cond_t seek_cond;
	int32_t cur_seekid;
	double seek_pos;
	double duration;

	AVStream *a_stream;
	AVCodecContext *a_codec_ctx;
	AVCodec *a_codec;
	AVAudioResampleContext *a_resampler;
	double a_ratio;

	AVStream *v_stream;
	AVCodecContext *v_codec_ctx;
	AVCodec *v_codec;
	int width, height;
	int64_t v_pkt_pts;
	int64_t v_faulty_dts, v_faulty_pts, v_last_pts, v_last_dts;

	AVFrame *v_frame;
	VideoFrame *v_bufs[VIDEO_BUF];
	int v_buf_len;
	int v_buf_get;
	int v_buf_put;
	VideoFrame *cur_frame;
	double last_frame_pts;
	struct SwsContext *v_sws_ctx;

	AVFrame *a_frame;
	short *a_resample_output[2];
	AudioSample *a_buf;
	int a_buf_len;
	int a_buf_get;
	int a_buf_put;
	double a_cur_pts;

	pthread_mutex_t a_buf_mutex;
	pthread_cond_t a_buf_not_full;
	pthread_cond_t a_buf_not_empty;
	pthread_mutex_t v_buf_mutex;
	pthread_cond_t v_buf_not_full;
	pthread_cond_t v_buf_not_empty;
};

size_t decode_audio(PlayerCtx *ctx, AVPacket *packet, int new_packet, int32_t seekid)
{
	int decoded, got_frame;
	
	ctx->a_frame = avcodec_alloc_frame();
	ctx->a_frame->nb_samples = AVCODEC_MAX_AUDIO_FRAME_SIZE;
	ctx->a_codec_ctx->get_buffer2(ctx->a_codec_ctx, ctx->a_frame, 0);
	decoded = avcodec_decode_audio4(ctx->a_codec_ctx, ctx->a_frame, &got_frame, packet);
	if (!got_frame) {
		fprintf(stderr, "Error while decoding audio frame\n");
		decoded = packet->size;
		goto fail;
	}
	int in_samples = ctx->a_frame->nb_samples;
	if (!in_samples)
		goto fail;

	int out_samples = avresample_convert(ctx->a_resampler,
		(uint8_t **)ctx->a_resample_output, 0, AVCODEC_MAX_AUDIO_FRAME_SIZE,
		ctx->a_frame->data, ctx->a_frame->linesize[0], in_samples);
	pthread_mutex_lock(&ctx->a_buf_mutex);

	int free_samples;
	while (1) {
		free_samples = ctx->a_buf_get - ctx->a_buf_put;
		if (free_samples <= 0)
			free_samples += ctx->a_buf_len;

		if (free_samples <= out_samples) {
			printf("Wait for space in audio buffer (get: %i put: %i)\n", ctx->a_buf_get, ctx->a_buf_put);
			pthread_cond_wait(&ctx->a_buf_not_full, &ctx->a_buf_mutex);
		} else {
			break;
		}
	}
	pthread_mutex_unlock(&ctx->a_buf_mutex);

	if (new_packet && packet->pts != AV_NOPTS_VALUE)
		ctx->a_cur_pts = av_q2d(ctx->a_stream->time_base) * packet->pts;

	int put = ctx->a_buf_put;
	short *rbuf = ctx->a_resample_output[0];
	int i;
	for (i = 0; i < out_samples; i++) {
		ctx->a_buf[put].l = *rbuf++;
		ctx->a_buf[put].r = *rbuf++;
		ctx->a_buf[put].seekid = seekid;
		ctx->a_buf[put].pts = ctx->a_cur_pts;
		put++;
		ctx->a_cur_pts += 1.0/SAMPLE_RATE;
		if (put == ctx->a_buf_len)
			put = 0;
	}

	printf("Put %d audio samples at pts %f\n", out_samples, ctx->a_cur_pts);

	pthread_mutex_lock(&ctx->a_buf_mutex);
	ctx->a_buf_put = put;
	pthread_cond_signal(&ctx->a_buf_not_empty);
	pthread_mutex_unlock(&ctx->a_buf_mutex);

fail:
	avcodec_free_frame(&ctx->a_frame);
	ctx->a_frame = NULL;
	return decoded;
}

size_t decode_video(PlayerCtx *ctx, AVPacket *packet, int new_packet, int32_t seekid)
{
	int decoded;
	int got_frame;

	if (!new_packet)
		fprintf(stderr, "warn: multi-frame video packets, pts might be inaccurate\n");

	ctx->v_pkt_pts = packet->pts;

	ctx->v_frame = avcodec_alloc_frame();
	decoded = avcodec_decode_video2(ctx->v_codec_ctx, ctx->v_frame, &got_frame, packet);
	if (decoded < 0) {
		fprintf(stderr, "Error while decoding video frame\n");
		decoded = packet->size;
		goto fail;
	}
	if (!got_frame)
		goto fail;


	// The pts magic guesswork
	int64_t pts = AV_NOPTS_VALUE;
	int64_t frame_pts = AV_NOPTS_VALUE;
	frame_pts = av_frame_get_best_effort_timestamp(ctx->v_frame);

	if (packet->dts != AV_NOPTS_VALUE) {
		ctx->v_faulty_dts += packet->dts <= ctx->v_last_dts;
		ctx->v_last_dts = packet->dts;
	}
	if (frame_pts != AV_NOPTS_VALUE) {
		ctx->v_faulty_pts += frame_pts <= ctx->v_last_pts;
		ctx->v_last_pts = frame_pts;
	}
	if ((ctx->v_faulty_pts <= ctx->v_faulty_dts || packet->dts == AV_NOPTS_VALUE)
		&& frame_pts != AV_NOPTS_VALUE)
		pts = frame_pts;
	else
		pts = packet->dts;

	if (pts == AV_NOPTS_VALUE) {
		if (ctx->v_last_pts != AV_NOPTS_VALUE) {
			pts = ctx->v_last_pts++;
		} else if (ctx->v_last_dts != AV_NOPTS_VALUE) {
			pts = ctx->v_last_dts++;
		}
	}

	if (pts == AV_NOPTS_VALUE) {
		if (ctx->v_last_pts != AV_NOPTS_VALUE) {
			pts = ctx->v_last_pts++;
		} else if (ctx->v_last_dts != AV_NOPTS_VALUE) {
			pts = ctx->v_last_dts++;
		} else {
			pts = 0;
		}
	}

	pthread_mutex_lock(&ctx->v_buf_mutex);
	while (((ctx->v_buf_put + 1) % ctx->v_buf_len) == ctx->v_buf_get) {
		printf("Wait for space in video buffer\n");
		pthread_cond_wait(&ctx->v_buf_not_full, &ctx->v_buf_mutex);
	}
	pthread_mutex_unlock(&ctx->v_buf_mutex);

	VideoFrame *frame = ctx->v_bufs[ctx->v_buf_put];
	if (!frame) {
		frame = malloc(sizeof(VideoFrame));
		frame->stride = ctx->v_frame->linesize[0];
		frame->data_size = frame->stride * ctx->height;
		frame->data = malloc(frame->data_size);
		ctx->v_bufs[ctx->v_buf_put] = frame;
	}

	if (frame->stride != ctx->v_frame->linesize[0]) {
		fprintf(stderr, "stride mismatch: %d != %d\n", (int)frame->stride, ctx->v_frame->linesize[0]);
		goto fail;
	}
	fprintf(stderr, "pix fmt: %d\n", ctx->v_codec_ctx->pix_fmt);

	if (!ctx->v_sws_ctx) {
		ctx->v_sws_ctx = sws_getContext(ctx->width, ctx->height, ctx->v_codec_ctx->pix_fmt,
										ctx->width, ctx->height, PIX_FMT_GRAY8, SWS_BICUBIC,
										NULL, NULL, NULL);
	}

	AVPicture pict;
	pict.data[0] = frame->data;
	pict.linesize[0] = frame->stride;

	sws_scale(ctx->v_sws_ctx, (const uint8_t* const*)ctx->v_frame->data, ctx->v_frame->linesize, 0, ctx->height, pict.data, pict.linesize);

	frame->pts = av_q2d(ctx->v_stream->time_base) * pts;
	frame->seekid = seekid;

	printf("Put frame %d (pts:%f seekid:%d)\n", ctx->v_buf_put, frame->pts, seekid);
	pthread_mutex_lock(&ctx->v_buf_mutex);
	if (++ctx->v_buf_put == ctx->v_buf_len)
		ctx->v_buf_put = 0;
	pthread_cond_signal(&ctx->v_buf_not_empty);
	pthread_mutex_unlock(&ctx->v_buf_mutex);

fail:
	avcodec_free_frame(&ctx->v_frame);
	ctx->v_frame = NULL;
	return decoded;
}

void push_eof(PlayerCtx *ctx, int32_t seekid)
{
	if (ctx->audio_idx != -1) {
		pthread_mutex_lock(&ctx->a_buf_mutex);
		while (((ctx->a_buf_put + 1) % ctx->a_buf_len) == ctx->a_buf_get) {
			printf("Wait for space in audio buffer\n");
			pthread_cond_wait(&ctx->a_buf_not_full, &ctx->a_buf_mutex);
		}
		ctx->a_buf[ctx->a_buf_put].l = 0;
		ctx->a_buf[ctx->a_buf_put].r = 0;
		ctx->a_buf[ctx->a_buf_put].pts = 0;
		ctx->a_buf[ctx->a_buf_put].seekid = -seekid;
		if (++ctx->a_buf_put == ctx->a_buf_len)
			ctx->a_buf_put = 0;
		pthread_cond_signal(&ctx->a_buf_not_empty);
		pthread_mutex_unlock(&ctx->a_buf_mutex);
	}

	pthread_mutex_lock(&ctx->v_buf_mutex);
	while (((ctx->v_buf_put + 1) % ctx->v_buf_len) == ctx->v_buf_get) {
		printf("Wait for space in video buffer\n");
		pthread_cond_wait(&ctx->v_buf_not_full, &ctx->v_buf_mutex);
	}
	ctx->v_bufs	[ctx->v_buf_put]->pts = 0;
	ctx->v_bufs[ctx->v_buf_put]->seekid = -seekid;
	if (++ctx->v_buf_put == ctx->v_buf_len)
		ctx->v_buf_put = 0;
	pthread_mutex_unlock(&ctx->v_buf_mutex);
}

void *decoder_thread(void *arg)
{
	PlayerCtx *ctx = arg;
	AVPacket packet;
	AVPacket cpacket;
	size_t decoded_bytes;
	int seekid = ctx->cur_seekid;

	printf("Decoder thread started\n");

	memset(&packet, 0, sizeof(packet));
	memset(&cpacket, 0, sizeof(cpacket));

	while (!ctx->exit) {
		int new_packet = 0;
		if (cpacket.size == 0) {
			if (packet.data)
				av_free_packet(&packet);
			pthread_mutex_lock(&ctx->seek_mutex);
			if (ctx->cur_seekid > seekid) {
				printf("Seek! %f\n", ctx->seek_pos);
				av_seek_frame(ctx->fmt_ctx, -1, (int64_t)(ctx->seek_pos * AV_TIME_BASE), 0);
				seekid = ctx->cur_seekid;
				// HACK! Avoid deadlock by waking up the video waiter
				pthread_mutex_lock(&ctx->v_buf_mutex);
				pthread_cond_signal(&ctx->v_buf_not_empty);
				pthread_mutex_unlock(&ctx->v_buf_mutex);
				if (ctx->audio_idx != -1)
					avcodec_flush_buffers(ctx->a_codec_ctx);
				avcodec_flush_buffers(ctx->v_codec_ctx);
			}
			if (av_read_frame(ctx->fmt_ctx, &packet) < 0) {
				fprintf(stderr, "EOF!\n");
				push_eof(ctx, seekid);
				pthread_cond_wait(&ctx->seek_cond, &ctx->seek_mutex);
				pthread_mutex_unlock(&ctx->seek_mutex);
				continue;
			}
			pthread_mutex_unlock(&ctx->seek_mutex);
			cpacket = packet;
			new_packet = 1;
		}
		if (ctx->audio_idx != -1 && cpacket.stream_index == ctx->audio_idx) {
			decoded_bytes = decode_audio(ctx, &cpacket, new_packet, seekid);
		} else if (cpacket.stream_index == ctx->video_idx) {
			decoded_bytes = decode_video(ctx, &cpacket, new_packet, seekid);
		} else {
			decoded_bytes = cpacket.size;
		}

		cpacket.data += decoded_bytes;
		cpacket.size -= decoded_bytes;
	}
	return NULL;
}

int decoder_init(PlayerCtx *ctx, const char *file)
{
	int i;

	memset(ctx, 0, sizeof(*ctx));

	ctx->video_idx = -1;
	ctx->audio_idx = -1;
	ctx->cur_seekid = 1;
	ctx->a_cur_pts = 0;

	AVInputFormat *format = NULL;
	if (!strncmp(file, "x11grab://", 10)) {
		printf("Using X11Grab\n");
		format = av_find_input_format("x11grab");
		file += 10;
	}

	if (avformat_open_input(&ctx->fmt_ctx, file, format, NULL) != 0) {
		printf("Couldn't open input file %s\n", file);
		return -1;
	}

	if (avformat_find_stream_info(ctx->fmt_ctx, NULL) < 0) {
		printf("Couldn't get stream info\n");
		return -1;
	}

	ctx->duration = ctx->fmt_ctx->duration/(double)AV_TIME_BASE;

	pthread_mutex_init(&ctx->seek_mutex, NULL);
	pthread_cond_init(&ctx->seek_cond, NULL);

	for (i = 0; i < ctx->fmt_ctx->nb_streams; i++) {
		switch (ctx->fmt_ctx->streams[i]->codec->codec_type) {
			case AVMEDIA_TYPE_VIDEO:
				if (ctx->video_idx == -1)
					ctx->video_idx = i;
				break;
			case AVMEDIA_TYPE_AUDIO:
				if (ctx->audio_idx == -1)
					ctx->audio_idx = i;
				break;
			default:
				break;
		}
	}

	if (ctx->video_idx == -1) {
		printf("No video streams\n");
		return -1;
	}

	if (ctx->audio_idx != -1) {
		ctx->a_stream = ctx->fmt_ctx->streams[ctx->audio_idx];
		ctx->a_codec_ctx = ctx->a_stream->codec;
		ctx->a_codec = avcodec_find_decoder(ctx->a_codec_ctx->codec_id);
		if (ctx->a_codec == NULL) {
			return -1;
			printf("No audio codec\n");
		}
		if (avcodec_open2(ctx->a_codec_ctx, ctx->a_codec, NULL) < 0) {
			printf("Failed to open audio codec\n");
			return -1;
		}

		printf("Audio srate: %d\n", ctx->a_codec_ctx->sample_rate);

		ctx->a_resampler = avresample_alloc_context();
		av_opt_set_int(ctx->a_resampler, "in_channel_layout", ctx->a_codec_ctx->channel_layout, 0);
		av_opt_set_int(ctx->a_resampler, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
		av_opt_set_int(ctx->a_resampler, "in_sample_rate", ctx->a_codec_ctx->sample_rate, 0);
		av_opt_set_int(ctx->a_resampler, "out_sample_rate", SAMPLE_RATE, 0);
		av_opt_set_int(ctx->a_resampler, "in_sample_fmt", ctx->a_codec_ctx->sample_fmt, 0);
		av_opt_set_int(ctx->a_resampler, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
		if (avresample_open(ctx->a_resampler))
			return -1;

		ctx->a_ratio = SAMPLE_RATE/(double)ctx->a_codec_ctx->sample_rate;

		ctx->a_resample_output[0] = malloc(2 * sizeof(short) * AVCODEC_MAX_AUDIO_FRAME_SIZE * (ctx->a_ratio * 1.1));
		ctx->a_resample_output[1] = 0;
		ctx->a_buf_len = AUDIO_BUF*SAMPLE_RATE;
		ctx->a_buf = malloc(sizeof(*ctx->a_buf) * ctx->a_buf_len);
		ctx->a_buf_put = 0;
		ctx->a_buf_get = 0;

		pthread_mutex_init(&ctx->a_buf_mutex, NULL);
		pthread_cond_init(&ctx->a_buf_not_full, NULL);
		pthread_cond_init(&ctx->a_buf_not_empty, NULL);
	}

	ctx->v_stream = ctx->fmt_ctx->streams[ctx->video_idx];
	ctx->v_codec_ctx = ctx->v_stream->codec;
	ctx->width = ctx->v_codec_ctx->width;
	ctx->height = ctx->v_codec_ctx->height;

	ctx->v_codec = avcodec_find_decoder(ctx->v_codec_ctx->codec_id);
	if (ctx->v_codec == NULL) {
		printf("No video codec\n");
		return -1;
	}

	if (avcodec_open2(ctx->v_codec_ctx, ctx->v_codec, NULL) < 0) {
		printf("Failed to open video codec\n");
		return -1;
	}

	ctx->v_pkt_pts = AV_NOPTS_VALUE;
    ctx->v_faulty_pts = ctx->v_faulty_dts = 0;
    ctx->v_last_pts = ctx->v_last_dts = INT64_MIN;

	ctx->v_buf_len = VIDEO_BUF;

	pthread_mutex_init(&ctx->v_buf_mutex, NULL);
	pthread_cond_init(&ctx->v_buf_not_full, NULL);
	pthread_cond_init(&ctx->v_buf_not_empty, NULL);

	if (pthread_create(&ctx->decoder_thread, NULL, decoder_thread, ctx) != 0)
		return -1;

	return 0;
}

PlayerCtx *g_ctx;

void drop_audio(PlayerCtx *ctx, int by_pts)
{
	if (!ctx->cur_frame)
		return;
	if (ctx->audio_idx == -1)
		return;
	while (1) {
		pthread_mutex_lock(&ctx->a_buf_mutex);
		int get = ctx->a_buf_get;
		int have_samples = ctx->a_buf_put - get;
		if (!have_samples) {
			pthread_mutex_unlock(&ctx->a_buf_mutex);
			break;
		}
		if (have_samples < 0)
			have_samples += ctx->a_buf_len;
		pthread_mutex_unlock(&ctx->a_buf_mutex);

		int i;
		for (i = 0; i < have_samples; i++) {
			if (ctx->a_buf[get].seekid == -1 ||
				(ctx->a_buf[get].seekid == ctx->cur_seekid &&
				(!by_pts || ctx->a_buf[get].pts >= ctx->cur_frame->pts)))
				break;
			if (++get == ctx->a_buf_len)
				get = 0;
		}
		printf("Dropped %d samples\n", i);

		pthread_mutex_lock(&ctx->a_buf_mutex);
		ctx->a_buf_get = get;
		pthread_cond_signal(&ctx->a_buf_not_full);
		pthread_mutex_unlock(&ctx->a_buf_mutex);
		if (i == 0)
			break;
	}
}

void drop_all_video(PlayerCtx *ctx)
{
	if (ctx->cur_frame && ctx->cur_frame->seekid == -ctx->cur_seekid) {
		printf("No more video (EOF)\n");
		return;
	}
	pthread_mutex_lock(&ctx->v_buf_mutex);
	int last = (ctx->v_buf_put + ctx->v_buf_len - 1) % ctx->v_buf_len;
	while (ctx->v_buf_get != ctx->v_buf_put) {
		if (ctx->v_buf_get == last)
			break;
		ctx->v_buf_get++;
		if (ctx->v_buf_get == ctx->v_buf_len)
			ctx->v_buf_get = 0;
	}
	pthread_cond_signal(&ctx->v_buf_not_full);
	pthread_mutex_unlock(&ctx->v_buf_mutex);
}

int next_video_frame(PlayerCtx *ctx)
{
	if (ctx->cur_frame && ctx->cur_frame->seekid == -ctx->cur_seekid) {
		printf("No more video (EOF)\n");
		return 0;
	}
	if (ctx->cur_frame)
		ctx->last_frame_pts = ctx->cur_frame->pts;
	pthread_mutex_lock(&ctx->v_buf_mutex);
	while (ctx->v_buf_get == ctx->v_buf_put) {
		printf("Wait for video (pts %f)\n", ctx->cur_frame?ctx->cur_frame->pts:-1);
		pthread_cond_wait(&ctx->v_buf_not_empty, &ctx->v_buf_mutex);
		// HACK! This makes sure to flush stale stuff from the audio queue to
		// avoid deadlocks while seeking
		pthread_mutex_unlock(&ctx->v_buf_mutex);
		drop_audio(ctx, 0);
		pthread_mutex_lock(&ctx->v_buf_mutex);
	}
	if (ctx->cur_frame && ctx->v_bufs[ctx->v_buf_get]->seekid > ctx->cur_frame->seekid)
		ctx->last_frame_pts = -1;
	ctx->cur_frame = ctx->v_bufs[ctx->v_buf_get];
	printf("Get frame %d (pts: %f)\n", ctx->v_buf_get, ctx->cur_frame->pts);
	ctx->v_buf_get++;
	if (ctx->v_buf_get == ctx->v_buf_len)
		ctx->v_buf_get = 0;
	pthread_cond_signal(&ctx->v_buf_not_full);
	pthread_mutex_unlock(&ctx->v_buf_mutex);
	return 1;
}

void get_audio(float *lb, float *rb, int samples)
{
	PlayerCtx *ctx = g_ctx;

	if (ctx->audio_idx == -1) {
		memset(lb, 0, samples * sizeof(*lb));
		memset(rb, 0, samples * sizeof(*rb));
		return;
	}

	pthread_mutex_lock(&ctx->display_mode_mutex);
	DisplayMode display_mode = ctx->display_mode;
	pthread_mutex_unlock(&ctx->display_mode_mutex);

	if (display_mode != PLAY)
	{
		if (display_mode == PAUSE) {
			printf("get_audio: paused\n");
			if (!ctx->cur_frame) {
				next_video_frame(ctx);
			}
			while (ctx->cur_frame->seekid != ctx->cur_seekid &&
				   ctx->cur_frame->seekid != -ctx->cur_seekid) {
				printf("Drop audio due to seek\n");
				drop_audio(ctx, 1);
				next_video_frame(ctx);
				drop_audio(ctx, 1);
			}
			if (ctx->skip_frame) {
				ctx->skip_frame = 0;
				drop_audio(ctx, 1);
				next_video_frame(ctx);
				drop_audio(ctx, 1);
			}
			printf("get_audio: pause complete\n");
		}
		memset(lb, 0, samples * sizeof(*lb));
		memset(rb, 0, samples * sizeof(*rb));
		return;
	}
	pthread_mutex_lock(&ctx->settings_mutex);
	double volume = ctx->settings.volume / 100.0;
	pthread_mutex_unlock(&ctx->settings_mutex);

	while (samples) {
		double pts = -1;

		pthread_mutex_lock(&ctx->a_buf_mutex);
		int have_samples = ctx->a_buf_put - ctx->a_buf_get;
		if (!have_samples) {
			printf("Wait for audio\n");
			pthread_cond_wait(&ctx->a_buf_not_empty, &ctx->a_buf_mutex);
			pthread_mutex_unlock(&ctx->a_buf_mutex);
			continue;
		}
		if (have_samples < 0)
			have_samples += ctx->a_buf_len;
		pthread_mutex_unlock(&ctx->a_buf_mutex);

		int get = ctx->a_buf_get;
		int played = 0;
		while (samples && have_samples--) {
			if (ctx->a_buf[get].seekid == -ctx->cur_seekid) {
				memset(lb, 0, sizeof(*lb) * samples);
				memset(rb, 0, sizeof(*rb) * samples);
				samples = 0;
				break;
			}
			if (ctx->a_buf[get].seekid == ctx->cur_seekid) {
				pts = ctx->a_buf[get].pts;
				*lb++ = ctx->a_buf[get].l / 32768.0 * volume;
				*rb++ = ctx->a_buf[get].r / 32768.0 * volume;
				samples--;
				played++;
			}
			if (++get >= ctx->a_buf_len)
				get = 0;
		}

		pthread_mutex_lock(&ctx->a_buf_mutex);
		ctx->a_buf_get = get;
		pthread_cond_signal(&ctx->a_buf_not_full);
		pthread_mutex_unlock(&ctx->a_buf_mutex);

		printf("Played %d samples, next pts %f\n", played, pts);

		while (1) {
			if (!ctx->cur_frame) {
				next_video_frame(ctx);
				continue;
			}
			if (ctx->cur_frame->seekid == -ctx->cur_seekid)
				break;
			double next_pts = ctx->cur_frame->pts;
			if (ctx->last_frame_pts != -1)
				next_pts = 2 * ctx->cur_frame->pts - ctx->last_frame_pts;
			if (pts > next_pts || ctx->cur_frame->seekid != ctx->cur_seekid) {
				if (next_video_frame(ctx))
					continue;
			}
			break;
		}
	}
}

void deliver_event(PlayerCtx *ctx, float time, float ftime, int frames, int ended)
{
	if (!ctx->ev_cb)
		return;
	PlayerEvent ev;
	OLFrameInfo info;
	olGetFrameInfo(&info);
	ev.ended = ended;
	ev.frames = frames;
	ev.time = time;
	ev.ftime = ftime;
	ev.points = info.points;
	ev.padding_points = info.padding_points;
	ev.resampled_points = info.resampled_points;
	ev.resampled_blacks = info.resampled_blacks;
	ev.objects = info.objects;
	if (ctx->cur_frame)
		ev.pts = ctx->cur_frame->pts;
	else
		ev.pts = -1;
	ctx->ev_cb(&ev);
}

void *display_thread(void *arg)
{
	PlayerCtx *ctx = arg;
	int i;

	OLRenderParams params;
	memset(&params, 0, sizeof params);
	params.rate = 48000;
	params.on_speed = 2.0/100.0;
	params.off_speed = 2.0/15.0;
	params.start_wait = 8;
	params.end_wait = 3;
	params.snap = 1/120.0;
	params.render_flags = RENDER_GRAYSCALE;
	params.min_length = 20;
	params.start_dwell = 2;
	params.end_dwell = 2;
	params.max_framelen = 48000/20.0;

	if(olInit(OL_FRAMES_BUF, 300000) < 0) {
		printf("OpenLase init failed\n");
		return NULL;
	}

	float aspect = ctx->width / (float)ctx->height;
	float sample_aspect = av_q2d(ctx->v_stream->sample_aspect_ratio);
	if (sample_aspect != 0)
		aspect *= sample_aspect;
	printf("Aspect: %f\n", aspect);

	float iaspect = 1/aspect;

	int maxd = ctx->width > ctx->height ? ctx->width : ctx->height;
	int mind = ctx->width < ctx->height ? ctx->width : ctx->height;

	g_ctx = ctx;
	olSetAudioCallback(get_audio);
	olSetRenderParams(&params);

	OLTraceCtx *trace_ctx;
	OLTraceParams tparams;
	OLTraceResult result;
	memset(&result, 0, sizeof(result));
	ctx->settings_changed = 1;

	tparams.sigma = ctx->settings.blur / 100.0;
	if (ctx->settings.canny)
		tparams.mode = OL_TRACE_CANNY;
	else
		tparams.mode = OL_TRACE_THRESHOLD;
	tparams.width = ctx->width;
	tparams.height = ctx->height;

	printf("Resolution: %dx%d\n", ctx->width, ctx->height);
	olTraceInit(&trace_ctx, &tparams);

	VideoFrame *last = NULL;

	pthread_mutex_lock(&ctx->display_mode_mutex);
	DisplayMode display_mode = ctx->display_mode;
	pthread_mutex_unlock(&ctx->display_mode_mutex);

	int inf = 0;
	int bg_white = -1;
	float time = 0;
	int frames = 0;
	while (display_mode != STOP) {
		pthread_mutex_lock(&ctx->settings_mutex);
		PlayerSettings settings = ctx->settings;
		int settings_changed = ctx->settings_changed;
		ctx->settings_changed = 0;
		pthread_mutex_unlock(&ctx->settings_mutex);

		if (ctx->audio_idx == -1) {
			drop_all_video(ctx);
			next_video_frame(ctx);
		}

		params.min_length = settings.minsize;
		params.end_dwell = params.start_dwell = settings.dwell;
		params.off_speed = settings.offspeed * 0.002;
		params.snap = (settings.snap*2.0)/(float)maxd;
		params.start_wait = settings.startwait;
		params.end_wait = settings.endwait;
		if (settings.minrate == 0)
			params.max_framelen = 0;
		else
			params.max_framelen = params.rate / settings.minrate;

		olSetRenderParams(&params);

		olLoadIdentity();
		if (aspect > 1) {
			olSetScissor(-1, -iaspect, 1, iaspect);
			olScale(1, iaspect);
		} else {
			olSetScissor(-aspect, -1, aspect, 1);
			olScale(aspect, 1);
		}

		olScale(1 + settings.overscan/100.0, 1 + settings.overscan/100.0);
		olTranslate(-1.0f, 1.0f);
		olScale(2.0f/ctx->width, -2.0f/ctx->height);

		if (!ctx->cur_frame || ctx->cur_frame->seekid < 0) {
			printf("Dummy frame\n");
			float ftime = olRenderFrame(80);
			pthread_mutex_lock(&ctx->display_mode_mutex);
			display_mode = ctx->display_mode;
			pthread_mutex_unlock(&ctx->display_mode_mutex);
			if (ctx->cur_frame && ctx->cur_frame->seekid < 0)
				deliver_event(ctx, time, ftime, frames, 1);
			else
				deliver_event(ctx, time, ftime, frames, 0);
			continue;
		}

		if (last != ctx->cur_frame || settings_changed) {
			tparams.sigma = settings.blur / 100.0;
			if (settings.canny) {
				tparams.mode = OL_TRACE_CANNY;
				tparams.threshold = settings.threshold;
				tparams.threshold2 = settings.threshold2;
				bg_white = -1;
			} else {
				tparams.mode = OL_TRACE_THRESHOLD;
				if (settings.splitthreshold) {
					int edge_off = mind * settings.offset / 100;
					int bsum = 0;
					int cnt = 0;
					int c;
					for (c = edge_off; c < (ctx->width-edge_off); c++) {
						bsum += ctx->cur_frame->data[c+edge_off*ctx->cur_frame->stride];
						bsum += ctx->cur_frame->data[c+(ctx->height-edge_off-1)*ctx->cur_frame->stride];
						cnt += 2;
					}
					for (c = edge_off; c < (ctx->height-edge_off); c++) {
						bsum += ctx->cur_frame->data[edge_off+ctx->cur_frame->stride];
						bsum += ctx->cur_frame->data[(c+1)*ctx->cur_frame->stride-1-edge_off];
						cnt += 2;
					}
					bsum /= cnt;
					if (bg_white == -1)
						bg_white = bsum > ((settings.darkval + settings.lightval)/2);
					if (bg_white && bsum < settings.darkval)
						bg_white = 0;
					if (!bg_white && bsum > settings.lightval)
						bg_white = 1;
					if (bg_white)
						tparams.threshold = settings.threshold2;
					else
						tparams.threshold = settings.threshold;
				} else {
					tparams.threshold = settings.threshold;
				}
			}
			olTraceReInit(trace_ctx, &tparams);
			olTraceFree(&result);
			printf("Trace\n");
			olTrace(trace_ctx, ctx->cur_frame->data, ctx->cur_frame->stride, &result);
			printf("Trace done\n");
			inf++;
			last = ctx->cur_frame;
		}

		int i, j;
		for (i = 0; i < result.count; i++) {
			OLTraceObject *o = &result.objects[i];
			olBegin(OL_POINTS);
			OLTracePoint *p = o->points;
			for (j = 0; j < o->count; j++) {
				if (j % settings.decimation == 0)
					olVertex(p->x, p->y, C_WHITE);
				p++;
			}
			olEnd();
		}

		float ftime = olRenderFrame(80);
		OLFrameInfo info;
		olGetFrameInfo(&info);
		frames++;
		time += ftime;
		printf("Frame time: %.04f, Cur FPS:%6.02f, Avg FPS:%6.02f, Drift: %7.4f, "
				"In %4d, Out %4d Thr %3d/%3d Bg %3d Pts %4d",
				ftime, 1/ftime, frames/time, 0.0, inf, frames,
				tparams.threshold, tparams.threshold2, 0, info.points);
		if (info.resampled_points)
			printf(" Rp %4d Bp %4d", info.resampled_points, info.resampled_blacks);
		if (info.padding_points)
			printf(" Pad %4d", info.padding_points);
		printf("\n");
		deliver_event(ctx, time, ftime, frames, 0);

		pthread_mutex_lock(&ctx->display_mode_mutex);
		display_mode = ctx->display_mode;
		pthread_mutex_unlock(&ctx->display_mode_mutex);
	}

	olTraceDeinit(trace_ctx);

	for(i = 0; i < OL_FRAMES_BUF; i++)
		olRenderFrame(80);

	olShutdown();
	return NULL;
}

int player_init(PlayerCtx *ctx)
{
	ctx->display_mode = PAUSE;
	ctx->settings_changed = 0;
	pthread_mutex_init(&ctx->display_mode_mutex, NULL);

	if (pthread_create(&ctx->display_thread, NULL, display_thread, ctx) != 0)
		return -1;
	return 0;
}

void playvid_init(void)
{
	av_register_all();
	avdevice_register_all();
}

int playvid_open(PlayerCtx **octx, const char *filename)
{
	PlayerCtx *ctx = malloc(sizeof(PlayerCtx));
	if (decoder_init(ctx, filename) < 0)
		return -1;
	ctx->display_mode = STOP;
	ctx->cur_frame = NULL;
	ctx->last_frame_pts = -1;
	ctx->ev_cb = NULL;
	*octx = ctx;
	return 0;
}

void playvid_play(PlayerCtx *ctx)
{
	switch (ctx->display_mode) {
		case STOP:
			player_init(ctx);
			// fallthrough
		case PAUSE:
			pthread_mutex_lock(&ctx->display_mode_mutex);
			ctx->display_mode = PLAY;
			pthread_mutex_unlock(&ctx->display_mode_mutex);
			break;
		case PLAY:
			break;
	}
}

void playvid_pause(PlayerCtx *ctx)
{
	switch (ctx->display_mode) {
		case STOP:
			player_init(ctx);
			break;
		case PLAY:
			pthread_mutex_lock(&ctx->display_mode_mutex);
			ctx->display_mode = PAUSE;
			pthread_mutex_unlock(&ctx->display_mode_mutex);
			break;
		case PAUSE:
			break;
	}
}

void playvid_stop(PlayerCtx *ctx)
{
	if (ctx->display_mode == STOP)
		return;

	pthread_mutex_lock(&ctx->display_mode_mutex);
	ctx->display_mode = STOP;
	pthread_mutex_unlock(&ctx->display_mode_mutex);
	pthread_join(ctx->display_thread, NULL);
}

void playvid_skip(PlayerCtx *ctx)
{
	if (ctx->display_mode != PAUSE)
		return;

	ctx->skip_frame = 1;
}

void playvid_update_settings(PlayerCtx *ctx, PlayerSettings *settings)
{
	if (ctx->display_mode != STOP)
		pthread_mutex_lock(&ctx->settings_mutex);
	ctx->settings = *settings;
	ctx->settings_changed = 1;
	if (ctx->display_mode != STOP)
		pthread_mutex_unlock(&ctx->settings_mutex);
}

void playvid_set_eventcb(PlayerCtx* ctx, PlayerEventCb cb)
{
	ctx->ev_cb = cb;
}

double playvid_get_duration(PlayerCtx *ctx)
{
	return ctx->duration;
}

void playvid_seek(PlayerCtx *ctx, double pos)
{
	pthread_mutex_lock(&ctx->seek_mutex);
	ctx->seek_pos = pos;
	ctx->cur_seekid++;
	pthread_cond_signal(&ctx->seek_cond);
	pthread_mutex_unlock(&ctx->seek_mutex);
}

// kate: space-indent off; indent-width 4; mixedindent off; indent-mode cstyle; 
