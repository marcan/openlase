#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <jack/jack.h>
#include <math.h>

#include <libmodplug/modplug.h>

#include "libol.h"
#include "text.h"
#include "ilda.h"

#include "trace.h"

Font *font;

float usin(float x, float r)
{
	float v = x / r * M_PI * 2.0f;
	return (sinf(v)+1.0f)/2.0f;
}

OLRenderParams params;

float gbl_time = 0;
int gbl_frames = 0;
float ftime = 0;
float audiotime;

#define BPM 135
#define AB (135.0/60.0)

void HSBToRGB(
    unsigned int inHue, unsigned int inSaturation, unsigned int inBrightness,
    unsigned int *oR, unsigned int *oG, unsigned int *oB )
{
    if (inSaturation == 0)
    {
        // achromatic (grey)
        *oR = *oG = *oB = inBrightness;
    }
    else
    {
        unsigned int scaledHue = (inHue * 6);
        unsigned int sector = scaledHue >> 8; // sector 0 to 5 around the color wheel
        unsigned int offsetInSector = scaledHue - (sector << 8);	// position within the sector
        unsigned int p = (inBrightness * ( 255 - inSaturation )) >> 8;
        unsigned int q = (inBrightness * ( 255 - ((inSaturation * offsetInSector) >> 8) )) >> 8;
        unsigned int t = (inBrightness * ( 255 - ((inSaturation * ( 255 - offsetInSector )) >> 8) )) >> 8;

        switch( sector ) {
        case 0:
            *oR = inBrightness;
            *oG = t;
            *oB = p;
            break;
        case 1:
            *oR = q;
            *oG = inBrightness;
            *oB = p;
            break;
        case 2:
            *oR = p;
            *oG = inBrightness;
            *oB = t;
            break;
        case 3:
            *oR = p;
            *oG = q;
            *oB = inBrightness;
            break;
        case 4:
            *oR = t;
            *oG = p;
            *oB = inBrightness;
            break;
        default:    // case 5:
            *oR = inBrightness;
            *oG = p;
            *oB = q;
            break;
        }
    }
}

float render(void)
{
	ftime = olRenderFrame(150);
	gbl_frames++;
	gbl_time += ftime;

	audiotime = gbl_time * AB;

	printf("Frame time: %f, FPS:%f, Avg FPS:%f, Audio: %f\n", ftime, 1/ftime, gbl_frames/gbl_time, audiotime);
	return ftime;
}

int passed(float time)
{
	return (((gbl_time - ftime) < time) && gbl_time >= time);
}

int apassed(float time)
{
	return passed(time/BPM*60.0);
}

