/* FCE Ultra - NES/Famicom Emulator - Emscripten video/window
 *
 * Copyright notice for this file:
 *  Copyright (C) 2015 Valtteri "tsone" Heikkila
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "em.h"
#include "ntsc.h"
#include <emscripten.h>
#include <math.h>


// TODO: tsone: from ntsc.c, please include from there etc.
// Half-width of Y and C box filter kernels.
#define YW2	6.0
#define CW2	12.0

extern uint64 FCEUD_GetTime();


// Canvas size scaler, ex. 1x, 2x
#define CANVAS_SCALER 2
#define CANVAS_W (CANVAS_SCALER * 256)
#define CANVAS_H (CANVAS_SCALER * em_scanlines)


static uint32 *s_lookupRGBA = 0;
static uint32 *s_pxs = 0;


static void Canvas2D_RecalcLookup()
{
	const double *yiqs = ntscGetLookup();
	for (int color = 0; color < NUM_COLORS; ++color) {
		double rgb[3];
		const int k = 3 * (color*LOOKUP_W + LOOKUP_W-1);

		ntscYIQ2RGB(rgb, &yiqs[k]);

		s_lookupRGBA[color] =
			   (int) (255.0*rgb[0] + 0.5)
			| ((int) (255.0*rgb[1] + 0.5) << 8)
			| ((int) (255.0*rgb[2] + 0.5) << 16)
			| 0xFF000000;
	}
}

static void ConvertNTSCScan2(uint32 *dst, const uint8 *row_pixels, int deemp,
	int row, const double *yiqs, uint8 overscan_color, double sharpness)
{
	/*
	To calculate a *single* rgb sample in the scanline:

	pixel:                  0 1 2 3 4 5 6 7 8
	phase:                  0 1 2 0 1 2 0 1 2
	                        v | |
	rgb00 (kernel0, subp0): 0 1 2 3 4
	rgb01 (kernel0, subp2):  0 1 2 3 4
	=> rgb0 = (rgb00+rgb01)/2 | |
	                          v |
	rgb10 (kernel1, subp0):   0 1 2 3 4
	rgb11 (kernel1, subp2):    0 1 2 3 4
	=> rgb1 = (rgb10+rgb11)/2   |
	                            v
	rgb20 (kernel2, subp0):     0 1 2 3 4
	rgb21 (kernel2, subp2):      0 1 2 3 4
	=> rgb2 = (rgb20+rgb21)/2

	Final result is Laplacian sharpen of the above:
	=> rgb = (1+2*f)*rgb1 - f * (rgb0+rgb2)
	*/

	const double f1 = -sharpness;
	const double f0 = 1.0 + 2.0*sharpness;

#define LK_N 8 // must be power of two and >= NUM_TAPS+2*2
	int lk[LK_N];
	int lk_top = 0;
