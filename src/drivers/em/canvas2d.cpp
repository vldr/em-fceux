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
#include <emscripten.h>
#include <math.h>


// Possible values: 1 or 2, corresponding to 1x and 2x.
#define CANVAS_SCALER 2
#define CANVAS_W (CANVAS_SCALER * 256)
#define CANVAS_H (CANVAS_SCALER * em_scanlines)


extern void genNTSCLookup();
extern double *g_yiqs;

// TODO: tsone: following must match with shaders common
static const double c_convMat[] = {
	1.0,        1.0,        1.0,       // Y
	0.946882,   -0.274788,  -1.108545, // I
	0.623557,   -0.635691,  1.709007   // Q
};

static uint32 *s_lookupRGBA = 0;
static uint32 *s_tmpBuf = 0;


static void canvas2DRecalcLookup()
{
	// TODO: valtteri: refactor out the reused parameter calculations?
	const double u_gamma = GAMMA_NTSC/GAMMA_SRGB + 0.3*GetController(FCEM_GAMMA);
	const double u_brightness = 0.15 * GetController(FCEM_BRIGHTNESS);
	const double u_contrast = 1.0 + 0.4*GetController(FCEM_CONTRAST);
	const double u_color = 1.0 + GetController(FCEM_COLOR);

	for (int color = 0; color < NUM_COLORS; ++color) {
		const int k = 3 * (color*LOOKUP_W + LOOKUP_W-1);
		double yiq[3] = { g_yiqs[k+0], g_yiqs[k+1], g_yiqs[k+2] };
		double rgb[3] = { 0, 0, 0 };

		yiq[1] *= u_color;
		yiq[2] *= u_color;

		for (int x = 0; x < 3; ++x) {

			for (int y = 0; y < 3; ++y) {
				rgb[x] += c_convMat[3*y + x] * yiq[y];
			}
			rgb[x] = (rgb[x] < 0) ? 0 : rgb[x];
			rgb[x] = u_contrast * pow(rgb[x], u_gamma) + u_brightness;
			rgb[x] = (rgb[x] > 1) ? 1 : (rgb[x] < 0) ? 0 : rgb[x];
			rgb[x] = 255.0*rgb[x] + 0.5;
		}
		s_lookupRGBA[color] = (int) rgb[0] | ((int) rgb[1] << 8) | ((int) rgb[2] << 16) | 0xFF000000;
	}
}


void canvas2DRender(uint8 *pixels, uint8* row_deemp)
{
	int row_offs = (INPUT_H - em_scanlines) / 2;

	int k = INPUT_W * row_offs;
	int m = 0;
#if CANVAS_SCALER == 1
	for (int row = row_offs; row < em_scanlines + row_offs; ++row) {
		int deemp = row_deemp[row] << 1;
		for (int x = INPUT_W; x != 0; --x) {
			s_tmpBuf[m] = s_lookupRGBA[pixels[k] + deemp];
			++m;
			++k;
		}
	}
#else
	for (int row = row_offs; row < em_scanlines + row_offs; ++row) {
		int deemp = row_deemp[row] << 1;
		for (int x = INPUT_W; x != 0; --x) {
			s_tmpBuf[m] = s_tmpBuf[m + 1] = s_tmpBuf[m + CANVAS_W]
				= s_tmpBuf[m+1 + CANVAS_W]
				= s_lookupRGBA[pixels[k] + deemp];
			m += 2;
			++k;
		}
		m += CANVAS_W;
	}
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
	}, (ptrdiff_t) s_tmpBuf >> 2);
}

void canvas2DVideoChanged()
{
	//printf("!!!! canvas2DVideoChanged(): %dx%d\n", CANVAS_W, CANVAS_H);
	EM_ASM_ARGS({
		var canvas = Module.canvas2D;
		canvas.width = canvas.widthNative = $0;
		canvas.height = canvas.heightNative = $1;
		FCEM.image = Module.ctx2D.getImageData(0, 0, $0, $1);
	}, CANVAS_W, CANVAS_H);
}

void canvas2DInit()
{
	genNTSCLookup();

	s_lookupRGBA = (uint32*) malloc(sizeof(uint32) * NUM_COLORS);
	canvas2DRecalcLookup();

	EM_ASM_ARGS({
		var canvas = Module.canvas;
		canvas.width = canvas.widthNative = $0;
		canvas.height = canvas.heightNative = $1;
		Module.ctx = Module.createContext(canvas, false, true);
		FCEM.image = Module.ctx.getImageData(0, 0, $0, $1);
		Module.canvas2D = Module.canvas;
		Module.ctx2D = Module.ctx;
	}, CANVAS_W, CANVAS_H);

	s_tmpBuf = (uint32*) malloc(sizeof(uint32) * CANVAS_SCALER*CANVAS_SCALER * INPUT_W*INPUT_H);
}

void canvas2DUpdateController(int idx, double v)
{
	switch(idx) {
	case FCEM_BRIGHTNESS:
	case FCEM_CONTRAST:
	case FCEM_COLOR:
	case FCEM_GAMMA:
		canvas2DRecalcLookup();
		break;
	default:
		break;
	}
}

