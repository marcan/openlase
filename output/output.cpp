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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <jack/jack.h>

#include "output.h"

#include <QApplication>
#include "output_settings.h"

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;

jack_port_t *in_x;
jack_port_t *in_y;
jack_port_t *in_g;

jack_port_t *out_x;
jack_port_t *out_y;
jack_port_t *out_g;
jack_port_t *out_e;

nframes_t rate, enable_period, enable_ctr;
nframes_t frames_dead;

#define DEAD_TIME (rate/10)

nframes_t ibuf_g = 0;
sample_t buf_g[MAX_DELAY];

output_config_t *cfg;

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

/* The following filters attempt to compensate for imperfections in my galvos.
You'll probably want to turn them off for anything else */

#define LIMIT 0.007
#define RATIO 0.30

static void cfilter(float *c, float *p)
{
	float delta = fabsf(*c - *p);
	if (delta > LIMIT) {
		if (*c > *p)
			*p = *c - LIMIT;
		else
			*p = *c + LIMIT;
	} else {
		*p = (1-RATIO) * *p + RATIO * *c;
	}

	*c += *c - *p;
}

#define DPOWER 0.05
#define DRATIO 0.05

static void dfilter(float *c, float *p)
{
	float delta = *c - *p;
	*c += DPOWER*delta;

	*p = (1-DRATIO) * *p + DRATIO * *c;
}

static inline void filter(float *x, float *y)
{
	static float px=0, py=0;
	static float dx=0, dy=0;
	dfilter(x,&dx);
	dfilter(y,&dy);
	cfilter(x,&px);
	cfilter(y,&py);
}

static int process (nframes_t nframes, void *arg)
{
	sample_t *o_x = (sample_t *) jack_port_get_buffer (out_x, nframes);
	sample_t *o_y = (sample_t *) jack_port_get_buffer (out_y, nframes);
	sample_t *o_g = (sample_t *) jack_port_get_buffer (out_g, nframes);
	sample_t *o_e = (sample_t *) jack_port_get_buffer (out_e, nframes);
	
	sample_t *i_x = (sample_t *) jack_port_get_buffer (in_x, nframes);
	sample_t *i_y = (sample_t *) jack_port_get_buffer (in_y, nframes);
	sample_t *i_g = (sample_t *) jack_port_get_buffer (in_g, nframes);

	nframes_t frm;

	for (frm = 0; frm < nframes; frm++) {
		sample_t x,y,g,orig_g;
		x = *i_x++;
		y = *i_y++;
		g = orig_g = *i_g++;

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
		if (!(cfg->scan_flags & ENABLE_X) && !cfg->safe)
			x = 0.0f;
		if (!(cfg->scan_flags & ENABLE_Y) && !cfg->safe)
			y = 0.0f;
		if (cfg->safe && cfg->size < 0.10f) {
			x *= 0.10f;
			y *= 0.10f;
		} else {
			x *= cfg->size;
			y *= cfg->size;
		}
		
		if (cfg->blank_flags & BLANK_INVERT)
			g = 1.0f - g;
		if (!(cfg->blank_flags & BLANK_ENABLE))
			g = 1.0f;
		if (!(cfg->blank_flags & OUTPUT_ENABLE))
			g = 0.0f;
		g *= cfg->power * (1.0f-cfg->offset);
		g += cfg->offset;

		if(orig_g == 0.0f) {
			if(frames_dead >= DEAD_TIME) {
				g = 0.0f;
			} else {
				frames_dead++;
			}
		} else {
		    frames_dead = 0;
		}
		
		filter(&x, &y);
		
		*o_x++ = x;
		*o_y++ = y;
		buf_g[ibuf_g] = g;
		*o_g++ = buf_g[(ibuf_g + MAX_DELAY - cfg->delay) % MAX_DELAY];
		ibuf_g = (ibuf_g + 1) % MAX_DELAY;
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
	char jack_client_name[] = "output";
	jack_status_t jack_status;	
	jack_options_t  jack_options = JackNullOption;		

	QApplication app(argc, argv);
	OutputSettings settings;
	
	cfg = &settings.cfg;

	if ((client = jack_client_open(jack_client_name, jack_options, &jack_status)) == 0) {
		fprintf (stderr, "jack server not running?\n");
		return 1;
	}

	jack_set_process_callback (client, process, 0);
	jack_set_buffer_size_callback (client, bufsize, 0);
	jack_set_sample_rate_callback (client, srate, 0);
	jack_on_shutdown (client, jack_shutdown, 0);

	in_x = jack_port_register (client, "in_x", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_y = jack_port_register (client, "in_y", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_g = jack_port_register (client, "in_g", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	out_x = jack_port_register (client, "out_x", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_y = jack_port_register (client, "out_y", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_g = jack_port_register (client, "out_g", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
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

