/* FCE Ultra - NES/Famicom Emulator - Emscripten configuration
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
#include "../../ines.h"
#include <emscripten.h>


static double s_c[FCEM_CONTROLLER_COUNT];
static const ESI s_periMap[] = {
	SI_GAMEPAD,
	SI_ZAPPER
};
static const int s_periMapSize = sizeof(s_periMap) / sizeof(*s_periMap);

// TODO: tsone: do we actually need Config at all?
#if 0
static Config *s_cfg = NULL;
#endif


/**
 * Creates the subdirectories used for saving snapshots, movies, game
 * saves, etc.  Hopefully obsolete with new configuration system.
 */
static void CreateDirs(const std::string &dir)
{
	char *subs[8]={"fcs","snaps","gameinfo","sav","cheats","movies","cfg.d"};
	std::string subdir;
	int x;

	mkdir(dir.c_str(), S_IRWXU);
	for(x = 0; x < 6; x++) {
		subdir = dir + PSS + subs[x];
		mkdir(subdir.c_str(), S_IRWXU);
	}
}

/**
 * Attempts to locate FCEU's application directory.  This will
 * hopefully become obsolete once the new configuration system is in
 * place.
 */
static void GetBaseDirectory(std::string &dir)
{
	char *home = getenv("HOME");
	if(home) {
		dir = std::string(home) + "/.fceux";
	} else {
		dir = "";
	}
}

static void WrapBindPort(int portIdx, int periId)
{
	if (periId >= 0 && periId < s_periMapSize) {
		Input_BindPort(portIdx, s_periMap[periId]);
	}
}


// returns a config structure with default options
// also creates config base directory (ie: /home/user/.fceux as well as subdirs
void Config_Init()
{
	std::string dir, prefix;

	GetBaseDirectory(dir);

	FCEUI_SetBaseDirectory(dir.c_str());
	CreateDirs(dir);

// TODO: tsone: do we actually need Config at all?
#if 0
	s_cfg = new Config(dir);
#endif

// TODO: tsone: network play is disabled
#if 0
	// network play options - netplay is broken
	config->addOption("server", "SDL.NetworkIsServer", 0);
	config->addOption('n', "net", "SDL.NetworkIP", "");
	config->addOption('u', "user", "SDL.NetworkUsername", "");
	config->addOption('w', "pass", "SDL.NetworkPassword", "");
	config->addOption('k', "netkey", "SDL.NetworkGameKey", "");
	config->addOption("port", "SDL.NetworkPort", 4046);
	config->addOption("players", "SDL.NetworkPlayers", 1);
#endif

// TODO: tsone: fc extension port peripherals support
#if PERI
	// PowerPad 0 - 1
	for(unsigned int i = 0; i < POWERPAD_NUM_DEVICES; i++) {
		char buf[64];
		snprintf(buf, 20, "SDL.Input.PowerPad.%d.", i);
		prefix = buf;

		config->addOption(prefix + "DeviceType", DefaultPowerPadDevice[i]);
		config->addOption(prefix + "DeviceNum",  0);
		for(unsigned int j = 0; j < POWERPAD_NUM_BUTTONS; j++) {
			config->addOption(prefix +PowerPadNames[j], DefaultPowerPad[i][j]);
		}
	}

	// QuizKing
	prefix = "SDL.Input.QuizKing.";
	config->addOption(prefix + "DeviceType", DefaultQuizKingDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < QUIZKING_NUM_BUTTONS; j++) {
		config->addOption(prefix + QuizKingNames[j], DefaultQuizKing[j]);
	}

	// HyperShot
	prefix = "SDL.Input.HyperShot.";
	config->addOption(prefix + "DeviceType", DefaultHyperShotDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < HYPERSHOT_NUM_BUTTONS; j++) {
		config->addOption(prefix + HyperShotNames[j], DefaultHyperShot[j]);
	}

	// Mahjong
	prefix = "SDL.Input.Mahjong.";
	config->addOption(prefix + "DeviceType", DefaultMahjongDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < MAHJONG_NUM_BUTTONS; j++) {
		config->addOption(prefix + MahjongNames[j], DefaultMahjong[j]);
	}

	// TopRider
	prefix = "SDL.Input.TopRider.";
	config->addOption(prefix + "DeviceType", DefaultTopRiderDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < TOPRIDER_NUM_BUTTONS; j++) {
		config->addOption(prefix + TopRiderNames[j], DefaultTopRider[j]);
	}

	// FTrainer
	prefix = "SDL.Input.FTrainer.";
	config->addOption(prefix + "DeviceType", DefaultFTrainerDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < FTRAINER_NUM_BUTTONS; j++) {
		config->addOption(prefix + FTrainerNames[j], DefaultFTrainer[j]);
	}

	// FamilyKeyBoard
	prefix = "SDL.Input.FamilyKeyBoard.";
	config->addOption(prefix + "DeviceType", DefaultFamilyKeyBoardDevice);
	config->addOption(prefix + "DeviceNum", 0);
	for(unsigned int j = 0; j < FAMILYKEYBOARD_NUM_BUTTONS; j++) {
		config->addOption(prefix + FamilyKeyBoardNames[j],
						DefaultFamilyKeyBoard[j]);
	}

	// All mouse devices
	config->addOption("SDL.OekaKids.0.DeviceType", "Mouse");
	config->addOption("SDL.OekaKids.0.DeviceNum", 0);

	config->addOption("SDL.Arkanoid.0.DeviceType", "Mouse");
	config->addOption("SDL.Arkanoid.0.DeviceNum", 0);

	config->addOption("SDL.Shadow.0.DeviceType", "Mouse");
	config->addOption("SDL.Shadow.0.DeviceNum", 0);

	config->addOption("SDL.Zapper.0.DeviceType", "Mouse");
	config->addOption("SDL.Zapper.0.DeviceNum", 0);
#endif
}

double Config_GetValue(int idx)
{
	assert(idx >= 0 && idx < FCEM_CONTROLLER_COUNT);
	return s_c[idx];
}


//>> Emscripten externals
extern "C"
{

// Write savegame and synchronize IDBFS contents to IndexedDB. Must be a C-function.
void FCEM_OnSaveGameInterval()
{
    if (GameInterface) {
        GameInterface(GI_SAVE);
    }
    EM_ASM({
      FS.syncfs(FCEM.onSyncToIDB);
    });
}

// Set control value.
void FCEM_SetController(int idx, double v)
{
	if ((idx < 0) || (idx >= FCEM_CONTROLLER_COUNT)) {
// TODO: tsone: warning message if contoller idx is invalid?
		// Skip if control idx is invalid.
		return;
	}

	s_c[idx] = v;

	switch (idx) {
	case FCEM_PORT2:
		WrapBindPort(1, (int) v);
		break;
	case FCEM_VIDEO_SYSTEM:
		//printf("!!!!! VIDEO_SYSTEM: %f\n", v);
		if (v <= -1) {
			FCEUI_SetVidSystem(iNESDetectVidSys()); // Attempt auto-detection.
		} else if (v <= 1) {
			FCEUI_SetVidSystem(v);
		}
		if (!GameInfo) {
			// This is only when game is not yet loaded...
			FCEUD_VideoChanged();
		}
		break;
	default:
		if ((idx >= FCEM_BRIGHTNESS) && (idx <= FCEM_NOISE)) {
			Video_UpdateController(idx, v);
		}
		break;
	}
}

void FCEM_SilenceSound(int option)
{
	Sound_Silence(option);
}

} //<< Emscripten externals
