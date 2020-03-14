# em-fceux

https://bitbucket.org/tsone/em-fceux/

Web port of the [FCEUX](https://github.com/TASVideos/fceux/) Nintendo
Entertainment System/Famicom (NES/FC) emulator. Powered by
[Emscripten](https://emscripten.org/).

Try it at https://tsone.kapsi.fi/em-fceux/.

## Overview

em-fceux enables the FCEUX emulator core feators on web browsers. The emulation
runs at 60 FPS on modern computers, has low input latency and good audio
quality. It also supports save states and battery-backed save RAM.

Additionally it has a new custom NTSC video signal and CRT TV emulation modes.

## Features

There are some modifications in FCEUX to make the code suitable for Emscripten.
Primary addition is WebGL renderer which enables the use of shaders.

Supported FCEUX features:

- Both NTSC and PAL system emulation.
- All mappers supported by FCEUX.
- Save states and battery-backed SRAM.
- Speed throttling.
- Support for two game controllers.
- Zapper support.
- Support for NES, ZIP and NSF file formats.

_Unsupported_ FCEUX features:

- New PPU emulation (old PPU emulation used for performance).
- FDS disk system.
- VS system.
- Special peripherals (Family Keyboard, Mahjong controller, etc.)
- Screenshots and movie recording.
- Cheats, debug, TAS and Lua scripting features.

New features:

- NTSC composite video emulation.
- CRT TV emulation.

## API

See [API.md](API.md).

## Build

Setup:

1. [Install and activate Emscripten 1.39.4 (upstream)](https://emscripten.org/docs/getting_started/downloads.html).
2. Have python 2.7.x.
3. Install [scons](https://scons.org/pages/download.html).
4. Run `source emsdk_env.sh` to setup Emscripten env.
   - Note, this also sets `npm` to env.
5. Run `npm install`.

Build for debug with `npm run build:debug` and for release with `npm run build`
(results will be in `dist/`).

### Building Shaders

You only need to rebuild the shaders when the shader sources in
`src/drivers/em/assets/shaders/` are changed. Do this with
`npm run build:shaders`. This will download and build `glsl-optimizer` from
source, and requires cmake and a C++ compiler.

Note, `npm run build:shaders` must be run before `npm run build` as the shaders
are embedded in the output binary.

## Browser Requirements

- [WebAssembly](https://webassembly.org/).
- [WebGL](https://www.khronos.org/webgl/).
- [Web Audio API](https://www.w3.org/TR/webaudio/).

## NTSC video signal and CRT TV Emulation Details

The NTSC signal emulation models the composite YIQ output. Separation of YIQ
luminance (Y) and chrominance (IQ/UV) is done with a "1D comb filter" technique
which reduces chroma fringing compared to band/low-pass filtering. Under the
hood, YIQ to RGB conversion relies on a large lookup texture which is referenced
in a fragment shader.

## Contact

Authored by Valtteri "tsone" Heikkilä. See git commits for email.

Please submit bugs and feature requests in the
[em-fceux issue tracker](https://bitbucket.org/tsone/em-fceux/issues?status=new&status=open).

## License

Licensed under [GNU GPL 2](https://www.gnu.org/licenses/gpl-2.0.txt).

This port if base on
[FCEUX 2.2.2 source code release](http://sourceforge.net/projects/fceultra/files/Source%20Code/2.2.2%20src/fceux-2.2.2.src.tar.gz/download).
