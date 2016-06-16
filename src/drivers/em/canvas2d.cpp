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
	int row, const double *yiqs, uint8 overscan_color)
{
	double rgb[2*3];
	int m = 0;

	// assuming 2x scaler, take 4 rgb samples per ppu pixel
#define LK_N 8 // must be >= NUM_TAPS and power of two
	int lk[LK_N];
	int lk_top = 0;
	int phase = ((-row)%3 - NUM_TAPS/2-1 + 256*NUM_PHASES) % NUM_PHASES; // avoid negative in phase
	for (int x = -NUM_TAPS/2+1; x < NUM_TAPS/2+1; ++x) {
		int idx = (x < 0) ? overscan_color : row_pixels[x];
		idx += deemp;
		lk[lk_top] = 3 * (idx*LOOKUP_W + phase*NUM_SUBPS*NUM_TAPS);
		lk_top = (lk_top + 1) & (LK_N-1);
		phase = (phase < NUM_PHASES-1) ? phase + 1 : 0;
	}
	for (int x = 0; x < INPUT_W; ++x) {
		int lk_i = (lk_top - NUM_TAPS + LK_N) & (LK_N-1);
		double yiq[4*3] = { 0.0, 0.0, 0.0,  0.0, 0.0, 0.0,
			0.0, 0.0, 0.0,  0.0, 0.0, 0.0 };

		for (int tap3 = 0; tap3 < 3 * NUM_TAPS; tap3 += 3) {
			int k0 = lk[lk_i] + tap3; // subp 0
			int k1 = k0 + 3*NUM_TAPS; // subp 1
			int k2 = k1 + 3*NUM_TAPS; // subp 2
//			int k3 = k2 + 3*NUM_TAPS; // subp 3

			yiq[0] += yiqs[k0  ];
			yiq[1] += yiqs[k0+1];
			yiq[2] += yiqs[k0+2];

//			yiq[3] += yiqs[k1  ];
//			yiq[4] += yiqs[k1+1];
//			yiq[5] += yiqs[k1+2];

			yiq[6] += yiqs[k2  ];
			yiq[7] += yiqs[k2+1];
			yiq[8] += yiqs[k2+2];

//			yiq[9 ] += yiqs[k3  ];
//			yiq[10] += yiqs[k3+1];
//			yiq[11] += yiqs[k3+2];

			lk_i = (lk_i + 1) & (LK_N-1);
		}

		yiq[0] *= (8.0/2.0) / (double) YW2;
		yiq[1] *= (8.0/2.0) / (double) (CW2-2.0);
		yiq[2] *= (8.0/2.0) / (double) (CW2-2.0);

//		yiq[3] *= (8.0/2.0) / (double) YW2;
//		yiq[4] *= (8.0/2.0) / (double) (CW2-2.0);
//		yiq[5] *= (8.0/2.0) / (double) (CW2-2.0);

		yiq[6] *= (8.0/2.0) / (double) YW2;
		yiq[7] *= (8.0/2.0) / (double) (CW2-2.0);
		yiq[8] *= (8.0/2.0) / (double) (CW2-2.0);

//		yiq[9 ] *= (8.0/2.0) / (double) YW2;
//		yiq[10] *= (8.0/2.0) / (double) (CW2-2.0);
//		yiq[11] *= (8.0/2.0) / (double) (CW2-2.0);

		int c;
		// subp 0 & 1
		ntscYIQ2RGB(rgb  , yiq  );
//		ntscYIQ2RGB(rgb+3, yiq+3);
//		rgb[0] = (rgb[0] + rgb[3]) * 0.5;
//		rgb[1] = (rgb[1] + rgb[4]) * 0.5;
//		rgb[2] = (rgb[2] + rgb[5]) * 0.5;
		c = (int) (255.0*rgb[0] + 0.5)
			| ((int) (255.0*rgb[1] + 0.5) << 8)
			| ((int) (255.0*rgb[2] + 0.5) << 16)
			| 0xFF000000;
		dst[m] = c;

		// subp 1/2
		ntscYIQ2RGB(rgb  , yiq+6);
//		ntscYIQ2RGB(rgb+3, yiq+9);
//		rgb[0] = (rgb[0] + rgb[3]) * 0.5;
//		rgb[1] = (rgb[1] + rgb[4]) * 0.5;
//		rgb[2] = (rgb[2] + rgb[5]) * 0.5;
		c = (int) (255.0*rgb[0] + 0.5)
			| ((int) (255.0*rgb[1] + 0.5) << 8)
			| ((int) (255.0*rgb[2] + 0.5) << 16)
			| 0xFF000000;
		dst[m+1] = c;

		m += 2;

		int idx = (x + NUM_TAPS/2+1 >= INPUT_W) ? overscan_color : row_pixels[x + NUM_TAPS/2+1];
		idx += deemp;
		lk[lk_top] = 3 * (idx*LOOKUP_W + phase*NUM_SUBPS*NUM_TAPS);
		lk_top = (lk_top + 1) & (LK_N-1);
		phase = (phase < NUM_PHASES-1) ? phase + 1 : 0;
	}
}

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

static void ConvertNTSC2(const uint8 *pixels, const uint8* row_deemp, uint8 overscan_color)
{
	print_fps();

	int row_offs = (INPUT_H - em_scanlines) / 2;

	const double *yiqs = ntscGetLookup();

	pixels += INPUT_W * row_offs;
	uint32 *dst = s_pxs;
	for (int row = 0; row < em_scanlines; ++row) {

		ConvertNTSCScan2(dst, pixels, row_deemp[row+row_offs] << 1, row, yiqs, overscan_color);
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
