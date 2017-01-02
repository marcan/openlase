/*
        OpenLase - a realtime laser graphics toolkit

Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 2.1 or version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef OUTPUT_H
#define OUTPUT_H

#define ENABLE_X      0x01
#define ENABLE_Y      0x02
#define INVERT_X      0x04
#define INVERT_Y      0x08
#define SWAP_XY       0x10
#define SAFE          0x20
#define FILTER_C      0x40
#define FILTER_D      0x80

#define OUTPUT_ENABLE 0x01
#define BLANK_ENABLE  0x02
#define BLANK_INVERT  0x04

#define COLORMODE_ANALOG	0
#define COLORMODE_TTL		1
#define COLORMODE_MODULATED	2

#define COLOR_RED 0x1
#define COLOR_GREEN 0x2
#define COLOR_BLUE 0x4
#define COLOR_MONOCHROME 0x08

#define MAX_DELAY 25

typedef struct {
	int power;
	int scanSize;

	int redMax;
	int redMin;
	int redBlank;
	int redDelay;

	int greenMax;
	int greenMin;
	int greenBlank;
	int greenDelay;

	int blueMax;
	int blueMin;
	int blueBlank;
	int blueDelay;

	float transform[3][3];

	int cLimit;
	int cRatio;
	int dPower;
	int dRatio;

	int colorMode;
	int scan_flags;
	int blank_flags;
    int color_flags;
} output_config_t;

#endif
