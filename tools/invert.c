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

jack_port_t *in_1;
jack_port_t *in_2;
jack_port_t *in_3;
jack_port_t *in_4;
jack_port_t *in_5;
jack_port_t *in_6;

jack_port_t *out_1;
jack_port_t *out_2;
jack_port_t *out_3;
jack_port_t *out_4;
jack_port_t *out_5;
jack_port_t *out_6;

jack_port_t *out_i1;
jack_port_t *out_i2;
jack_port_t *out_i3;
jack_port_t *out_i4;
jack_port_t *out_i5;
jack_port_t *out_i6;

nframes_t rate;

int process (nframes_t nframes, void *arg)
{
	sample_t *i_1 = (sample_t *) jack_port_get_buffer (in_1, nframes);
	sample_t *i_2 = (sample_t *) jack_port_get_buffer (in_2, nframes);
	sample_t *i_3 = (sample_t *) jack_port_get_buffer (in_3, nframes);
	sample_t *i_4 = (sample_t *) jack_port_get_buffer (in_4, nframes);
	sample_t *i_5 = (sample_t *) jack_port_get_buffer (in_5, nframes);
	sample_t *i_6 = (sample_t *) jack_port_get_buffer (in_6, nframes);

	sample_t *o_1 = (sample_t *) jack_port_get_buffer (out_1, nframes);
	sample_t *o_2 = (sample_t *) jack_port_get_buffer (out_2, nframes);
	sample_t *o_3 = (sample_t *) jack_port_get_buffer (out_3, nframes);
	sample_t *o_4 = (sample_t *) jack_port_get_buffer (out_4, nframes);
	sample_t *o_5 = (sample_t *) jack_port_get_buffer (out_5, nframes);
	sample_t *o_6 = (sample_t *) jack_port_get_buffer (out_6, nframes);

	sample_t *o_i1 = (sample_t *) jack_port_get_buffer (out_i1, nframes);
	sample_t *o_i2 = (sample_t *) jack_port_get_buffer (out_i2, nframes);
	sample_t *o_i3 = (sample_t *) jack_port_get_buffer (out_i3, nframes);
	sample_t *o_i4 = (sample_t *) jack_port_get_buffer (out_i4, nframes);
	sample_t *o_i5 = (sample_t *) jack_port_get_buffer (out_i5, nframes);
	sample_t *o_i6 = (sample_t *) jack_port_get_buffer (out_i6, nframes);

	nframes_t frm;
	for (frm = 0; frm < nframes; frm++) {
		sample_t s_1 = *i_1++;
		sample_t s_2 = *i_2++;
		sample_t s_3 = *i_3++;
		sample_t s_4 = *i_4++;
		sample_t s_5 = *i_5++;
		sample_t s_6 = *i_6++;

		*o_1++ = s_1;
		*o_2++ = s_2;
		*o_3++ = s_3;
		*o_4++ = s_4;
		*o_5++ = s_5;
		*o_6++ = s_6;

		*o_i1++ = -s_1;
		*o_i2++ = -s_2;
		*o_i3++ = -s_3;
		*o_i4++ = -s_4;
		*o_i5++ = -s_5;
		*o_i6++ = -s_6;
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

	if ((client = jack_client_new ("invert")) == 0) {
		fprintf (stderr, "jack server not running?\n");
		return 1;
	}

	jack_set_process_callback (client, process, 0);
	jack_set_buffer_size_callback (client, bufsize, 0);
	jack_set_sample_rate_callback (client, srate, 0);
	jack_on_shutdown (client, jack_shutdown, 0);

	in_1 = jack_port_register (client, "in_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_2 = jack_port_register (client, "in_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_3 = jack_port_register (client, "in_3", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_4 = jack_port_register (client, "in_4", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_5 = jack_port_register (client, "in_5", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	in_6 = jack_port_register (client, "in_6", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

	out_1 = jack_port_register (client, "thru_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_2 = jack_port_register (client, "thru_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_3 = jack_port_register (client, "thru_3", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_4 = jack_port_register (client, "thru_4", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_5 = jack_port_register (client, "thru_5", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_6 = jack_port_register (client, "thru_6", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	out_i1 = jack_port_register (client, "invert_1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_i2 = jack_port_register (client, "invert_2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_i3 = jack_port_register (client, "invert_3", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_i4 = jack_port_register (client, "invert_4", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_i5 = jack_port_register (client, "invert_5", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	out_i6 = jack_port_register (client, "invert_6", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		return 1;
	}

	while (1)
		sleep(1);
	jack_client_close (client);
	exit (0);
}

