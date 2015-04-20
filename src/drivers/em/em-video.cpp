/* FCE Ultra - NES/Famicom Emulator
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

/// \file
/// \brief Handles the graphical game display for the SDL implementation.

#include "em.h"
#include "em-es2.h"
#include "../../fceu.h"
#include "../../version.h"
#include "../../video.h"

#include "../../utils/memory.h"

#include "dface.h"

#include "../common/configSys.h"
#include "em-video.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <emscripten.h>
#include <emscripten/html5.h>

// GLOBALS
extern Config *g_config;

static int s_curbpp;
static int s_srendline, s_erendline;
static int s_tlines;
static int s_inited;

static double s_exs, s_eys;
static int s_clipSides;
static int s_nativeWidth = -1;
static int s_nativeHeight = -1;

#define NWIDTH	(256 - (s_clipSides ? 16 : 0))
#define NOFFSET	(s_clipSides ? 8 : 0)

//draw input aids if we are fullscreen
bool FCEUD_ShouldDrawInputAids()
{
	return false;
}
 
/**
 * Attempts to destroy the graphical video display.  Returns 0 on
 * success, -1 on failure.
 */
int
KillVideo()
{
	// return failure if the video system was not initialized
	if(s_inited == 0)
		return -1;
    
	// if the rest of the system has been initialized, shut it down
	// check for OpenGL and shut it down
	KillOpenGL();

	s_inited = 0;
	return 0;
}

/**
 * These functions determine an appropriate scale factor for fullscreen/
 */
inline double GetXScale(int xres)
{
	return ((double)xres) / NWIDTH;
}
inline double GetYScale(int yres)
{
	return ((double)yres) / s_tlines;
}

void FCEUD_VideoChanged()
{
	if(FSettings.PAL)
		PAL = 1;
	else
		PAL = 0;
}

/**
 * Attempts to initialize the graphical video display.  Returns 0 on
 * success, -1 on failure.
 */
int
InitVideo(FCEUGI *gi)
{
#if 1
	s_clipSides = 0; // Don't clip left side.

	// check the starting, ending, and total scan lines
	FCEUI_GetCurrentVidSystem(&s_srendline, &s_erendline);
	s_tlines = s_erendline - s_srendline + 1;

	// Scale x to compensate the 24px overscan.
	s_exs = 4.0 * (280.0/256.0) + 0.5/256.0;
	s_eys = 4.0;
	int w = (int) (NWIDTH * s_exs);
	int h = (int) (s_tlines * s_eys);
	s_nativeWidth = w;
	s_nativeHeight = h;

	s_inited = 1;

	FCEUI_SetShowFPS(0);
    
	FCEU_printf("Initializing with OpenGL.\n");

  emscripten_set_canvas_size(w, h);
  EmscriptenWebGLContextAttributes attr;
  emscripten_webgl_init_context_attributes(&attr);
  attr.alpha = attr.antialias = attr.premultipliedAlpha = 1;
  attr.depth = attr.stencil = attr.preserveDrawingBuffer = attr.preferLowPowerToHighPerformance = attr.failIfMajorPerformanceCaveat = 0;
  attr.enableExtensionsByDefault = 0;
  attr.majorVersion = 1;
  attr.minorVersion = 0;
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(0, &attr);
  emscripten_webgl_make_context_current(ctx);

	s_curbpp = 32;

		if(!InitOpenGL(NOFFSET, 256 - (s_clipSides ? 8 : 0),
					s_srendline, s_erendline + 1,
					s_exs, s_eys,
					0, 0)) 
		{
			FCEUD_PrintError("Error initializing OpenGL.");
			KillVideo();
			return -1;
		}

    return 0;

#else
	// XXX soules - const?  is this necessary?
	const SDL_VideoInfo *vinf;
	int error;

	FCEUI_printf("Initializing video...");

	s_clipSides = 0; // Don't clip left side.

	// check the starting, ending, and total scan lines
	FCEUI_GetCurrentVidSystem(&s_srendline, &s_erendline);
	s_tlines = s_erendline - s_srendline + 1;

	// initialize the SDL video subsystem if it is not already active
	if(!SDL_WasInit(SDL_INIT_VIDEO)) {
		error = SDL_InitSubSystem(SDL_INIT_VIDEO);
		if(error) {
			FCEUD_PrintError(SDL_GetError());
			return -1;
		}
	}

	s_inited = 1;

	vinf = SDL_GetVideoInfo();
    
	// get the monitor's current resolution if we do not already have it
	if(s_nativeWidth < 0) {
		s_nativeWidth = vinf->current_w;
	}
	if(s_nativeHeight < 0) {
		s_nativeHeight = vinf->current_h;
	}

	FCEUI_SetShowFPS(0);
    

	FCEU_printf("Initializing with OpenGL.\n");
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 2);

	// Scale x to compensate the 24px overscan.
	s_exs = 4.0 * (280.0/256.0) + 0.5/256.0;
	s_eys = 4.0;

	const int flags = SDL_DOUBLEBUF | SDL_HWSURFACE | SDL_OPENGL;
	s_screen = SDL_SetVideoMode((int)(NWIDTH * s_exs), (int)(s_tlines * s_eys), 32, flags);
	if(!s_screen) {
		FCEUD_PrintError(SDL_GetError());
		return -1;
	}

