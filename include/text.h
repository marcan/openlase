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

#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
	OL_FONT_DEFAULT = 0,
	/* future: add more OL_FONT_SOMETHING types here */
} olFontType;

typedef struct {
	int flag;
	float x;
	float y;
} FontPoint;

typedef struct {
	float width;
	const FontPoint *points;
} FontChar;

typedef struct {
	float height;
	float overlap;
	const FontChar *chars;
	void *font_data;
	olFontType font_type;
} Font;

Font *olGetDefaultFont(void);
Font *olGetFont(olFontType font_type, const char *fontname);
void olFreeFont(Font *font);
float olGetCharWidth(Font *font, float height, char c);
float olGetStringWidth(Font *fnt, float height, const char *s);
float olGetCharOverlap(Font *font, float height);
float olDrawChar(Font *fnt, float x, float y, float height, uint32_t color, char c);
float olDrawString(Font *fnt, float x, float y, float height, uint32_t color, const char *s);

#endif
