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

#include "libol.h"
#include "ilda.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#if BYTE_ORDER == LITTLE_ENDIAN
static inline uint16_t swapshort(uint16_t v) {
	return (v >> 8) | (v << 8);
}
# define MAGIC 0x41444C49
#else
static inline uint16_t swapshort(uint16_t v) {
	return v;
}
# define MAGIC 0x494C4441
#endif

#include <stdint.h>

struct ilda_hdr {
	uint32_t magic;
	uint8_t pad1[3];
	uint8_t format;
	char name[8];
	char company[8];
	uint16_t count;
	uint16_t frameno;
	uint16_t framecount;
	uint8_t scanner;
	uint8_t pad2;
} __attribute__((packed));


struct icoord3d {
	int16_t x;
	int16_t y;
	int16_t z;
	uint8_t state;
	uint8_t color;
} __attribute__((packed));

struct icoord2d {
	int16_t x;
	int16_t y;
	uint8_t state;
	uint8_t color;
} __attribute__((packed));

IldaFile *olLoadIlda(const char *filename)
{
	int i;
	FILE *fd = fopen(filename, "rb");
	IldaFile *ild;

	if (!fd) {
		return NULL;
	}

	ild = malloc(sizeof(*ild));

	memset(ild, 0, sizeof(*ild));

	ild->min_x = 1.0;
	ild->min_y = 1.0;
	ild->min_z = 1.0;
	ild->max_x = -1.0;
	ild->max_y = -1.0;
	ild->max_z = -1.0;

	while(!ild->count) {

		struct ilda_hdr hdr;

		if (fread(&hdr, sizeof(hdr), 1, fd) != 1) {
			olLog("ILDA: error while reading header\n");
			olFreeIlda(ild);
			fclose(fd);
			return NULL;
		}

		if (hdr.magic != MAGIC) {
			olLog("ILDA: Invalid magic 0x%08x\n", hdr.magic);
			olFreeIlda(ild);
			fclose(fd);
			return NULL;
		}

		hdr.count = swapshort(hdr.count);
		hdr.frameno = swapshort(hdr.frameno);
		hdr.framecount = swapshort(hdr.framecount);

		switch (hdr.format) {
		case 0:
			olLog("ILD: Got 3D frame, %d points\n", hdr.count);
			ild->points = malloc(sizeof(IldaPoint) * hdr.count);
			struct icoord3d *tmp3d = malloc(sizeof(struct icoord3d) * hdr.count);
			if (fread(tmp3d, sizeof(struct icoord3d), hdr.count, fd) != hdr.count) {
				olLog("ILDA: error while reading frame\n");
				olFreeIlda(ild);
				fclose(fd);
				free(tmp3d);
				return NULL;
			}
			for(i=0; i<hdr.count; i++) {
				ild->points[i].x = ((int16_t)swapshort(tmp3d[i].x)) / 32768.0f;
				ild->points[i].y = ((int16_t)swapshort(tmp3d[i].y)) / 32768.0f;
				ild->points[i].z = ((int16_t)swapshort(tmp3d[i].z)) / 32768.0f;
				ild->points[i].is_blank = !!(tmp3d[i].state & 0x40);
				ild->points[i].color = tmp3d[i].color;
			}
			free(tmp3d);
			ild->count = hdr.count;
			break;
		case 1:
			olLog("Got 2D frame, %d points\n", hdr.count);
			ild->points = malloc(sizeof(IldaPoint) * hdr.count);
			struct icoord2d *tmp2d = malloc(sizeof(struct icoord2d) * hdr.count);
			if (fread(tmp2d, sizeof(struct icoord2d), hdr.count, fd) != hdr.count) {
				olLog("ILDA: error while reading frame\n");
				olFreeIlda(ild);
				fclose(fd);
				free(tmp2d);
				return NULL;
			}
			for(i=0; i<hdr.count; i++) {
				ild->points[i].x = ((int16_t)swapshort(tmp2d[i].x)) / 32768.0f;
				ild->points[i].y = ((int16_t)swapshort(tmp2d[i].y)) / 32768.0f;
				ild->points[i].z = 0;
				ild->points[i].is_blank = !!(tmp2d[i].state & 0x40);
				ild->points[i].color = tmp2d[i].color;
			}
			free(tmp2d);
			ild->count = hdr.count;
			break;
		case 2:
			olLog("ILDA: Got color palette section, %d entries\n", hdr.count);
			olLog("ILDA: NOT SUPPORTED\n");
			olFreeIlda(ild);
			fclose(fd);
			return NULL;
		}
	}

	fclose(fd);

	for(i=0; i<ild->count; i++) {
		if(ild->points[i].x > ild->max_x)
			ild->max_x = ild->points[i].x;
		if(ild->points[i].y > ild->max_y)
			ild->max_y = ild->points[i].y;
		if(ild->points[i].z > ild->max_z)
			ild->max_z = ild->points[i].z;
		if(ild->points[i].x < ild->min_x)
			ild->min_x = ild->points[i].x;
		if(ild->points[i].y < ild->min_y)
			ild->min_y = ild->points[i].y;
		if(ild->points[i].z < ild->min_z)
			ild->min_z = ild->points[i].z;
	}

	return ild;
}

void olDrawIlda(IldaFile *ild)
{
	if (!ild)
		return;
	IldaPoint *p = ild->points;
	int i;
	olBegin(OL_POINTS);
	for (i = 0; i < ild->count; i++) {
		//olLog("%f %f %f %d\n", p->x, p->y, p->z, p->is_blank);
		if (p->is_blank)
			olVertex(p->x, p->y, C_BLACK);
		else
			olVertex(p->x, p->y, C_WHITE);
		p++;
	}
	olEnd();
}

void olDrawIlda3D(IldaFile *ild)
{
	if (!ild)
		return;
	IldaPoint *p = ild->points;
	int i;
	olBegin(OL_POINTS);
	for (i = 0; i < ild->count; i++) {
		if (p->is_blank)
			olVertex3(p->x, p->y, p->z, C_BLACK);
		else
			olVertex3(p->x, p->y, p->z, C_WHITE);
		p++;
	}
	olEnd();
}

void olFreeIlda(IldaFile *ild)
{
	if(ild->points)
		free(ild->points);
	free(ild);
}