void render_cubes(float time)
{
	int i;

	OLRenderParams mpar;

	memcpy(&mpar, &params, sizeof(OLRenderParams));

	if (time > 32) {
		time += 1.0;
		mpar.on_speed = 0.022 + (1-usin(time, 2.0)) * 0.02;
		mpar.corner_dwell = 8*usin(time, 2.0);
		mpar.start_dwell = 2+3*usin(time, 2.0);
		mpar.start_wait = 3+5*usin(time, 2.0);
		mpar.end_dwell = 2+3*usin(time, 2.0);
		mpar.end_wait = 2*usin(time, 2.0);
		olSetRenderParams(&mpar);
		time -= 1.0;
	}
	printf("%f %d %d %d %d %d\n", mpar.on_speed, mpar.corner_dwell, mpar.start_dwell, mpar.start_wait, mpar.end_dwell, mpar.end_wait);

	olLoadIdentity3();
	olPerspective(60, 1, 1, 100);
	olTranslate3(0, 0, -2.1);

	for(i=0; i<3; i++) {
		if (i>0)
			olPushMatrix3();
		olScale3(0.6, 0.6, 0.6);
		if (i>0) {
			float tx = sinf(time + (i-1)*M_PI);
			float ty = cosf(time + (i-1)*M_PI);
			float tz = sinf(time + (i-1)*M_PI);
			float s = sinf(0.6*time);
			olTranslate3(tx*s,ty*s,tz*s);
			//olScale3(s,s,s);
			olScale3(0.3,0.3,0.3);
		}

		float mult;
		if (i==0)
			mult = 1;
		else if (i==1)
			mult = 1.5;
		else if (i==2)
			mult = -1.5;

		if (i==0)
			olMultColor(C_GREY(120));
		else
			olResetColor();



		olRotate3Z(mult*time * M_PI * 0.1 / 3.0);
		olRotate3Y(mult*time * M_PI * 0.8 / 3.0);
		olRotate3X(mult*time * M_PI * 0.73 / 3.0);

		olBegin(OL_LINESTRIP);
		olVertex3(-1, -1, -1, C_RED);
		olVertex3( 1, -1, -1, C_RED);
		olVertex3( 1,  1, -1, C_RED);
		olVertex3(-1,  1, -1, C_RED);
		olVertex3(-1, -1, -1, C_RED);
		olVertex3(-1, -1,  1, C_RED);
		olEnd();

		olBegin(OL_LINESTRIP);
		olVertex3( 1,  1,  1, C_GREEN);
		olVertex3(-1,  1,  1, C_GREEN);
		olVertex3(-1, -1,  1, C_GREEN);
		olVertex3( 1, -1,  1, C_GREEN);
		olVertex3( 1,  1,  1, C_GREEN);
		olVertex3( 1,  1, -1, C_GREEN);
		olEnd();

		olBegin(OL_LINESTRIP);
		olVertex3( 1, -1, -1, C_RED);
		olVertex3( 1, -1,  1, C_RED);
		olEnd();

		olBegin(OL_LINESTRIP);
		olVertex3(-1,  1,  1, C_GREEN);
		olVertex3(-1,  1, -1, C_GREEN);
		olEnd();

		/*olBegin(OL_BEZIERSTRIP);
		olVertex3(-1, 1, 0, C_WHITE);
		olVertex3(0, -1, 0, C_WHITE);
		olVertex3(0, -1, 0, C_WHITE);
		olVertex3(1, 1, 0, C_WHITE);
		olVertex3(-1, 0, 0, C_WHITE);
		olVertex3(-1, 0, 0, C_WHITE);
		olVertex3(1, -1, 0, C_WHITE);
		olVertex3(0, 1, 0, C_WHITE);
		olVertex3(0, 1, 0, C_WHITE);
		olVertex3(-1, -1, 0, C_WHITE);
		olVertex3(1, 0, 0, C_WHITE);
		olVertex3(1, 0, 0, C_WHITE);
		olVertex3(-1, 1, 0, C_WHITE);
		olEnd();*/
		if (i>0)
			olPopMatrix3();
	}
	olLoadIdentity3();
	olLoadIdentity();
	olSetRenderParams(&params);
	olResetColor();
}

int mbuf[256][256];
uint8_t mtmp[256][256];

void draw_metaball(float x, float y, float radius)
{
	int cx,cy;
	float px,py;
	int *p = &mbuf[0][0];
	px = -x;
	py = -y;

	radius *= 400000.0f;

	for(cy=0; cy<256; cy++) {
		for(cx=0; cx<256;cx++) {
			float d = px*px+py*py;
			*p++ += radius/(d+1);
			px++;
		}
		py++;
		px = -x;
	}
}


