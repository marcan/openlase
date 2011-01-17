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

#ifndef VIDEO_H
#define VIDEO_H

typedef struct {
	char *file;
	int thresh_dark;
	int thresh_light;
	int sw_dark;
	int sw_light;
	int edge_off;
	int decimate;
	float overscan;
	float aspect;
	float framerate;
	float volume;

	int min_length;
	int start_wait;
	int end_wait;
	int off_speed;
	int snap_pix;
	float min_framerate;
} video_params;

#endif