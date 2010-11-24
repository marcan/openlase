/*
        OpenLase - a realtime laser graphics toolkit

Copyright (C) 2009-2010 Hector Martin "marcan" <hector@marcansoft.com>

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

#ifndef ILD_H
#define ILD_H

#include <stdint.h>

typedef struct {
	float x;
	float y;
	float z;
	int is_blank;
	uint8_t color;
} IldaPoint;

typedef struct {
	int count;
	float min_x;
	float max_x;
	float min_y;
	float max_y;
	float min_z;
	float max_z;
	IldaPoint *points;
} IldaFile;

IldaFile *olLoadIlda(const char *filename);
void olDrawIlda(IldaFile *ild);
void olDrawIlda3D(IldaFile *ild);
void olFreeIlda(IldaFile *ild);

#endif