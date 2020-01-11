
# Changelog

This is changelog for em-fceux.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).


## [0.5.0] - 2020-01-11
### Added
- Changelog.
### Deprecated
- asm.js site fallback (use WebAssembly only).
### Fixed
- #28 - Games not loading on Chrome.
- Web Audio context creation from user input.
- Safari audio not starting.
- Build scripts, upgrade to emscripten 1.39.4 (upstream).

## [0.4.1] - 2018-01-28
### Changed
- Migrate to WebAssembly (have asm.js site as fallback).
### Fixed
- #27 - Zapper not working in Safari on MacBook.


## [0.4.0] - 2018-01-27
### Added
- First release.
