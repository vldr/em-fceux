/* FCE Ultra - NES/Famicom Emulator - Emscripten video/window
 *
 * Copyright notice for this file:
 *  Copyright (C) 2002 Xodnizel
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
#include "es2.h"
#include "../../fceu.h"
#include "../../video.h"
#include "../../utils/memory.h"
#include <emscripten.h>
#include <emscripten/html5.h>


extern uint8 *XBuf;
extern uint8 deempScan[240];
extern uint8 PALRAM[0x20];

static int s_inited;

static int s_window_width, s_window_height;
static int s_canvas_width, s_canvas_height;


// Functions only needed for linking.
void SetOpenGLPalette(uint8*) {}
void FCEUD_SetPalette(uint8, uint8, uint8, uint8) {}
void FCEUD_GetPalette(uint8 index, uint8 *r, uint8 *g, uint8 *b)
{
	*r = *g = *b = 0;
}

bool FCEUD_ShouldDrawInputAids()
{
	return false;
}

static double GetAspect()
{
	// Aspect is adjusted with CRT TV pixel aspect to get proper look.
	// In-depth details can be found here:
	//   http://wiki.nesdev.com/w/index.php/Overscan
	// While 8:7 pixel aspect is probably correct, this uses 292:256 as it
	// looks better on LCD. This assumes square pixel aspect in output.
	return (256.0/em_scanlines) * (292.0/256.0);
}

static void Resize()
{
	int new_width, new_height;

	const double targetAspect = GetAspect();
	// Aspect for window resize.
	const double aspect = s_window_width / (double) s_window_height;

	if (aspect >= targetAspect) {
		new_width = s_window_height * targetAspect;
		new_height = s_window_height;
	} else {
		new_width = s_window_width;
		new_height = s_window_width / targetAspect;
	}

	if ((new_width == s_canvas_width) && (new_height == s_canvas_height)) {
		return;
	}
	s_canvas_width = new_width;
	s_canvas_height = new_height;

	//printf("!!!! resize: (%dx%d) '(%dx%d) asp:%f\n", s_window_width, s_window_height,
	//	s_canvas_width, s_canvas_height, aspect);

	// HACK: emscripten_set_canvas_size() forces canvas size by setting css style
	// width and height with "!important" flag. Workaround is to set size manually
	// and remove the style attribute. See Emscripten's updateCanvasDimensions()
	// in library_browser.js for the faulty code.
	EM_ASM_INT({
		const canvas = Module.canvas;
		canvas.width = canvas.widthNative = $0;
		canvas.height = canvas.heightNative = $1;
		canvas.style.setProperty( "width", $0 + "px", "important");
		canvas.style.setProperty("height", $1 + "px", "important");
	}, s_canvas_width, s_canvas_height);

	ES2_SetViewport(s_canvas_width, s_canvas_height);
}

void FCEUD_VideoChanged()
{
	//printf("!!!!! FCEUD_VideoChanged: %d\n", FSettings.PAL);
	PAL = FSettings.PAL ? 1 : 0;
	em_sound_frame_samples = em_sound_rate / (FSettings.PAL ? PAL_FPS : NTSC_FPS);
	em_scanlines = FSettings.PAL ? 240 : 224;

	ES2_VideoChanged();
	Resize();
}

static EM_BOOL FCEM_ResizeCallback(int eventType, const EmscriptenUiEvent *uiEvent, void *userData)
{
	s_window_width = uiEvent->windowInnerWidth;
	s_window_height = uiEvent->windowInnerHeight;
	Resize();
	return 1;
}

void Video_Render(int draw_splash)
{
	if (draw_splash) {
		Splash_Draw();
	}

	ES2_Render(XBuf, deempScan, PALRAM[0]);
}

// Return 0 on success, -1 on failure.
int Video_Init()
{
	if (s_inited) {
		return 0;
	}

#if FCEM_DEBUG == 1
	//FCEUI_SetShowFPS(1);
#endif

	emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, 0, FCEM_ResizeCallback);

	FCEU_printf("Initializing WebGL.\n");
	if (!ES2_Init(GetAspect())) {
		puts("ERROR: Failed to initialize video, WebGL not supported.");
		return 0;
	}
	Input_RegisterCallbacksForCanvas();

	// HACK: Manually resize to cover the window inner size.
	// Apparently there's no way to do this with Emscripten...?
	s_window_width = EM_ASM_INT_V({ return window.innerWidth; });
	s_window_height = EM_ASM_INT_V({ return window.innerHeight; });
	s_canvas_width = s_window_width;
	s_canvas_height = s_window_height;
	Resize();

	s_inited = 1;
	return 0;
}

void Video_CanvasToNESCoords(uint32 *x, uint32 *y)
{
	*x = INPUT_W * (*x) / s_canvas_width;
	*y = em_scanlines * (*y) / s_canvas_height;
	*y += (INPUT_H - em_scanlines) / 2;
}

void Video_UpdateController(int idx, double v)
{
	if ((idx == FCEM_BRIGHTNESS) || (idx == FCEM_CONTRAST)
			|| (idx == FCEM_COLOR) || (idx == FCEM_GAMMA)) {
		ntscSetControls(
			0.15 * Config_GetValue(FCEM_BRIGHTNESS),
			1.0 + 0.4*Config_GetValue(FCEM_CONTRAST),
			1.0 + Config_GetValue(FCEM_COLOR),
			GAMMA_NTSC/GAMMA_SRGB + 0.3*Config_GetValue(FCEM_GAMMA)
		);
	}

	ES2_UpdateController(idx, v);
}
