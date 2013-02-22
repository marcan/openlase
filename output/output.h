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

#ifndef OUTPUT_H
#define OUTPUT_H

#define ENABLE_X      0x01
#define ENABLE_Y      0x02
#define INVERT_X      0x04
#define INVERT_Y      0x08
#define SWAP_XY       0x10

#define OUTPUT_ENABLE 0x01
#define BLANK_ENABLE  0x02
#define BLANK_INVERT  0x04

#define MAX_DELAY 25

typedef struct {
	float power;
	float size;
	float redMax;
	float redMin;
	float redBlank;
	int redDelay;
	float greenMax;
	float greenMin;
	float greenBlank;
	int greenDelay;
	float blueMax;
	float blueMin;
	float blueBlank;
	int blueDelay;
	float transform[3][3];

	int scan_flags;
	int blank_flags;
	int safe;
} output_config_t;

#endif