int render_metaballs(float time, float ltime)
{
	int obj = 0;
	float dist1 = 128 + sinf(time * M_PI * 1.0 * 0.8 * 0.4 + 0) * 95;
	float dist2 = 128 + sinf(time * M_PI * 1.2 * 0.8 * 0.4 + 1) * 95;
	float dist3 = 130 + sinf(time * M_PI * 1.3 * 0.8 * 0.4 + 2) * 95;
	float dist4 = 100 + sinf(time * M_PI * 1.4 * 0.8 * 0.4 + 3) * 95;
	float dist5 = 128 + sinf(time * M_PI * 1.5 * 0.5 * 0.4 + 4) * 95;
	float dist6 = 128 + sinf(time * M_PI * 1.6 * 0.5 * 0.4 + 5) * 95;
	float dist7 = 130 + sinf(time * M_PI * 1.7 * 0.5 * 0.4 + 6) * 95;
	float dist8 = 100 + sinf(time * M_PI * 1.8 * 0.5 * 0.4 + 7) * 95;

	if (time < 1.0)
		dist1 -= 256-256*sinf((time) * M_PI / 2);
	if (time < 3.0)
		dist2 += 256-256*sinf((time-2.0) * M_PI / 2);
	if (time < 5.0) {
		dist7 -= 256-256*sinf((time-4.0) * M_PI / 2);
		dist3 += 256-256*sinf((time-4.0) * M_PI / 2);
	}
	if (time < 7.0) {
		dist8 -= 256-256*sinf((time-6.0) * M_PI / 2);
		dist4 -= 256-256*sinf((time-6.0) * M_PI / 2);
	}

	if (ltime < 7.0)
		dist1 -= 256-256*sinf((ltime-6.0) * M_PI / 2);
	if (ltime < 5.0)
		dist2 += 256-256*sinf((ltime-4.0) * M_PI / 2);
	if (ltime < 3.0) {
		dist7 -= 256-256*sinf((ltime-2.0) * M_PI / 2);
		dist3 += 256-256*sinf((ltime-2.0) * M_PI / 2);
	}
	if (ltime < 1.0) {
		dist8 -= 256-256*sinf((ltime) * M_PI / 2);
		dist4 -= 256-256*sinf((ltime) * M_PI / 2);
	}

	memset(mbuf, 0, sizeof mbuf);

	if (ltime > 6.0)
		draw_metaball(dist1, dist5, 45);
	if (time > 2.0 && ltime > 4.0)
		draw_metaball(dist2, dist6, 10);
	if (time > 4.0 && ltime > 2.0)
		draw_metaball(dist7, dist3, 30);
	if (time > 6.0)
		draw_metaball(dist8, dist4, 70);

	olPushMatrix();
	olTranslate(-1.0f, -1.0f);
	olScale(2.0f/256.0f, 2.0f/256.0f);

	//printf("%d %d\n", mbuf[0][0], mbuf[128][128]);

	obj = trace(&mbuf[0][0], &mtmp[0][0], 20000, 256, 256, 1);

	olPopMatrix();
	return obj;
}

void render_fire(float left)
{
	int x,y,i;
	int w = 256;
	int h = 256;

	for(i=0; i<2; i++) {
		int *p = &mbuf[h-1][0];

		for (x=0;x<256;x++) {
			if (left > 0.7 && (random() % 100) > 60)
				*p++ = 1024;
			else
				*p++ = 0;
		}

		if (left > 2.0 && random() % 32 == 0)
			mbuf[h-1][random()%w] = random()%120000;


		for (y=254;y>=0;y--) {
			p = &mbuf[y][0];
			*p++ = 0;
			for (x=1;x<255;x++) {
				*p = (*p + p[w-1] + p[w] + p[w+1]) / 4;
				if (*p)
					(*p)--;
				p++;
			}
			*p++ = 0;
		}
	}

	olPushMatrix();
	olScale(1, -1);
	olTranslate(-1.0f, -1.0f);
	olScale(2.0f/256.0f, 2.0f/256.0f);

	olPushColor();
	olMultColor(C_RED+C_GREEN_I(150));
	trace(&mbuf[0][0], &mtmp[0][0], 200, 256, 235, 2);
	olPopColor();
	olPushColor();
	olMultColor(C_RED+C_GREEN_I(100));
	trace(&mbuf[0][0], &mtmp[0][0], 250, 256, 235, 2);
	olPopColor();
	olPushColor();
	olMultColor(C_RED+C_GREEN_I(50));
	trace(&mbuf[0][0], &mtmp[0][0], 300, 256, 235, 2);
	olPopColor();
	olPushColor();
	olMultColor(C_RED+C_GREEN_I(0));
	trace(&mbuf[0][0], &mtmp[0][0], 400, 256, 235, 2);
	olPopColor();
	olPopMatrix();

}

void sinescroller(float *x, float *y, uint32_t *color)
{
	*y += 0.05*sinf(*x * 6);
}

void DoMetaballs(float limit)
{
	float ctime = 0;
	memset(mbuf, 0, sizeof mbuf);

	params.off_speed = 2.0/30.0;
	params.start_wait = 8;
	params.start_dwell = 2;
	params.end_dwell = 2;
	params.end_wait = 3;
	params.render_flags |= RENDER_NOREORDER;
	olSetRenderParams(&params);

	float xpos = 6;

	while(audiotime < limit)
	{
		params.start_dwell = 7;
		params.end_dwell = 7;
		olSetRenderParams(&params);
		render_metaballs(ctime * AB, limit-audiotime);

		params.start_dwell = 3;
		params.end_dwell = 3;
		olSetRenderParams(&params);


		olSetVertexShader(sinescroller);

		olDrawString(font, xpos, -0.5, 0.4, C_WHITE, "WARNING: Laser Radiation. Do not stare into beam with remaining eye!");

		olSetVertexShader(NULL);

		ctime += render();
		xpos -= ftime*1.2;
	}
}

