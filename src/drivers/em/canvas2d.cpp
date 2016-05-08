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


// Possible values: 1 or 2, corresponding to 1x and 2x.
#define CANVAS_SCALER 2
#define CANVAS_W (CANVAS_SCALER * 256)
#define CANVAS_H (CANVAS_SCALER * em_scanlines)


static uint32 *s_lookupRGBA = 0;
static uint32 *s_tmpb = 0;


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


void Canvas2D_Render(uint8 *pixels, uint8* row_deemp)
{
	int row_offs = (INPUT_H - em_scanlines) / 2;

	int k = INPUT_W * row_offs;
	int m = 0;
#if CANVAS_SCALER == 1
	for (int row = row_offs; row < em_scanlines + row_offs; ++row) {
		int deemp = row_deemp[row] << 1;
		for (int x = INPUT_W; x != 0; --x) {
			s_tmpb[m] = s_lookupRGBA[pixels[k] + deemp];
			++m;
			++k;
		}
	}
#else
	for (int row = row_offs; row < em_scanlines + row_offs; ++row) {
		int deemp = row_deemp[row] << 1;
		for (int x = INPUT_W; x != 0; --x) {
			uint32 c = s_lookupRGBA[pixels[k] + deemp];
			s_tmpb[m] = s_tmpb[m+1] = s_tmpb[m+CANVAS_W] = s_tmpb[m+CANVAS_W+1] = c;
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
	}, (ptrdiff_t) s_tmpb >> 2);
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

	s_tmpb = (uint32*) malloc(sizeof(uint32) * CANVAS_SCALER*CANVAS_SCALER * INPUT_W*INPUT_H);
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