#define RGB_NUM 8
	double rgb[RGB_NUM][2*3];
	int rgb_i2 = 0;
	int m = 0;

	int phase = ((-row)%3 + (256*NUM_PHASES-NUM_TAPS/2-2)) % NUM_PHASES;

	for (int x = -NUM_TAPS/2; x < NUM_TAPS/2; ++x) {
		int idx = (x < 0) ? overscan_color : row_pixels[x];
		idx += deemp;
		lk[lk_top] = idx*(3*LOOKUP_W) + phase*(3*NUM_SUBPS*NUM_TAPS);
		lk_top = (lk_top+1) & (LK_N-1);
		phase = (phase+1) % NUM_PHASES;
	}

	for (int x = -1; x <= INPUT_W; ++x) {
		int lk_i = (lk_top + (LK_N-NUM_TAPS)) & (LK_N-1);
		double yiq[2*3] = { 0.0, 0.0, 0.0,  0.0, 0.0, 0.0 };

		for (int tap3 = 0; tap3 < (3*NUM_TAPS); tap3 += 3) {
			int k0 = lk[lk_i] + tap3;   // kernel 0, subp 0
			int k2 = k0 + (2*3*NUM_TAPS); // kernel 0, subp 2

			yiq[0] += yiqs[k0  ];
			yiq[1] += yiqs[k0+1];
			yiq[2] += yiqs[k0+2];
			yiq[3] += yiqs[k2  ];
			yiq[4] += yiqs[k2+1];
			yiq[5] += yiqs[k2+2];

			lk_i = (lk_i+1) & (LK_N-1);
		}

		yiq[0] *= (8.0/2.0) / (double) YW2;
		yiq[1] *= (8.0/2.0) / (double) (CW2-2.0);
		yiq[2] *= (8.0/2.0) / (double) (CW2-2.0);
		yiq[3] *= (8.0/2.0) / (double) YW2;
		yiq[4] *= (8.0/2.0) / (double) (CW2-2.0);
		yiq[5] *= (8.0/2.0) / (double) (CW2-2.0);

		ntscYIQ2RGB(&rgb[rgb_i2][0], &yiq[0]);
		ntscYIQ2RGB(&rgb[rgb_i2][3], &yiq[3]);

		if (x > 0) {
			double rgbd[2*3];
			const int rgb_i0 = (rgb_i2 + (RGB_NUM-2)) & (RGB_NUM-1);
			const int rgb_i1 = (rgb_i2 + (RGB_NUM-1)) & (RGB_NUM-1);

			for(int i = 0; i < (2*3); ++i){
				double v = f0*rgb[rgb_i1][i] + f1*(rgb[rgb_i0][i]+rgb[rgb_i2][i]);
				if(v > 1.0) rgbd[i] = 1.0;
				else if(v < 0.0) rgbd[i] = 0.0;
				else rgbd[i] = v;
			}

			int c0 = (int) (255.0*rgbd[0] + 0.5)
				| ((int) (255.0*rgbd[1] + 0.5) << 8)
				| ((int) (255.0*rgbd[2] + 0.5) << 16)
				| 0xFF000000;
			int c1 = (int) (255.0*rgbd[3] + 0.5)
				| ((int) (255.0*rgbd[4] + 0.5) << 8)
				| ((int) (255.0*rgbd[5] + 0.5) << 16)
				| 0xFF000000;

			dst[m  ] = c0;
			dst[m+1] = c1;
			m += 2;
		}

		int idx = (x + (NUM_TAPS/2+1) >= INPUT_W) ? overscan_color
			: row_pixels[x + (NUM_TAPS/2+1)];
		lk[lk_top] = (idx+deemp) * (3*LOOKUP_W) + phase*(3*NUM_SUBPS*NUM_TAPS);
		lk_top = (lk_top+1) & (LK_N-1);

		rgb_i2 = (rgb_i2+1) & (RGB_NUM-1);

		phase = (phase+1) % NUM_PHASES;
	}
}

#if FCEM_DEBUG == 1
static void print_fps()
{
	static int frame_count = 0;
	static uint64 prev_time = -1;
	uint64 curr_time = FCEUD_GetTime();
	if (prev_time == -1) {
		prev_time = curr_time;
	}
	if (curr_time >= prev_time + 1000) {
		printf("!!!! %.2f fps\n", 1000 * frame_count / (double) (curr_time - prev_time));
		prev_time = curr_time;
		frame_count = 0;
	}
	++frame_count;
}
#endif

static void ConvertNTSC2(const uint8 *pixels, const uint8* row_deemp, uint8 overscan_color)
{
#if FCEM_DEBUG == 1
	print_fps();
#endif

	int row_offs = (INPUT_H - em_scanlines) / 2;
	// TODO: pull out, calculated in multiple places
	double sharpness = 0.4 * (Config_GetValue(FCEM_SHARPNESS)+0.5);

	const double *yiqs = ntscGetLookup();

	pixels += INPUT_W * row_offs;
	uint32 *dst = s_pxs;
	for (int row = 0; row < em_scanlines; ++row) {

		ConvertNTSCScan2(dst, pixels, row_deemp[row+row_offs] << 1, row, yiqs, overscan_color, sharpness);
		memcpy(dst + CANVAS_W, dst, sizeof(uint32) * CANVAS_W);
		dst += CANVAS_W << 1;
		pixels += INPUT_W;
	}
}

