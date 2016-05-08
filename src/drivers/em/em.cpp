/* FCE Ultra - NES/Famicom Emulator - Emscripten main
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
#include "../../fceu.h"
#include "../../version.h"
#include <emscripten.h>


// Number of frames to skip per regular frame when frameskipping.
#define TURBO_FRAMESKIPS 3
// Set to 1 to set mainloop call at higher rate than requestAnimationFrame.
// It's recommended to set this to 0, i.e. to use requestAnimationFrame.
#define SUPER_RATE 0

// For s_status.
#define STATUS_INIT (1<<0)
#define STATUS_LOAD (1<<1)


bool em_no_waiting = true;
int em_scanlines = 224; // Default is NTSC, 224.

static int s_status = 0;


static int CloseGame()
{
	if (!(s_status & STATUS_LOAD)) {
		return 0;
	}

	FCEUI_CloseGame();

//	DriverKill();
	s_status &= ~STATUS_LOAD;
	GameInfo = 0;
	return 1;
}

/**
 * Loads a game, given a full path/filename.  The driver code must be
 * initialized after the game is loaded, because the emulator code
 * provides data necessary for the driver code(number of scanlines to
 * render, what virtual input devices to use, etc.).
 */
static int LoadGame(const char *path)
{
	if (s_status & STATUS_LOAD) {
		CloseGame();
	}

	// Pass user's autodetect setting to LoadGame().
	int autodetect = (Config_GetValue(FCEM_VIDEO_SYSTEM) <= -1) ? 1 : 0;
	if(!FCEUI_LoadGame(path, autodetect)) {
		return 0;
	}

	s_status |= STATUS_LOAD;
	return 1;
}

static void EmulateFrame(int frameskipmode)
{
	uint8 *gfx = 0;
	int32 *sound;
	int32 ssize;

	FCEUD_UpdateInput();
	FCEUI_Emulate(&gfx, &sound, &ssize, frameskipmode);
	Sound_Write(sound, ssize);
}

static int DoFrame()
{
	if (em_no_waiting) {
		for (int i = 0; i < TURBO_FRAMESKIPS; ++i) {
			EmulateFrame(2);
		}
	}

	// Get the number of frames to fill the audio buffer.
	int frames = (SOUND_BUF_MAX - Sound_GetBufferCount()) / em_sound_frame_samples;

	// It's possible audio to go ahead of visuals. If so, skip all emulation for this frame.
// TODO: tsone: this is not a good solution as it may cause unnecessary skipping in emulation
	if (Sound_IsInitialized() && frames <= 0) {
		return 0;
	}

	// Skip frames (video) to fill the audio buffer. Leave two frames free for next requestAnimationFrame in case they come too frequently.
	if (Sound_IsInitialized() && (frames > 3)) {
		// Skip only even numbers of frames to correctly display flickering sprites.
		frames = (frames - 3) & (~1);
		while (frames > 0) {
			EmulateFrame(1);
			--frames;
		}
	}

	EmulateFrame(0);
	return 1;
}

static void ReloadROM(void*)
{
	char *filename = emscripten_run_script_string("Module.romName");
//	CloseGame();
	LoadGame(filename);
}

static void MainLoop()
{
	if (s_status & STATUS_INIT) {
		if (GameInfo) {
			if (!DoFrame()) {
				return; // Frame was not processed, skip rest of this callback.
			} else {
				Video_Render(0);
			}
		} else {
			Video_Render(1);
		}
	}

// FIXME: tsone: should be probably using exported funcs
	int reload = EM_ASM_INT_V({ return Module.romReload||0; });
	if (reload) {
		emscripten_push_main_loop_blocker(ReloadROM, 0);
		EM_ASM({ Module.romReload = 0; });
	}
}

static int DriverInitialize()
{
	Video_Init();
	Sound_Init();
	s_status |= STATUS_INIT;

	int fourscore = 0; // Set to 1 to enable FourScore.
// TODO: tsone: this sets two controllers by default, but should be more flexible
	FCEUD_SetInput(fourscore, false, SI_GAMEPAD, SI_GAMEPAD, SIFC_NONE);

	return 1;
}