//	SDL_SetAlpha(s_screen, SDL_SRCALPHA, 255);
	 
	s_curbpp = s_screen->format->BitsPerPixel;
	if(!s_screen) {
		FCEUD_PrintError(SDL_GetError());
		KillVideo();
		return -1;
	}

	int alphaBits = 0;
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &alphaBits);

	FCEU_printf(" Video Mode: %d x %d x %d bpp %d alpha\n",
				s_screen->w, s_screen->h, s_screen->format->BitsPerPixel, alphaBits);

	if(s_curbpp != 8 && s_curbpp != 16 && s_curbpp != 24 && s_curbpp != 32) {
		FCEU_printf("  Sorry, %dbpp modes are not supported by FCE Ultra.  Supported bit depths are 8bpp, 16bpp, and 32bpp.\n", s_curbpp);
		KillVideo();
		return -1;
	}

	// if the game being run has a name, set it as the window name
	if(gi)
	{
		if(gi->name) {
			SDL_WM_SetCaption((const char *)gi->name, (const char *)gi->name);
		} else {
			SDL_WM_SetCaption(FCEU_NAME_AND_VERSION,"FCE Ultra");
		}
	}

	if(s_curbpp > 8) {
		if(!InitOpenGL(NOFFSET, 256 - (s_clipSides ? 8 : 0),
					s_srendline, s_erendline + 1,
					s_exs, s_eys,
					0, 0)) 
		{
			FCEUD_PrintError("Error initializing OpenGL.");
			KillVideo();
			return -1;
		}
	}
	return 0;
#endif
}

// TODO: tsone: may be removed if fceux doesn't really use these..
static unsigned int *s_pal = 0;

void
FCEUD_SetPalette(uint8 index,
                 uint8 r,
                 uint8 g,
                 uint8 b)
{
	FCEU_ARRAY_EM(s_pal, unsigned int, 256); 
	s_pal[index] = (b << 16) | (g << 8) | r;
}

void
FCEUD_GetPalette(uint8 index,
				uint8 *r,
				uint8 *g,
				uint8 *b)
{
	if (s_pal) {
		*r = s_pal[index] & 255;
		*g = (s_pal[index] >> 8) & 255;
		*b = (s_pal[index] >> 16) & 255;
	}
}

/**
 *  Converts an x-y coordinate in the window manager into an x-y
 *  coordinate on FCEU's screen.
 */
void PtoV(int *x, int *y)
{
	*y /= s_eys;
	*x /= s_exs;
	if (s_clipSides) {
		*x += 8;
	}
	*y += s_srendline;
}

// TODO: tsone: AVI recording should be removed, these are unnecessary
bool enableHUDrecording = false;
bool FCEUI_AviEnableHUDrecording()
{
	if (enableHUDrecording)
		return true;

	return false;
}
void FCEUI_SetAviEnableHUDrecording(bool enable)
{
	enableHUDrecording = enable;
}

bool disableMovieMessages = false;
bool FCEUI_AviDisableMovieMessages()
{
	if (disableMovieMessages)
		return true;

	return false;
}
void FCEUI_SetAviDisableMovieMessages(bool disable)
{
	disableMovieMessages = disable;
}