void DoFire(float limit)
{
	float ctime = 0;
	memset(mbuf, 0, sizeof mbuf);

	while(audiotime < limit)
	{
		render_fire(limit - audiotime);
		ctime += render();
	}
}

void hfade(float *x, float *y, uint32_t *color)
{
	float wipe_col = 1.0f;

	if (*x < -0.8)
		wipe_col = 1+((*x+0.8)/0.2);
	if (*x > 0.8)
		wipe_col = 1-((*x-0.8)/0.2);

	if (wipe_col > 1.0f)
		wipe_col = 1.0f;
	if (wipe_col < 0.0f)
		wipe_col = 0.0f;

	uint8_t r,g,b;
	r = (uint8_t)(*color >> 16);
	g = (uint8_t)((*color&0xff00) >> 8);
	b = (uint8_t)(*color&0xff);

	r *= wipe_col;
	g *= wipe_col;
	b *= wipe_col;

	*color = (r<<16) | (g<<8) | b;
}

void DoCubes(float limit)
{
	float ctime = 0;

	params.on_speed = 2.0/90.0;
	params.off_speed = 2.0/30.0;
	params.start_wait = 8;
	params.start_dwell = 6;
	params.curve_dwell = 0;
	params.corner_dwell = 8;
	params.end_dwell = 6;
	params.end_wait = 2;
	params.render_flags |= RENDER_NOREORDER;
	olSetRenderParams(&params);

	float xpos = -2.0;

	while(audiotime < limit)
	{
		if (ctime*AB < 1.5)
			xpos = -2.0 +2*sinf(ctime*AB * M_PI * 0.5 / 1.5);
		else if (audiotime > (limit-1.5))
			xpos = 2-2*sinf((limit-audiotime) * M_PI * 0.5 / 1.5);
		else
			xpos = 0;

		olLoadIdentity();
		olTranslate(xpos, 0);
		render_cubes(ctime*AB);

		olSetPixelShader(hfade);

		ctime -= 15.0;

		olDrawString(font, -1.2*(ctime-1.0), 0.35, 0.3, C_WHITE, "Spontz");
		olDrawString(font, -1.5*(ctime-2.0), -0.60, 0.3, C_WHITE, "BiXo");
		olDrawString(font, -1.3*(ctime-3.0), 0.70, 0.3, C_WHITE, "Xplsv");
		olDrawString(font, -1.7*(ctime-4.0), 0.40, 0.3, C_WHITE, "Software Failure");
		olDrawString(font, -1.4*(ctime-5.0), -0.75, 0.3, C_WHITE, "Nocturns");
		olDrawString(font, -1.2*(ctime-6.0), -0.45, 0.3, C_WHITE, "Purple Studios");
		olDrawString(font, -1.7*(ctime-7.0), 0.35, 0.3, C_WHITE, "Masca Team");
		olDrawString(font, -1.2*(ctime-8.0), 0.60, 0.3, C_WHITE, "Glenz");
		olDrawString(font, -1.3*(ctime-9.0), -0.40, 0.3, C_WHITE, "Napalm Core");
		olDrawString(font, -1.5*(ctime-10.0), -0.75, 0.3, C_WHITE, "TDA");
		olDrawString(font, -1.3*(ctime-11.0), 0.45, 0.3, C_WHITE, "ASD");

		ctime += 15.0;

		olSetPixelShader(NULL);

		ctime += render();
	}
}

int points_left = 0;
int include_dark_points = 0;

void cutoff(float *x, float *y, uint32_t *color)
{
	static float save_x, save_y;
	static uint32_t save_color;
	static int points_dot = 0;
	if ((!include_dark_points) && (*color == C_BLACK)) {
		if (points_left)
			return;
		*x = save_x;
		*y = save_y;
	}
	if (points_left) {
		points_left--;
		save_x = *x;
		save_y = *y;
		save_color = *color;
		points_dot = 200;
	} else {
		*x = save_x;
		*y = save_y;
		if (points_dot) {
			*color = C_WHITE;
			points_dot--;
		} else {
			*color = C_BLACK;
		}
	}
}

