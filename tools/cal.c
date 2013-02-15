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

jack_port_t *out_x;
jack_port_t *out_y;
jack_port_t *out_r;
jack_port_t *out_g;
jack_port_t *out_b;

nframes_t rate;

sample_t max_size = 1.0f;

//float w = 110 * (2*M_PI);
float w = 523.251131f / 4.0f * (2*M_PI) / 1;
float pos = 0.0f;

#define MAX(a,b) (((a)<(b))?(b):(a))
#define MIN(a,b) (((a)>(b))?(b):(a))

int process (nframes_t nframes, void *arg)
{
	sample_t *o_x = (sample_t *) jack_port_get_buffer (out_x, nframes);
	sample_t *o_y = (sample_t *) jack_port_get_buffer (out_y, nframes);
	sample_t *o_r = (sample_t *) jack_port_get_buffer (out_r, nframes);
	sample_t *o_g = (sample_t *) jack_port_get_buffer (out_g, nframes);
	sample_t *o_b = (sample_t *) jack_port_get_buffer (out_b, nframes);

	nframes_t frm;
	for (frm = 0; frm < nframes; frm++) {
		sample_t val = max_size;

		*o_r++ = 0.5f * sinf(pos) + 0.5f;
		*o_g++ = 0.5f * sinf(pos + 2.0f*M_PI/3.0f) + 0.5f;
		*o_b++ = 0.5f * sinf(pos + 4.0f*M_PI/3.0f) + 0.5f;

		*o_x++ = cosf(pos) * val;
		*o_y++ = sinf(pos) * val;

		pos += w / rate;
		while(pos >= (2*M_PI)) {
			pos -= (2*M_PI);
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
	static const char jack_client_name[] = "cal";
	jack_status_t jack_status;	

	if ((client = jack_client_open(jack_client_name, JackNullOption, &jack_status)) == 0) {
		fprintf (stderr, "jack server not running?\n");
		return 1;
	}

	jack_set_process_callback (client, process, 0);
	jack_set_buffer_size_callback (client, bufsize, 0);
	jack_set_sample_rate_callback (client, srate, 0);
	jack_on_shutdown (client, jack_shutdown, 0);

	out_x = jack_port_register (client, "out_x", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_y = jack_port_register (client, "out_y", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_r = jack_port_register (client, "out_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_g = jack_port_register (client, "out_g", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_b = jack_port_register (client, "out_b", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		return 1;
	}

	while (1)
		sleep(1);
	jack_client_close (client);
	exit (0);
}

