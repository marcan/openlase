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

#define _BSD_SOURCE

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <jack/jack.h>

#include <stdint.h>
#include <math.h>

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;

jack_port_t *in_l;
jack_port_t *in_r;
jack_port_t *out_x;
jack_port_t *out_y;
jack_port_t *out_w;

nframes_t rate;

sample_t max_size = 1.0f;
sample_t min_size = 0.2f;
sample_t boost = 8;

//float w = 110 * (2*M_PI);
float w = 523.251131f / 4.0f;
float pos = 0.0f;

#define MAX(a,b) (((a)<(b))?(b):(a))
#define MIN(a,b) (((a)>(b))?(b):(a))

int process (nframes_t nframes, void *arg)
{
	sample_t *i_l = (sample_t *) jack_port_get_buffer (in_l, nframes);
	sample_t *i_r = (sample_t *) jack_port_get_buffer (in_r, nframes);
	sample_t *o_x = (sample_t *) jack_port_get_buffer (out_x, nframes);
	sample_t *o_y = (sample_t *) jack_port_get_buffer (out_y, nframes);
	sample_t *o_w = (sample_t *) jack_port_get_buffer (out_w, nframes);

	nframes_t frm;
	for (frm = 0; frm < nframes; frm++) {
		sample_t val = (*i_l++ + *i_r++) / 2;

		val *= boost;
		val = MAX(MIN(val,1.0f),-1.0f);

		if (pos < 0.05 || pos > 0.95)
			*o_w++ = 0.0f;
		else
			*o_w++ = 1.0f;
		*o_x++ = pos * 2 - 1;
		*o_y++ = val;

		pos += w / rate;
		while(pos >= 1) {
			pos -= 1;
		}
	}

	return 0;
}

int bufsize (nframes_t nframes, void *arg)
{
	printf ("the maximum buffer size is now %u\n", nframes);
	return 0;
}

int srate (nframes_t nframes, void *arg)
{
	rate = nframes;
	printf ("Sample rate: %u/sec\n", nframes);
	return 0;
}

void jack_shutdown (void *arg)
{
	exit (1);
}

int main (int argc, char *argv[])
{
	jack_client_t *client;
	char jack_client_name[] = "scope";
	jack_status_t jack_status;
	jack_options_t  jack_options = JackNullOption;		

	if ((client = jack_client_open(jack_client_name, jack_options, &jack_status)) == 0) {
		fprintf (stderr, "jack server not running?\n");
		return 1;
	}

	jack_set_process_callback (client, process, 0);
	jack_set_buffer_size_callback (client, bufsize, 0);
	jack_set_sample_rate_callback (client, srate, 0);
	jack_on_shutdown (client, jack_shutdown, 0);

	in_l = jack_port_register (client, "in_l", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_r = jack_port_register (client, "in_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	out_x = jack_port_register (client, "out_x", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_y = jack_port_register (client, "out_y", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_w = jack_port_register (client, "out_w", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		return 1;
	}

	while (1)
		sleep(1);
	jack_client_close (client);
	exit (0);
}

