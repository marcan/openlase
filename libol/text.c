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

#include <math.h>
#include "libol.h"
#include "text.h"

extern Font default_font;

Font *olGetDefaultFont(void)
{
	return &default_font;
}

float olGetCharWidth(Font *font, float height, char c)
{
	if (!font)
		return 0;
	float ratio = height / font->height;
	return font->chars[(uint8_t)c].width * ratio;
}

float olGetCharOverlap(Font *font, float height)
{
	if (!font)
		return 0;
	float ratio = height / font->height;
	return font->overlap * ratio;
}

float olDrawChar(Font *font, float x, float y, float height, uint32_t color, char c)
{
	if (!font)
		return 0;

	const FontChar *chr = &font->chars[(uint8_t)c];
	const FontPoint *p = chr->points;

	float ratio = height / font->height;

	if (p) {

		olBegin(OL_BEZIERSTRIP);

		do {
			olVertex3(x + p->x * ratio, y - p->y * ratio, 0, color);
			if (p->flag == 1) {
				olEnd();
				olBegin(OL_BEZIERSTRIP);
			}
		} while ((p++)->flag != 2);

		olEnd();
	}

	return chr->width * ratio;
}

float olDrawString(Font *font, float x, float y, float height, uint32_t color, const char *s)
{
	float w = 0;
	float ratio = height / font->height;

	while(*s) {
		w += olDrawChar(font, x+w, y, height, color, *s) - font->overlap * ratio;
		s++;
	}

	return w + font->overlap * ratio;
}


float olGetStringWidth(Font *font, float height, const char *s)
{
	float w = 0;
	float ratio = height / font->height;

	while(*s) {
		w += font->chars[(uint8_t)*s].width * ratio - font->overlap * ratio;
		s++;
	}

	return w + font->overlap * ratio;
}


