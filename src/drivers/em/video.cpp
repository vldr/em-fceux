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
extern Config *g_config;

int webgl_supported = 0;

static int s_inited;
static int s_webgl = -1;

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

// Returns 0 on success, -1 on failure.
int KillVideo()
{
// TODO: tsone: never cleanup video?
#if 0
	// return failure if the video system was not initialized
	if(s_inited == 0)
		return -1;

	// if the rest of the system has been initialized, shut it down
	// check for OpenGL and shut it down
	es2Deinit();

	s_inited = 0;
#endif
	return 0;
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
		var canvas = Module.canvas2D;
		canvas.style.setProperty( "width", $0 + "px", "important");
		canvas.style.setProperty("height", $1 + "px", "important");

		if ($2) {
			canvas = Module.canvas3D;
			canvas.width = canvas.widthNative = $0;
			canvas.height = canvas.heightNative = $1;
			canvas.style.setProperty( "width", $0 + "px", "important");
			canvas.style.setProperty("height", $1 + "px", "important");
		}

	}, s_canvas_width, s_canvas_height, webgl_supported);

	es2SetViewport(s_canvas_width, s_canvas_height);
}

void FCEUD_VideoChanged()
{
	//printf("!!!!! FCEUD_VideoChanged: %d\n", FSettings.PAL);
	PAL = FSettings.PAL ? 1 : 0;
	em_sound_frame_samples = em_sound_rate / (FSettings.PAL ? PAL_FPS : NTSC_FPS);
	em_scanlines = FSettings.PAL ? 240 : 224;

	canvas2DVideoChanged();
	es2VideoChanged();
	Resize();
}

static EM_BOOL FCEM_ResizeCallback(int eventType, const EmscriptenUiEvent *uiEvent, void *userData)
{
	s_window_width = uiEvent->windowInnerWidth;
	s_window_height = uiEvent->windowInnerHeight;
	Resize();
	return 1;
}

void RenderVideo(int draw_splash)
{
	if (draw_splash) {
		DrawSplash();
	}

	if (s_webgl) {
		es2Render(XBuf, deempScan, PALRAM[0]);
	} else {
		canvas2DRender(XBuf, deempScan);
	}
}

void EnableWebGL(int enable)
{
	enable = enable ? webgl_supported : 0;

	EM_ASM_ARGS({
		if ($0) {
			Module.canvas = Module.canvas3D;
			Module.ctx = Module.ctx3D;
			Module.canvas3D.style.display = 'block';
			Module.canvas2D.style.display = 'none';
		} else {
			Module.canvas = Module.canvas2D;
			Module.ctx = Module.ctx2D;
			Module.canvas3D.style.display = 'none';
			Module.canvas2D.style.display = 'block';
		}

		Module.useWebGL = $0;
	}, enable);

	s_webgl = enable;
}

// Return 0 on success, -1 on failure.
int InitVideo()
{
	if (s_inited) {
		return 0;
	}

//	FCEUI_SetShowFPS(1);

	emscripten_set_resize_callback(0, 0, 0, FCEM_ResizeCallback);

	// Init both 2D and WebGL (3D) canvases and contexts.
	FCEU_printf("Initializing canvas 2D context.\n");
	canvas2DInit();
	RegisterCallbacksForCanvas();
	FCEU_printf("Initializing WebGL.\n");
	// NOTE: Following reset of canvas and ctx is required by es2Init().
	EM_ASM({
		Module.canvas = Module.canvas3D;
		Module.ctx = null;
	});
	webgl_supported = es2Init(GetAspect());
	if (webgl_supported) {
		RegisterCallbacksForCanvas();
	}

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

void CanvasToNESCoords(uint32 *x, uint32 *y)
{
	*x = INPUT_W * (*x) / s_canvas_width;
	*y = em_scanlines * (*y) / s_canvas_height;
	*y += (INPUT_H - em_scanlines) / 2;
}
