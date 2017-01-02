/*
        OpenLase - a realtime laser graphics toolkit

Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 or version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <jack/jack.h>

#include "output.h"

#include <QApplication>
#include <QString>
#include "output_settings.h"

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;

jack_port_t *in_x;
jack_port_t *in_y;
jack_port_t *in_r;
jack_port_t *in_g;
jack_port_t *in_b;

jack_port_t *out_x;
jack_port_t *out_y;
jack_port_t *out_r;
jack_port_t *out_g;
jack_port_t *out_b;
jack_port_t *out_e;

nframes_t rate, enable_period, enable_ctr;
nframes_t frames_dead;

#define DEAD_TIME (rate/10)

nframes_t ibuf_r = 0;
nframes_t ibuf_g = 0;
nframes_t ibuf_b = 0;
sample_t buf_r[MAX_DELAY];
sample_t buf_g[MAX_DELAY];
sample_t buf_b[MAX_DELAY];

output_config_t *cfg;

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

static void generate_enable(sample_t *buf, nframes_t nframes)
{
	while (nframes--) {
		if (enable_ctr < (enable_period / 2))
			*buf++ = -1.0;
		else
			*buf++ = 1.0;
		enable_ctr = (enable_ctr + 1) % enable_period;
	}
}

static void transform(sample_t *ox, sample_t *oy)
{
	float x,y,w;
	x = *ox;
	y = *oy;
	
	*ox = cfg->transform[0][0]*x + cfg->transform[0][1]*y + cfg->transform[0][2];
	*oy = cfg->transform[1][0]*x + cfg->transform[1][1]*y + cfg->transform[1][2];
	w = cfg->transform[2][0]*x + cfg->transform[2][1]*y + cfg->transform[2][2];
	
	*ox /= w;
	*oy /= w;
}

static void cfilter(float *c, float *p)
{
	float delta = fabsf(*c - *p);
	if (delta > (cfg->cLimit / 1000.0f)) {
		if (*c > *p)
			*p = *c - (cfg->cLimit / 1000.0f);
		else
			*p = *c + (cfg->cLimit / 1000.0f);
	} else {
		*p = (1 - (cfg->cRatio / 1000.0f)) * *p + (cfg->cRatio / 1000.0f) * *c;
	}

	*c += *c - *p;
}

static void dfilter(float *c, float *p)
{
	float delta = *c - *p;

	*p = (1 - (cfg->dRatio / 1000.0f)) * *p + (cfg->dRatio / 1000.0f) * *c;

	*c += (cfg->dPower / 1000.0f) * delta;
}

static inline void filter(float *x, float *y)
{
	static float px=0, py=0;
	static float dx=0, dy=0;
	if (cfg->scan_flags & FILTER_D) {
		dfilter(x, &dx);
		dfilter(y, &dy);
	}
	if (cfg->scan_flags & FILTER_C) {
		cfilter(x, &px);
		cfilter(y, &py);
	}
}

static inline float scale_color(float c, int c_max, int c_min, int blank, int power)
{
	if ( c < 0.001 ) {
		return blank / 100.0f;
	}
	else {
		return ((c * power / 100.f * (c_max - c_min)) + c_min) / 100.0f;
	}
}

static int process (nframes_t nframes, void *arg)
{
	sample_t *o_x = (sample_t *) jack_port_get_buffer (out_x, nframes);
	sample_t *o_y = (sample_t *) jack_port_get_buffer (out_y, nframes);
	sample_t *o_r = (sample_t *) jack_port_get_buffer (out_r, nframes);
	sample_t *o_g = (sample_t *) jack_port_get_buffer (out_g, nframes);
	sample_t *o_b = (sample_t *) jack_port_get_buffer (out_b, nframes);
	sample_t *o_e = (sample_t *) jack_port_get_buffer (out_e, nframes);
	
	sample_t *i_x = (sample_t *) jack_port_get_buffer (in_x, nframes);
	sample_t *i_y = (sample_t *) jack_port_get_buffer (in_y, nframes);
	sample_t *i_r = (sample_t *) jack_port_get_buffer (in_r, nframes);
	sample_t *i_g = (sample_t *) jack_port_get_buffer (in_g, nframes);
	sample_t *i_b = (sample_t *) jack_port_get_buffer (in_b, nframes);

	nframes_t frm;

	for (frm = 0; frm < nframes; frm++) {
		sample_t x,y,r,g,b,orig_r,orig_g,orig_b;
		x = *i_x++;
		y = *i_y++;
		r = orig_r = *i_r++;
		g = orig_g = *i_g++;
		b = orig_b = *i_b++;

		y = -y;
		transform(&x, &y);
		y = -y;

		if (cfg->scan_flags & SWAP_XY) {
			sample_t tmp;
			tmp = x;
			x = y;
			y = tmp;
		}

		if (cfg->scan_flags & INVERT_X)
			x = -x;
		if (cfg->scan_flags & INVERT_Y)
			y = -y;
		if (!(cfg->scan_flags & ENABLE_X) && !(cfg->scan_flags & SAFE))
			x = 0.0f;
		if (!(cfg->scan_flags & ENABLE_Y) && !(cfg->scan_flags & SAFE))
			y = 0.0f;
		if ((cfg->scan_flags & SAFE) && cfg->scanSize < 10) {
			x *= 0.10f;
			y *= 0.10f;
		} else {
			x *= cfg->scanSize / 100.0f;
			y *= cfg->scanSize / 100.0f;
		}
	
		if (cfg->blank_flags & BLANK_INVERT) {
			r = 1.0f - r;
			g = 1.0f - g;
			b = 1.0f - b;
		}
		if (cfg->color_flags & COLOR_MONOCHROME) {
			float v = max(r,max(g,b));
			r = g = b = v;
		}

		if (!(cfg->blank_flags & BLANK_ENABLE)) {
			r = 1.0f;
			g = 1.0f;
			b = 1.0f;
		}

		r = scale_color(r, cfg->redMax, cfg->redMin, cfg->redBlank, cfg->power);
		g = scale_color(g, cfg->greenMax, cfg->greenMin, cfg->greenBlank, cfg->power);
		b = scale_color(b, cfg->blueMax, cfg->blueMin, cfg->blueBlank, cfg->power);

		if (!(cfg->blank_flags & OUTPUT_ENABLE)) {
			r = 0.0f;
			g = 0.0f;
			b = 0.0f;
		}
		if (!(cfg->color_flags & COLOR_RED)) {
			r = 0.0f;
		}
		if (!(cfg->color_flags & COLOR_GREEN)) {
			g = 0.0f;
		}
		if (!(cfg->color_flags & COLOR_BLUE)) {
			b = 0.0f;
		}
		if (cfg->colorMode == COLORMODE_TTL) {
			// TTL color
			r = (r >= 0.5f? 1.0f: 0.0f);
			g = (g >= 0.5f? 1.0f: 0.0f);
			b = (b >= 0.5f? 1.0f: 0.0f);
		}

		if (cfg->colorMode == COLORMODE_MODULATED) {
			// Modulated at half the sample rate (for AC-coupling)
			if (frm % 2) {
				r = -r;
				g = -g;
				b = -b;
			}
		}

		if(orig_r == 0.0f && orig_g == 0.0f && orig_b == 0.0f) {
			if(frames_dead >= DEAD_TIME) {
				r = 0.0f;
				g = 0.0f;
				b = 0.0f;
			} else {
				frames_dead++;
			}
		} else {
		    frames_dead = 0;
		}

		filter(&x, &y);

		*o_x++ = x;
		*o_y++ = y;
		buf_r[ibuf_r] = r;
		buf_g[ibuf_g] = g;
		buf_b[ibuf_b] = b;
		*o_r++ = buf_r[(ibuf_r + MAX_DELAY - cfg->redDelay) % MAX_DELAY];
		*o_g++ = buf_g[(ibuf_g + MAX_DELAY - cfg->greenDelay) % MAX_DELAY];
		*o_b++ = buf_b[(ibuf_b + MAX_DELAY - cfg->blueDelay) % MAX_DELAY];
		ibuf_r = (ibuf_r + 1) % MAX_DELAY;
		ibuf_g = (ibuf_g + 1) % MAX_DELAY;
		ibuf_b = (ibuf_b + 1) % MAX_DELAY;
	}
	generate_enable(o_e, nframes);

	return 0;
}

static int bufsize (nframes_t nframes, void *arg)
{
	printf ("the maximum buffer size is now %u\n", nframes);
	return 0;
}

static int srate (nframes_t nframes, void *arg)
{
	rate = nframes;
	if(rate % 1000) {
		printf("error: the sample rate should be a multiple of 1000\n");
		exit(1);
	}
	enable_period = nframes / 1000;
	enable_ctr = 0;
	printf ("Sample rate: %u/sec\n", nframes);
	return 0;
}

static void jack_shutdown (void *arg)
{
	exit (1);
}

int main (int argc, char *argv[])
{
	int retval;
	jack_client_t *client;
	static const char jack_client_name[] = "output";
	jack_status_t jack_status;	

	QApplication app(argc, argv);
	OutputSettings settings;

	if (argc > 1)
		settings.loadSettings(QString(argv[1]));

	cfg = &settings.cfg;

	if ((client = jack_client_open(jack_client_name, JackNullOption, &jack_status)) == 0) {
		fprintf (stderr, "jack server not running?\n");
		return 1;
	}

	jack_set_process_callback (client, process, 0);
	jack_set_buffer_size_callback (client, bufsize, 0);
	jack_set_sample_rate_callback (client, srate, 0);
	jack_on_shutdown (client, jack_shutdown, 0);

	in_x = jack_port_register (client, "in_x", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_y = jack_port_register (client, "in_y", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_r = jack_port_register (client, "in_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_g = jack_port_register (client, "in_g", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_b = jack_port_register (client, "in_b", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	out_x = jack_port_register (client, "out_x", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_y = jack_port_register (client, "out_y", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_r = jack_port_register (client, "out_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_g = jack_port_register (client, "out_g", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_b = jack_port_register (client, "out_b", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_e = jack_port_register (client, "out_e", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		return 1;
	}

	settings.show();

	retval = app.exec();

	jack_client_close (client);
	return retval;
}

