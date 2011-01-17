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
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* pong demo */

#define TOP 0.25
#define BOTTOM 0.85
#define LEFT 0.07
#define RIGHT 0.93
#define WIDTH (RIGHT-LEFT)
#define HEIGHT (BOTTOM-TOP)
#define PH 0.12
#define PW 0.03
#define BW 0.02
#define BH 0.02

static float p1,p2,bx,by,bdx,bdy;
int score1, score2;
int adv;

static float ltime = 0;

#define BDX 0.75
#define BDY 0.4

void pong_init(void **ctx, void *arg, OLRenderParams *params)
{
	params->render_flags |= RENDER_NOREVERSE;

	p1 = (HEIGHT-PH)/2;
	p2 = (HEIGHT-PH)/2;
	bx = 0;
	by = HEIGHT/2;
	bdx = BDX;
	bdy = BDY;

	score1 = 0;
	score2 = 0;
	adv = random()%2;

	ltime = -1;
}

void pong_deinit(void *ctx)
{
}

void digit(float x, float y, int d, uint32_t color)
{
}

void pong_render(void *ctx, float time)
{
	float ftime = time - ltime;

	if (ltime == -1)
		ftime = 0;
	ltime = time;
	
	bx += ftime*bdx;
	by += ftime*bdy;

	if (by > HEIGHT - BH) {
		bdy = -bdy;
		by += 2*ftime*bdy;
	} else if (by < 0) {
		bdy = -bdy;
		by += 2*ftime*bdy;
	}

	float r1 = ((random()%100-50) / 10000.0);
	float r2 = ((random()%100-50) / 10000.0);

	float p1fac = powf((1-bx/WIDTH),2) * (adv?0.17:0.22) + 0.03 + r1;
	float p2fac = powf(bx/WIDTH,2) * (adv?0.22:0.17) + 0.03 + r2;

	p1 = p1 * (1-p1fac) + (by-PH/2) * p1fac;
	p2 = p2 * (1-p2fac) + (by-PH/2) * p2fac;

	if (p1 < 0)
		p1 = 0;
	if (p1 > HEIGHT-PH)
		p1 = HEIGHT-PH;

	if (p2 < 0)
		p2 = 0;
	if (p2 > HEIGHT-PH)
		p2 = HEIGHT-PH;

	olLog("%f %10f %10f %10f %10f %10f %10f\n", p1, p2, r1, r2, p1fac, p2fac, bx);
	
	if (bx <= 0) {
		if (by < p1-BH || by > p1+PH) {
			if (bx < -BW) {
				by = p2 + PH/2 - BH/2;
				bx = WIDTH - BW;
				bdx = -BDX;
				bdy = BDY;
				score2++;
				adv = random()%2;
			}
		} else {
			bdx = -bdx;
			bx += 2*ftime*bdx;
		}
	} else if (bx > WIDTH - BW) {
		if (by < p2-BH || by > p2+PH) {
			if (bx > WIDTH) {
				by = p1 + PH/2 - BH/2;
				bx = 0;
				bdx = BDX;
				bdy = BDY;
				score1++;
				adv = random()%2;
			}
		} else {
			bdx = -bdx;
			bx += 2*ftime*bdx;
		}
	}

	bdx *= powf(1.2, ftime);
	bdy *= powf(1.2, ftime);

	

	olLoadIdentity();

	olTranslate(-1,1);
	olScale(2,-2);
	// window is now 0.0-1.0 X and Y, Y going down)
	olRect(0, TOP, 1, BOTTOM, C_WHITE);
	olRect(LEFT-PW, p1+TOP, LEFT, p1+TOP+PH, C_WHITE);
	olRect(RIGHT, p2+TOP, RIGHT+PW, p2+TOP+PH, C_WHITE);
	olRect(LEFT+bx, TOP+by, LEFT+bx+BW, TOP+by+BW, C_WHITE);
	olLine((LEFT+RIGHT)/2, TOP, (LEFT+RIGHT)/2, BOTTOM, C_GREY(70));

	olTranslate(0, TOP - 0.13);
	olScale(1, -1);
	
	char buf[10];
	sprintf(buf, "%d", score1);
	olDrawString(olGetDefaultFont(), LEFT, 0, 0.15, C_WHITE, buf);

	sprintf(buf, "%d", score2);
	float sw = olGetStringWidth(olGetDefaultFont(), 0.15, buf);
	olDrawString(olGetDefaultFont(), RIGHT-sw, 0, 0.15, C_WHITE, buf);

	//float sw = olGetS

	/*
	if (score1 >= 100)
		digit(0,0,score1/100,C_WHITE);
	if (score1 >= 10)
		digit(DIGW,0,score1/10%10,C_WHITE);
	digit(2*DIGW,0,score1%10,C_WHITE);

	if (score2 >= 100)
		digit(1-3*DIGW,0,score2/100,C_WHITE);
	if (score2 >= 10)
		digit(1-2*DIGW,0,score2/10%10,C_WHITE);
	digit(1-1*DIGW,0,score2%10,C_WHITE);*/
}
