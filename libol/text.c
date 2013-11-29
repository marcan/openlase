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

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_HERSHEYFONT
# include <hersheyfont.h>
#endif
#include "libol.h"
#include "text.h"

extern Font default_font;

Font *olGetDefaultFont(void)
{
	return &default_font;
}

void olFreeFont(Font *font)
{
	if ( !font || (font == &default_font) )
		return;

#ifdef HAVE_HERSHEYFONT
	if ( font->font_type == OL_FONT_HERSHEY ) {
		struct hershey_font *hf = font->font_data;
		hershey_font_free(hf);
	}
#endif

	free(font);
}

#ifdef HAVE_HERSHEYFONT
static Font *olGetFont_Hershey(const char *fontname)
{
	struct hershey_font *hf;
	Font *font;

	font = malloc(sizeof(Font));
	if ( !font )
		return 0;

	hf = hershey_font_load(fontname);
	if ( !hf ) {
		free(font);
		return 0;
	}

	font->font_type = OL_FONT_HERSHEY;
	font->font_data = hf;
	font->height = 32;		// Hershey fonts are fixed height (?)

	return font;
}
#endif

Font *olGetFont(olFontType font_type, const char *fontname)
{
	if ( font_type == OL_FONT_HERSHEY ) {
#ifdef HAVE_HERSHEYFONT
		return olGetFont_Hershey(fontname);
#else
		errno = ENOTSUP;
		return 0;
#endif
	}

	return olGetDefaultFont();
}

float olGetCharWidth(Font *font, float height, char c)
{
	if (!font)
		return 0;
	float ratio = height / font->height;

#ifdef HAVE_HERSHEYFONT
	if ( font->font_type == OL_FONT_HERSHEY ) {
		struct hershey_font *hf = font->font_data;
		return hf->glyphs[(uint8_t)c].width * ratio;
	}
#endif

	return font->chars[(uint8_t)c].width * ratio;
}

float olGetCharOverlap(Font *font, float height)
{
	if (!font)
		return 0;
	float ratio = height / font->height;
	return font->overlap * ratio;
}

static float olDrawChar_Default(Font *font, float x, float y, float height, uint32_t color, char c)
{
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
		return chr->width * ratio;
	} else {
		return font->overlap * ratio;
	}

	return chr->width * ratio;
}

#ifdef HAVE_HERSHEYFONT
static float olDrawChar_Hershey(Font *font, float x, float y, float height, uint32_t color, char c)
{
	struct hershey_font *hf = font->font_data;
	struct hershey_glyph *hg = hershey_font_glyph(hf, (uint8_t)c);
	if ( hg->width == 0 )
	    return 0;

	float ratio = height / font->height;

	struct hershey_path *hp;
	for ( hp=hg->paths; hp; hp=hp->next ) {
	    olBegin(OL_LINESTRIP);
	    int i;
	    for ( i=0; i<hp->nverts; i++ ) {
		short px = hp->verts[i].x;
		short py = hp->verts[i].y;
		olVertex3(x + px*ratio, y - height + py*ratio, 0, color);
	    }
	    olEnd();
	}

	return hg->width * ratio;
}
#endif

float olDrawChar(Font *font, float x, float y, float height, uint32_t color, char c)
{
	if (!font)
		return 0;

#ifdef HAVE_HERSHEYFONT
	if ( font->font_type == OL_FONT_HERSHEY )
		return olDrawChar_Hershey(font, x, y, height, color, c);
#endif

	return olDrawChar_Default(font, x, y, height, color, c);
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

	if (!font)
		return 0;

	while(*s) {
		w += olGetCharWidth(font, height, *s) - font->overlap * ratio;
		s++;
	}

	return w + font->overlap * ratio;
}