#if 0
static void ConvertDirect1(uint32 *dst, const uint8 *pixels, const uint8* row_deemp)
{
	int row_offs = (INPUT_H - em_scanlines) / 2;
	int k = INPUT_W * row_offs;
	int m = 0;

	for (int row = row_offs; row < em_scanlines + row_offs; ++row) {
		int deemp = row_deemp[row] << 1;

		for (int x = INPUT_W; x != 0; --x) {
			s_pxs[m] = s_lookupRGBA[pixels[k] + deemp];
			++m;
			++k;
		}
	}
}
#endif

static void ConvertDirect2(const uint8 *pixels, const uint8* row_deemp)
{
	int row_offs = (INPUT_H - em_scanlines) / 2;
	int k = INPUT_W * row_offs;
	int m = 0;

	for (int row = row_offs; row < em_scanlines + row_offs; ++row) {
		int deemp = row_deemp[row] << 1;

		for (int x = INPUT_W; x != 0; --x) {
			uint32 c = s_lookupRGBA[pixels[k] + deemp];
			s_pxs[m] = s_pxs[m+1] = s_pxs[m+CANVAS_W] = s_pxs[m+CANVAS_W+1] = c;
			m += 2;
			++k;
		}
		m += CANVAS_W;
	}
}

void Canvas2D_Render(uint8 *pixels, uint8* row_deemp, uint8 overscan_color)
{
	//if(!s_pxs){
	//	puts("!!!! WARNING: no s_tbpb");
	//	return;
	//}

	//uint64 t = FCEUD_GetTime();
	//while(FCEUD_GetTime() - t < 72) ;

#if CANVAS_SCALER == 2
	if (Config_GetValue(FCEM_NTSC_EMU)) {
  		ConvertNTSC2(pixels, row_deemp, overscan_color);
	} else {
		ConvertDirect2(pixels, row_deemp);
	}
#else
	#error CANVAS_SCALER == 2 is only supported.
#endif

	EM_ASM_ARGS({
		var src = $0;
		var data = FCEM.image.data;
		if ((typeof CanvasPixelArray === 'undefined') || !(data instanceof CanvasPixelArray)) {
			if (FCEM.prevData !== data) {
				FCEM.data32 = new Int32Array(data.buffer);
				FCEM.prevData = data;
			}
			FCEM.data32.set(HEAP32.subarray(src, src + FCEM.data32.length));
		} else {
			// ImageData is CanvasPixelArray which doesn't have buffers.
			var dst = -1;
			var val;
			var num = data.length + 1;
			while (--num) {
				val = HEAP32[src++];
				data[++dst] = val & 0xFF;
				data[++dst] = (val >> 8) & 0xFF;
				data[++dst] = (val >> 16) & 0xFF;
				data[++dst] = 0xFF;
			}
		}

		Module.ctx2D.putImageData(FCEM.image, 0, 0);
	}, (ptrdiff_t) s_pxs >> 2);
}

void Canvas2D_Init()
{
	s_lookupRGBA = (uint32*) malloc(sizeof(uint32) * NUM_COLORS);
	Canvas2D_RecalcLookup();

	EM_ASM_ARGS({
		var canvas = Module.canvas;
		canvas.width = canvas.widthNative = $0;
		canvas.height = canvas.heightNative = $1;
		Module.ctx = Module.createContext(canvas, false, true);
		FCEM.image = Module.ctx.getImageData(0, 0, $0, $1);
		Module.canvas2D = Module.canvas;
		Module.ctx2D = Module.ctx;
	}, CANVAS_W, CANVAS_H);

	s_pxs = (uint32*) malloc(sizeof(uint32) * CANVAS_SCALER*CANVAS_SCALER * INPUT_W*INPUT_H);
}

void Canvas2D_VideoChanged()
{
	//printf("!!!! Canvas2D_VideoChanged(): %dx%d\n", CANVAS_W, CANVAS_H);
	EM_ASM_ARGS({
		var canvas = Module.canvas2D;
		canvas.width = canvas.widthNative = $0;
		canvas.height = canvas.heightNative = $1;
		FCEM.image = Module.ctx2D.getImageData(0, 0, $0, $1);
	}, CANVAS_W, CANVAS_H);
}

void Canvas2D_UpdateController(int idx, double v)
{
	if ((idx == FCEM_BRIGHTNESS) || (idx == FCEM_CONTRAST)
			|| (idx == FCEM_COLOR) || (idx == FCEM_GAMMA)) {
		Canvas2D_RecalcLookup();
	}
}
