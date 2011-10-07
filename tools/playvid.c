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
This is a simple video player based on libavformat. It originated as a quick
hack based on my metaball/fire tracer from LASE, but evolved to have way too
many settings.

The one ugly as heck hack here is we open the input file twice, once for audio,
once for video. This is because laser display is on one level asynchronous to
audio, and a single video frame may end up taking very long to display, while
we have to keep streaming audio. In other words, doing it this way saves us from
doing more buffering and syncing logic.

On the other hand, audio is pretty much synced to video as long as the input
audio samplerate and video framerate are accurate. This is because in the end
both audio and video go through one synchronized output device, and we keep a
running time value for video that is precisely synced to audio (even though it
can lead or lag audio, we drop or duplicate frames to sync in the long term).
This is unlike your average video player where video is usually explicitly
synced to audio. Here, video is implicitly synced to audio and we only have to
keep track of the variable mapping between laser frames and video frames.

See trace.c for the horrible ad-hoc tracing algorithm. And note that we
currently use the video luma alone and convert it to 1bpp monochrome. It really
is a hack.
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

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#define FRAMES_BUF 8

#define AUDIO_BUF AVCODEC_MAX_AUDIO_FRAME_SIZE

AVFormatContext *pFormatCtx;
AVFormatContext *pAFormatCtx;
AVCodecContext  *pCodecCtx;
AVCodecContext  *pACodecCtx;
AVCodec         *pCodec;
AVCodec         *pACodec;
AVFrame         *pFrame;
ReSampleContext *resampler;

int buffered_samples;
float *poabuf;
float oabuf[AUDIO_BUF];
short iabuf[AUDIO_BUF];

float volume = 0.8;

int videoStream, audioStream;

int GetNextFrame(AVFormatContext *pFormatCtx, AVCodecContext *pCodecCtx,
    int videoStream, AVFrame **oFrame)
{
	static AVPacket packet;
	int             bytesDecoded;
	int             frameFinished = 0;

	while (!frameFinished) {
		do {
			if(av_read_frame(pFormatCtx, &packet)<0) {
				fprintf(stderr, "EOF!\n");
				return 0;
			}
		} while(packet.stream_index!=videoStream);

		bytesDecoded=avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
		if (bytesDecoded < 0)
		{
			fprintf(stderr, "Error while decoding frame\n");
			return 0;
		}
		if (bytesDecoded != packet.size) {
			printf("Multiframe packets not supported (%d != %d)\n", bytesDecoded, packet.size);
			exit(1);
		}
	}
	*oFrame = pFrame;
	return 1;
}

void moreaudio(float *lb, float *rb, int samples)
{
	AVPacket packet;
	int bytes, bytesDecoded;
	int input_samples;
	while (samples)
	{
		if (!buffered_samples) {
			do {
				if(av_read_frame(pAFormatCtx, &packet)<0) {
					fprintf(stderr, "Audio EOF!\n");
					memset(lb, 0, samples*sizeof(float));
					memset(rb, 0, samples*sizeof(float));
					return;
				}
			} while(packet.stream_index!=audioStream);

			bytes = AUDIO_BUF * sizeof(short);

			bytesDecoded = avcodec_decode_audio3(pACodecCtx, iabuf, &bytes, &packet);
			if(bytesDecoded < 0)
			{
				fprintf(stderr, "Error while decoding audio frame\n");
				return;
			}

			input_samples = bytes / (sizeof(short)*pACodecCtx->channels);

			buffered_samples = audio_resample(resampler, (void*)oabuf, iabuf, input_samples);
			poabuf = oabuf;
		}

		*lb++ = *poabuf++ * volume;
		*rb++ = *poabuf++ * volume;
		buffered_samples--;
		samples--;
	}
}

int	 av_vid_init(char *file)
{
	int i;

	if (av_open_input_file(&pFormatCtx, file, NULL, 0, NULL)!=0)
		return -1;

	if (av_find_stream_info(pFormatCtx)<0)
		return -1;

	dump_format(pFormatCtx, 0, file, 0);

	videoStream=-1;
	for (i=0; i<pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
			videoStream=i;
			break;
		}
	}
	if (videoStream==-1)
		return -1;

	pCodecCtx=pFormatCtx->streams[videoStream]->codec;

	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec==NULL)
		return -1;

	if (avcodec_open(pCodecCtx, pCodec)<0)
		return -1;

	pFrame=avcodec_alloc_frame();

	return 0;
}

int av_aud_init(char *file)
{
	int i;

	av_register_all();

	if (av_open_input_file(&pAFormatCtx, file, NULL, 0, NULL)!=0)
		return -1;

	if (av_find_stream_info(pAFormatCtx)<0)
		return -1;

	audioStream=-1;
	for (i=0; i<pAFormatCtx->nb_streams; i++)
		if (pAFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO)
		{
			audioStream=i;
			break;
		}
	if (audioStream==-1)
		return -1;

	pACodecCtx=pAFormatCtx->streams[audioStream]->codec;

	pACodec=avcodec_find_decoder(pACodecCtx->codec_id);
	if (pACodec==NULL)
		return -1;

	if (avcodec_open(pACodecCtx, pACodec)<0)
		return -1;

	resampler = av_audio_resample_init(2, pACodecCtx->channels,
									   48000, pACodecCtx->sample_rate,
									   SAMPLE_FMT_FLT, pACodecCtx->sample_fmt,
									   16, 10, 0, 0.8);

	if (!resampler)
		return -1;

	buffered_samples = 0;

	return 0;
}