EMUFILE_FILE* FCEUD_UTF8_fstream(const char *fn, const char *m)
{
// TODO: tsone: mode variable is not even used, remove?
	std::ios_base::openmode mode = std::ios_base::binary;
	if(!strcmp(m,"r") || !strcmp(m,"rb"))
		mode |= std::ios_base::in;
	else if(!strcmp(m,"w") || !strcmp(m,"wb"))
		mode |= std::ios_base::out | std::ios_base::trunc;
	else if(!strcmp(m,"a") || !strcmp(m,"ab"))
		mode |= std::ios_base::out | std::ios_base::app;
	else if(!strcmp(m,"r+") || !strcmp(m,"r+b"))
		mode |= std::ios_base::in | std::ios_base::out;
	else if(!strcmp(m,"w+") || !strcmp(m,"w+b"))
		mode |= std::ios_base::in | std::ios_base::out | std::ios_base::trunc;
	else if(!strcmp(m,"a+") || !strcmp(m,"a+b"))
		mode |= std::ios_base::in | std::ios_base::out | std::ios_base::app;
	return new EMUFILE_FILE(fn, m);
}

FILE *FCEUD_UTF8fopen(const char *fn, const char *mode)
{
	return fopen(fn, mode);
}

static char *s_linuxCompilerString = "emscripten " __VERSION__;
const char *FCEUD_GetCompilerString()
{
	return (const char*) s_linuxCompilerString;
}

int main(int argc, char *argv[])
{
	int error;

	FCEUD_Message("Starting " FCEU_NAME_AND_VERSION "...\n");

	Config_Init();

	// initialize the infrastructure
	error = FCEUI_Initialize();
	std::string s;

	// override savegame, savestate and rom directory with IndexedDB mount at /fceux
	FCEUI_SetDirOverride(FCEUIOD_NV, "fceux/sav");
	FCEUI_SetDirOverride(FCEUIOD_STATES, "fceux/sav");
//	FCEUI_SetDirOverride(FCEUIOD_ROMS, "fceux/rom");

	FCEUI_SetVidSystem(0);
	FCEUI_SetGameGenie(0);
	FCEUI_SetLowPass(0);
	FCEUI_DisableSpriteLimitation(0);

	int start = 0, end = 239;
// TODO: tsone: can be removed? not sure what this is.. it's disabled due to #define
#if DOING_SCANLINE_CHECKS
	for(int i = 0; i < 2; x++) {
		if(srendlinev[x]<0 || srendlinev[x]>239) srendlinev[x]=0;
		if(erendlinev[x]<srendlinev[x] || erendlinev[x]>239) erendlinev[x]=239;
	}
#endif
	FCEUI_SetRenderedLines(start + 8, end - 8, start, end);

// TODO: tsone: do not use new PPU, it's probably not working
	int use_new_ppu = 0;
	if (use_new_ppu) {
		newppu = 1;
	}

	DriverInitialize();

#if SUPER_RATE
// NOTE: tsone: UNTESTED! higher call rate to generate frames
	emscripten_set_main_loop(MainLoop, 2 * (em_sound_rate + SOUND_HW_BUF_MAX-1) / SOUND_HW_BUF_MAX, 1);
#else
	emscripten_set_main_loop(MainLoop, 0, 1);
#endif

	return 0;
}

uint64 FCEUD_GetTime()
{
	return emscripten_get_now();
}

uint64 FCEUD_GetTimeFreq(void)
{
	// emscripten_get_now() returns time in milliseconds.
	return 1000;
}

void FCEUD_Message(const char *text)
{
	fputs(text, stdout);
}

void FCEUD_PrintError(const char *errormsg)
{
	fprintf(stderr, "%s\n", errormsg);
}


// NOTE: tsone: following are non-implemented "dummy" functions in this driver
#define DUMMY(__f) void __f() {}
DUMMY(FCEUD_DebugBreakpoint)
DUMMY(FCEUD_TraceInstruction)
DUMMY(FCEUD_HideMenuToggle)
DUMMY(FCEUD_MovieReplayFrom)
DUMMY(FCEUD_ToggleStatusIcon)
DUMMY(FCEUD_AviRecordTo)
DUMMY(FCEUD_AviStop)
void FCEUI_AviVideoUpdate(const unsigned char* buffer) {}
int FCEUD_ShowStatusIcon(void) {return 0;}
bool FCEUI_AviIsRecording(void) {return false;}
void FCEUI_UseInputPreset(int preset) {}
bool FCEUD_PauseAfterPlayback() { return false; }
// These are actually fine, but will be unused and overriden by the current UI code.
void FCEUD_TurboToggle() { em_no_waiting = !em_no_waiting; }
FCEUFILE* FCEUD_OpenArchiveIndex(ArchiveScanRecord& asr, std::string &fname, int innerIndex) { return 0; }
FCEUFILE* FCEUD_OpenArchive(ArchiveScanRecord& asr, std::string& fname, std::string* innerFilename) { return 0; }
ArchiveScanRecord FCEUD_ScanArchive(std::string fname) { return ArchiveScanRecord(); }