int count_active_points(IldaFile *ild)
{
	IldaPoint *p = ild->points;
	int count = 0, i;
	for (i = 0; i < ild->count; i++) {
		if (include_dark_points || !p->is_blank)
			count++;
		p++;
	}
	return count;
}

float wipe_center;
float wipe_w;
int wipe_inv;

void hwipe(float *x, float *y, uint32_t *color)
{
	float wipe_col = (*x - wipe_center) / wipe_w;
	if (wipe_col > 1.0f)
		wipe_col = 1.0f;
	if (wipe_col < 0.0f)
		wipe_col = 0.0f;

	if (wipe_inv)
		wipe_col = 1.0f-wipe_col;

	uint8_t r,g,b;
	r = (uint8_t)(*color >> 16);
	g = (uint8_t)((*color&0xff00) >> 8);
	b = (uint8_t)(*color&0xff);

	r *= wipe_col;
	g *= wipe_col;
	b *= wipe_col;

	*color = (r<<16) | (g<<8) | b;
}



void DoEuskal(void)
{
	IldaFile *ild;
	ild = olLoadIlda("euskal18.ild");

	params.off_speed = 2.0/20.0;
	params.start_wait = 8;
	params.end_wait = 1;
	params.render_flags |= RENDER_NOREORDER;
	olSetRenderParams(&params);

	include_dark_points = 0;
	int count = count_active_points(ild);
	float cur_draw = 0;

	olSetVertexShader(cutoff);

	while(1) {
		int obj;
		float w;

		points_left = cur_draw;
		olDrawIlda(ild);
		render();

		cur_draw += ftime * count / 5.0;

		if (cur_draw > count) {
			break;
		}
	}

	olSetVertexShader(NULL);

	float bright = 300.0f;

	while(1) {
		uint32_t col;
		if (bright > 255.0f)
			col = C_WHITE;
		else
			col = C_GREY((int)bright);
		olPushColor();
		olMultColor(col);
		olDrawIlda(ild);
		olPopColor();

		render();

		bright -= ftime * 40.0;
		if (bright < 0)
			break;
	}

}

float randf(float max)
{
	return max * ((float)random() / (float)RAND_MAX);
}

void DoTitle(float limit)
{
	IldaFile *ild;
	ild = olLoadIlda("lase_title.ild");

	include_dark_points = 1;

	int count = count_active_points(ild);
	float cur_draw = 0;
	float ctime = 0;

	olSetVertexShader(cutoff);

	params.render_flags |= RENDER_NOREORDER;
	olSetRenderParams(&params);

	while(1) {
		int obj;
		float w;

		points_left = cur_draw;
		olDrawIlda(ild);
		ctime += render();

		cur_draw += ftime * count / 3.0;

		if (cur_draw > count) {
			break;
		}
	}

	olSetVertexShader(NULL);

	while(AB*ctime < 8) {
		olDrawIlda(ild);
		ctime += render();
	}

	wipe_center = -1.0;
	wipe_w = 0.4;
	wipe_inv = 1;

	const char *s="A realtime laser demo";

	float s_x = -olGetStringWidth(font, 0.2, s) / 2;
	float s_y = -0.5;
	float s_h = 0.2;

	while(1) {
		olDrawIlda(ild);
		olSetPixelShader(hwipe);
		olDrawString(font, s_x, s_y, s_h, C_WHITE, s);
		olSetPixelShader(NULL);
		ctime += render();
		wipe_center += 1.7*ftime;

		if(wipe_center > 1.0f)
			break;
	}

	float bright = 300.0f;
	while(audiotime < limit) {
		uint32_t col;
		if (bright > 255.0f)
			col = C_WHITE;
		else
			col = C_GREY((int)bright);
		olPushColor();
		olMultColor(col);
		olDrawIlda(ild);
		olDrawString(font, s_x, s_y, s_h, C_WHITE, s);
		olPopColor();
		render();

		bright -= ftime * 130.0;
		if (bright < 0)
			bright = 0;
	}

}

#define NUM_STARS 100

float stars[NUM_STARS][5];