int av_deinit(void)
{
	av_free(pFrame);

	// Close the codec
	avcodec_close(pCodecCtx);
	avcodec_close(pACodecCtx);

	audio_resample_close(resampler);

	// Close the video file
	av_close_input_file(pFormatCtx);
	av_close_input_file(pAFormatCtx);

	return 0;
}

void usage(const char *argv0)
{
	printf("Usage: %s [options] inputfile\n\n", argv0);
	printf("Options:\n");
	printf("-c        Use Canny edge detector instead of thresholder\n");
	printf("-t INT    Tracing threshold\n");
	printf("-b INT    Tracing threshold for dark-background (black) scenes\n");
	printf("-w INT    Tracing threshold for light-background (white) scenes\n");
	printf("-T INT    Second tracing threshold (canny)\n");
	printf("-B INT    Average edge value at which the scene is considered dark\n");
	printf("-W INT    Average edge value at which the scene is considered light\n");
	printf("-O INT    Edge offset\n");
	printf("-d INT    Decimation factor\n");
	printf("-m INT    Minimum object size in samples\n");
	printf("-S INT    Start wait in samples\n");
	printf("-E INT    End wait in samples\n");
	printf("-D INT    Start/end dwell in samples\n");
	printf("-g FLOAT  Gaussian blur sigma\n");
	printf("-s FLOAT  Inverse off (inter-object) scan speed (in samples per screen width)\n");
	printf("-p FLOAT  Snap distance in video pixels\n");
	printf("-a FLOAT  Force aspect ratio\n");
	printf("-r FLOAT  Force framerate\n");
	printf("-R FLOAT  Minimum framerate (resample slow frames to be faster)\n");
	printf("-o FLOAT  Overscan factor (to get rid of borders etc.)\n");
	printf("-v FLOAT  Audio volume\n");
}

