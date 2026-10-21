# Changelog

This file follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Release tags must
match a section heading exactly (for example `v1.0.0`): CI uses the matching
section as the GitHub Release notes.

## [Unreleased]

## [v1.0.3] - 2026-10-21

### Fixed

- Recognize the original CD's file revisions in the browser converter, while
  continuing to skip files that do not match the known game-data hashes.

## [v1.0.2] - 2026-10-18

### Fixed

- Choppy sound on the console: the audio is now fed by the firmware's
  interrupt from a ring the game fills about 93 ms ahead (Retro-Go SD PCM
  ring), so frames longer than a DMA half buffer no longer break it.

### Added

- The first minute of statistics (frames per second, longest frame, audio
  underruns) is written to `/data/jazz/<name>/log.txt` on the SD card.

## [v1.0.1] - 2026-10-18

### Fixed

- Sound too quiet / garbled on the console: the audio half buffer is 46 ms
  instead of 16.7 ms (one frame: a frame a little longer than that left a
  half buffer unfilled, played again), and the music and the sound effects
  are twice as loud (the game's default volumes are half of the maximum).

## [v1.0.0] - 2026-10-18

### Added

- Jazz Jackrabbit 1 (PC DOS: full game, shareware, Holiday Hare) as a
  Retro-Go SD GWHB homebrew, built on OpenJazz and the retro-go-sd-templates
  SDK (firmware ABI v2, Retro-Go SD v2.0.0-rc1 or later), on a small SDL 1.2
  written for the console.
- Game data from a `.jj` pack next to the `.bin`, used in place from the
  external flash (`tools/jj_pack.py`), with every picture pre-decoded and the
  music pre-rendered on the PC (`jj_render` for Windows, macOS and Linux).
- Savestates (level, checkpoint, score, lives, ammo), resume from the
  launcher.
- Pause menu options: music, sound effects, swap A/B.
- 320x240 picture (the whole screen), 60 frames per second.