void cart_rnd_star(int id)
{
	float x = randf(2.0)-1.0;
	float y = randf(2.0)-1.0;
	stars[id][0] = sqrtf(x*x+y*y);
	stars[id][1] = atan2(y,x);
}

void render_stars(void)
{
	int count = NUM_STARS;
	int i;
	for (i=0; i<count; i++) {
		float r = stars[i][0];
		float a = stars[i][1];
		float x = sinf(a)*r;
		float y = cosf(a)*r;

		unsigned int brightness=255;
                if ((stars[i][2]*stars[i][3]) < 20)
                        brightness = (int)(12.70*(stars[i][2]*stars[i][3]));

                unsigned int red,green,blue;
                HSBToRGB(stars[i][4], 255, brightness, &red, &green, &blue);
                uint32_t color = (blue) + (green<<8) + (red<<16);

		olDot(x,y,(int)stars[i][2],color);
	}
}

float star_deps[][2] = {
	{0x20,	20},
	{0x28,	20},
	{0x3c,	20},
	{0x3e,	20},
	{0x40,	20},
	{0x4d,	20},
	{0x50,	20},
	{0x58,	20},
	{0x60,	20},
	{0x64,	20},
	{0x68,	20},
	{0x70,	20},
	{0x74,	20},
	{0x78,	20},
	{0x7a,	30},
	{0x7c,	40},
	{0x80,	50},
	{0x84,	50},
	{0x89,	50},
	{0x8c,	50},
	{0x90,	50},
	{0x96,	50},
	{0x98,	50},
	{0x9a,	50},
	{0xa0,	50},
	{0xa4,	50},
	{0xa9,	50},
	{0xac,	50},
	{0xb0,	50},
	{0xb6,	50},
	{0xba,	50},
	{0xbd,	50},
/*	{0xc0,	50},
	{0xc4,	50},
	{0xc9,	50},
	{0xcc,	50},
	{0xd0,	50},
	{0xd6,	50},
	{0xd8,	50},
	{0xda,	50},
	{0xe0,	50},
	{0xe4,	40},
	{0xe9,	40},
	{0xec,	30},
	{0xf0,	30},
	{0xf6,	30},
	{0xfa,	20},
	{0xfd,	20},*/
};

#define STAR_ST1 0x30
#define STAR_ST2 0x40

#define NUM_PRESETS (sizeof(star_deps)/sizeof(star_deps[0]))

void DoStars(float limit)
{
	int i;
	float ctime = 0;
	float lctime = 0;

	params.start_wait = 7;
	params.off_speed = 2.0/20.0;
	params.end_wait = 3;
	params.render_flags &= ~RENDER_NOREORDER;
	olSetRenderParams(&params);

	for (i=0; i<NUM_STARS; i++) {
		stars[i][0] = randf(1.5);
		stars[i][1] = randf(2*M_PI);
		stars[i][2] = 0;
		stars[i][3] = 0;
		stars[i][4] = randf(255);

	}

	ftime = 0;

	int id = 0;

	while (!apassed(STAR_ST1)) {
		for (i=0; i<NUM_PRESETS; i++) {
			if(apassed(star_deps[i][0]/4.0f)) {
				printf("%d\n", i);
				cart_rnd_star(id);
				stars[id][2] = star_deps[i][1];
				stars[id][3] = 1;
				id = (id+1)%NUM_STARS;
			}
		}
		render_stars();
		ctime += render();
		for (i=0; i<NUM_STARS; i++) {
			stars[i][3] -= ftime*0.3;
			if (stars[i][3] < 0)
				stars[i][3] = 0;
		}
	}

	int ct = 0;
	int max;
	ctime = 0;

	while (!apassed(STAR_ST2)) {
		max = ctime * 100.0 / 0x10;
		while(ct < max) {
			stars[id][0] = 0.05 + randf(1.50);
			stars[id][1] = randf(2*M_PI);
			stars[id][2] = randf(20);
			stars[id][3] = 1;
			id = (id+1)%NUM_STARS;
			ct++;
		}
		render_stars();
		ctime += render()*AB;
	}

	float speed = 0.3;

	params.start_wait = 6;
	params.end_wait = 1;
	olSetRenderParams(&params);

	while (audiotime < limit) {
		render_stars();
		for (i=0; i<NUM_STARS; i++) {
			stars[i][0] *= (1+speed*ftime);
			stars[i][3] += speed*ftime;
			if (stars[i][3] > 1.0)
				stars[i][3] = 1.0;
			if (stars[i][0] > 1.5 && audiotime < (limit-2.2*AB)) {
				stars[i][0] = 0.05 + randf(0.1);
				stars[i][1] = randf(2*M_PI);
				stars[i][2] = randf(20);
				stars[i][3] = 0;
			}
		}

		ctime += render();
		if (speed < 2) {
			speed += 1.2f * ftime;
			if (speed > 2)
				speed = 2;
		}
	}

}