int main (int argc, char *argv[])
{
	OLRenderParams params;
	AVFrame *frame;
	int i;

	// Register all formats and codecs
	av_register_all();

	memset(&params, 0, sizeof params);
	params.rate = 48000;
	params.on_speed = 2.0/100.0;
	params.off_speed = 2.0/15.0;
	params.start_wait = 8;
	params.end_wait = 3;
	params.snap = 1/120.0;
	params.render_flags = RENDER_GRAYSCALE;
	params.min_length = 4;
	params.start_dwell = 2;
	params.end_dwell = 2;

	float snap_pix = 3;
	float aspect = 0;
	float framerate = 0;
	float overscan = 0;
	int thresh_dark = 60;
	int thresh_light = 160;
	int sw_dark = 100;
	int sw_light = 256;
	int decimate = 2;
	int edge_off = 0;

	int optchar;

	OLTraceParams tparams = {
		.mode = OL_TRACE_THRESHOLD,
		.sigma = 0,
		.threshold2 = 50
	};

	while ((optchar = getopt(argc, argv, "hct:T:b:w:B:W:O:d:m:S:E:D:g:s:p:a:r:R:o:v:")) != -1) {
		switch (optchar) {
			case 'h':
			case '?':
				usage(argv[0]);
				return 0;
			case 'c':
				tparams.mode = OL_TRACE_CANNY;
				tparams.sigma = 1;
				break;
			case 't':
				thresh_dark = thresh_light = atoi(optarg);
				break;
			case 'T':
				tparams.threshold2 = atoi(optarg);
				break;
			case 'b':
				thresh_dark = atoi(optarg);
				break;
			case 'w':
				thresh_light = atoi(optarg);
				break;
			case 'B':
				sw_dark = atoi(optarg);
				break;
			case 'W':
				sw_light = atoi(optarg);
				break;
			case 'O':
				edge_off = atoi(optarg);
				break;
			case 'd':
				decimate = atoi(optarg);
				break;
			case 'm':
				params.min_length = atoi(optarg);
				break;
			case 'S':
				params.start_wait = atoi(optarg);
				break;
			case 'E':
				params.end_wait = atoi(optarg);
				break;
			case 'D':
				params.start_dwell = atoi(optarg);
				params.end_dwell = atoi(optarg);
				break;
			case 'g':
				tparams.sigma = atof(optarg);
				break;
			case 's':
				params.off_speed = 2.0f/atof(optarg);
				break;
			case 'p':
				snap_pix = atof(optarg);
				break;
			case 'a':
				aspect = atof(optarg);
				break;
			case 'r':
				framerate = atof(optarg);
				break;
			case 'R':
				params.max_framelen = params.rate/atof(optarg);
				break;
			case 'o':
				overscan = atof(optarg);
				break;
			case 'v':
				volume = atof(optarg);
				break;
		}
	}

	if (optind == argc) {
		usage(argv[0]);
		return 1;
	}

	if (av_vid_init(argv[optind]) != 0) {
		printf("Video open/init failed\n");
		return 1;
	}
	if (av_aud_init(argv[optind]) != 0) {
		printf("Audio open/init failed\n");
		return 1;
	}

	if(olInit(FRAMES_BUF, 300000) < 0) {
		printf("OpenLase init failed\n");
		return 1;
	}

	if (aspect == 0)
		aspect = pCodecCtx->width / (float)pCodecCtx->height;

	if (framerate == 0)
		framerate = (float)pFormatCtx->streams[videoStream]->r_frame_rate.num / (float)pFormatCtx->streams[videoStream]->r_frame_rate.den;

	float iaspect = 1/aspect;

	if (aspect > 1) {
		olSetScissor(-1, -iaspect, 1, iaspect);
		olScale(1, iaspect);
	} else {
		olSetScissor(-aspect, -1, aspect, 1);
		olScale(aspect, 1);
	}

	printf("Aspect is %f %f\n", aspect, iaspect);
	printf("Overscan is %f\n", overscan);

	olScale(1+overscan, 1+overscan);
	olTranslate(-1.0f, 1.0f);
	olScale(2.0f/pCodecCtx->width, -2.0f/pCodecCtx->height);

	int maxd = pCodecCtx->width > pCodecCtx->height ? pCodecCtx->width : pCodecCtx->height;
	params.snap = (snap_pix*2.0)/(float)maxd;

	float frametime = 1.0f/framerate;
	printf("Framerate: %f (%fs per frame)\n", framerate, frametime);

	olSetAudioCallback(moreaudio);
	olSetRenderParams(&params);

	float vidtime = 0;
	int inf=0;
	int bg_white = -1;
	float time = 0;
	float ftime;
	int frames = 0;

	OLFrameInfo info;

	OLTraceCtx *trace_ctx;

	OLTraceResult result;

	memset(&result, 0, sizeof(result));

	tparams.width = pCodecCtx->width,
	tparams.height = pCodecCtx->height,
	olTraceInit(&trace_ctx, &tparams);

	while(GetNextFrame(pFormatCtx, pCodecCtx, videoStream, &frame)) {
		if (inf == 0)
			printf("Frame stride: %d\n", frame->linesize[0]);
		inf+=1;
		if (vidtime < time) {
			vidtime += frametime;
			printf("Frame skip!\n");
			continue;
		}
		vidtime += frametime;

		int thresh;
		int obj;
		int bsum = 0;
		int c;
		for (c=edge_off; c<(pCodecCtx->width-edge_off); c++) {
			bsum += frame->data[0][c+edge_off*frame->linesize[0]];
			bsum += frame->data[0][c+(pCodecCtx->height-edge_off-1)*frame->linesize[0]];
		}
		for (c=edge_off; c<(pCodecCtx->height-edge_off); c++) {
			bsum += frame->data[0][edge_off+c*frame->linesize[0]];
			bsum += frame->data[0][(c+1)*frame->linesize[0]-1-edge_off];
		}
		bsum /= (2*(pCodecCtx->width+pCodecCtx->height));
		if (bg_white == -1)
			bg_white = bsum > 128;
		if (bg_white && bsum < sw_dark)
			bg_white = 0;
		if (!bg_white && bsum > sw_light)
			bg_white = 1;

		if (bg_white)
			thresh = thresh_light;
		else
			thresh = thresh_dark;

		tparams.threshold = thresh;
		olTraceReInit(trace_ctx, &tparams);
		olTraceFree(&result);
		obj = olTrace(trace_ctx, frame->data[0], frame->linesize[0], &result);

		do {
			int i, j;
			for (i = 0; i < result.count; i++) {
				OLTraceObject *o = &result.objects[i];
				olBegin(OL_POINTS);
				OLTracePoint *p = o->points;
				for (j = 0; j < o->count; j++) {
					if (j % decimate == 0)
						olVertex(p->x, p->y, C_WHITE);
					p++;
				}
				olEnd();
			}

			ftime = olRenderFrame(200);
			olGetFrameInfo(&info);
			frames++;
			time += ftime;
			printf("Frame time: %.04f, Cur FPS:%6.02f, Avg FPS:%6.02f, Drift: %7.4f, "
				   "In %4d, Out %4d Thr %3d Bg %3d Pts %4d",
				   ftime, 1/ftime, frames/time, time-vidtime,
				   inf, frames, thresh, bsum, info.points);
			if (info.resampled_points)
				printf(" Rp %4d Bp %4d", info.resampled_points, info.resampled_blacks);
			if (info.padding_points)
				printf(" Pad %4d", info.padding_points);
			printf("\n");
		} while ((time+frametime) < vidtime);
	}

	olTraceDeinit(trace_ctx);

	for(i=0;i<FRAMES_BUF;i++)
		olRenderFrame(200);

	olShutdown();
	av_deinit();
	exit (0);
}