void tunnel_xform(float z)
{
	float theta = 0.3*z * M_PI;
	float r = 0.2;

	olTranslate3(r*sinf(theta),r*cosf(theta),0);
}

void tunnel_revxform(float z)
{
	float theta = 0.3*z * M_PI;
	float r = 0.2;

	olTranslate3(-r*sinf(theta),-r*cosf(theta),0);

	theta = -0.17*z * M_PI;
	r = 0.2;

	olTranslate3(-r*sinf(theta),-r*cosf(theta),0);

}


void DoTunnel(float limit)
{
	params.on_speed = 2.0/100.0;
	params.start_dwell = 2;
	params.curve_dwell = 0;
	params.corner_dwell = 2;
	params.curve_angle = cosf(30.0*(M_PI/180.0)); // 30 deg
	params.end_dwell = 2;
	params.snap = 1/100000.0;
	params.flatness = 0.000005;
	params.start_wait = 6;
	params.off_speed = 2.0/30.0;
	params.end_wait = 3;
	params.render_flags &= ~RENDER_NOREORDER;
	olSetRenderParams(&params);

	float ctime = 0;

	int i,j;

	olLoadIdentity();

	float z = 0.0f;
	float rz = 0.0f;

	float dz=1.2;

	int id=0;

	while (audiotime < limit) {
		float left = (limit-audiotime)/AB;
		olResetColor();
		if (ctime < 2.0)
			olMultColor(C_RED+C_GREEN_I((int)(255*ctime/2)));
		else if (left < 2.0)
			olMultColor(C_BLUE+C_RED_I((int)(255*left/2)));

		olLoadIdentity3();
		olPerspective(45, 1, 1, 100);

		while(z > dz) {
			z -= dz;
			id++;
		}

		olScale3(0.6, 0.6, 1.0);
		olTranslate3(0, 0, 1.5);
		olTranslate3(0, 0, -z);
		tunnel_revxform(rz);

		for(i=0;i<10;i++) {
			if ((id+i) > 5) {
				olPushMatrix3();

				olTranslate3(0,0,dz*i);

				tunnel_xform(rz+dz*(i+id));
				olBegin(OL_LINESTRIP);

				for(j=0;j<11;j++) {
					float theta = j/5.0*M_PI;
					uint32_t c = C_RED;
					if(i==9) {
						c = C_RED+C_BLUE_I((int)(255 * z/dz));
					}
					olVertex3(sinf(theta), cosf(theta), 0, c);
					//olVertex3(j/11.0,0,0,C_WHITE);
				}
				olEnd();

				olPopMatrix3();
			}
		}

		for(j=0;j<10;j++) {
			float theta = j/5.0*M_PI;
			olBegin(OL_LINESTRIP);
			for(i=0;i<9;i++) {
				if ((id+i) > 5) {
					olPushMatrix3();
					olTranslate3(0,0,dz*i);
					tunnel_xform(rz+dz*(i+id));
					olVertex3(sinf(theta), cosf(theta), 0, C_GREEN);
					olPopMatrix3();
				}
			}
			olEnd();
		}


		ctime += render();
		z += ftime*3.2;
		rz += ftime*3.2;

	}
}

ModPlugFile *module;

int16_t mod_smpbuf[2*48000];

/*
static inline float cubic_interp(float sp, float s0, float s1, float s2, float pos)
{
    float x = freqAcc/(float)(1 << DSOUND_FREQSHIFT);

    float b = .5*(s1+sp)-s0, a = (1/6.)*(s2-s1+sp-s0-4*b);
    float c = s1-s0-a-b;
    float out = ((a*x + b)*x + c)*x + s0;

    return out;
}*/

void gen_audio(float *left, float *right, int count)
{
	int read;
	int16_t *p;
	memset(left, 0, sizeof(float)*count);
	memset(right, 0, sizeof(float)*count);
	read = ModPlug_Read(module, mod_smpbuf, count*4);
	read /= 4;
	p = mod_smpbuf;
	while(read--) {
		*left++ = *p++ / 32768.0;
		*right++ = *p++ / 32768.0;
	}
}

int main (int argc, char *argv[])
{

	void *mod_data;
	FILE *mod_fd;
	size_t mod_size;

	mod_fd = fopen("GLOS-pope.xm","r");
	fseek(mod_fd, 0, SEEK_END);
	mod_size = ftell(mod_fd);
	fseek(mod_fd, 0, SEEK_SET);
	mod_data = malloc(mod_size);
	fread(mod_data, 1, mod_size, mod_fd);
	fclose(mod_fd);

	ModPlug_Settings cfg;
	ModPlug_GetSettings(&cfg);
	cfg.mChannels = 2;
	cfg.mBits = 16;
	cfg.mFrequency = 48000;
	cfg.mResamplingMode = MODPLUG_RESAMPLE_SPLINE;
	cfg.mFlags = MODPLUG_ENABLE_OVERSAMPLING;
	ModPlug_SetSettings(&cfg);

	module = ModPlug_Load(mod_data, mod_size);

	srandom(0);

	memset(&params, 0, sizeof params);
	params.rate = 48000;
	params.on_speed = 2.0/100.0;
	params.off_speed = 2.0/30.0;
	params.start_wait = 8;
	params.start_dwell = 2;
	params.curve_dwell = 0;
	params.corner_dwell = 2;
	params.curve_angle = cosf(30.0*(M_PI/180.0)); // 30 deg
	params.end_dwell = 2;
	params.end_wait = 1;
	params.snap = 1/100000.0;
	params.flatness = 0.000005;
	params.max_framelen = params.rate / 24;
	params.render_flags = RENDER_GRAYSCALE;

	if(olInit(10, 30000) < 0)
		return 1;
	olSetRenderParams(&params);

	olSetAudioCallback(gen_audio);

	float time = 0;
	float ftime;
	int i,j;

	int frames = 0;

	memset(mbuf, 0, sizeof mbuf);

	font = olGetDefaultFont();

	float xpos = 1.0;

	DoStars(96);
	DoTitle(111);

	DoMetaballs(143);
	DoFire(174);

	DoTunnel(175+32);
	DoCubes(175+32+64);


	DoEuskal();

#if 0

	while(1) {
		int obj;
		float w;

		points_left = cur_draw;
		olPushColor();
		//olMultColor(C_GREY((int)(255.0f * cur_draw / count)));
		olSetVertexShader(cutoff);
		olDrawIlda(ild);
		olSetVertexShader(NULL);
		olPopColor();
/*
		olSetVertexShader(sinescroller);

		w = olDrawString(font, xpos, 0.35, 0.4, C_WHITE, "Hello, World! This is a test message displayed using the OpenLase text functions. Sine scrollers FTW!");
		if (xpos < (-w-1.5))
			xpos = 1.0;

		olSetVertexShader(NULL);*/

/*
		olDrawString(font, -1, 0.35, 0.4, C_WHITE, "Hello, World!");
		olDrawString(font, -1, -0, 0.4, C_WHITE, "How are you?");
		olDrawString(font, -1, -0.35, 0.4, C_WHITE, "    (-;   :-)");*/

		//render_cubes(time);
		//render_metaballs(time);
		//ender_fire();
		/*
		olBegin(OL_BEZIERSTRIP);
		olVertex(0,1,C_WHITE);
		olVertex(1,1,C_WHITE);
		olVertex(1,1,C_WHITE);
		olVertex(1,0,C_WHITE);

		olVertex(1,-1,C_WHITE);
		olVertex(1,-1,C_WHITE);
		olVertex(0,-1,C_WHITE);
		olEnd();*/

		ftime = olRenderFrame(150);
		frames++;
		gbl_time += ftime;
		xpos -= ftime;
		cur_draw += ftime * count / 5.0;
		if (cur_draw > count)
			cur_draw = count;
		printf("Frame time: %f, FPS:%f\n", ftime, frames/time);
	}

#endif

	olShutdown();
	exit (0);
}

