# BG-13 — PCM source separation and Spotify integration roadmap

Status: BG-13 implemented and PSP-3000 hardware accepted (2026-11-19). Baseline: commit 5d30eca (PSP-3000 / ARK-5 / Metal Slug Anthology hardware acceptance).

## Accepted baseline — do not regress
- GAME PRX loads and unloads safely.
- L+R+TRIANGLE menu, controller neutralization, emergency release.
- The overlay is stable after gating vblank fallback on successful framebuffer-hook paints.
- WAV music and game audio play concurrently; music continues after closing the menu.
- The earlier PRX is backed up on the test Memory Stick.
- **No Spotify streaming in the GAME PRX yet.**

## Existing separation
- `background/pspotify_bg.c`: scans `music01.wav` through `music08.wav`, or `music.wav`; accepts PCM16 44.1 kHz mono/stereo, fills 1024-frame stereo PCM blocks, and outputs through a reserved audio channel or SRC fallback.
- `background/bg_audio.c`: independent game-volume hooks.
- `background/bg_overlay.c`, `bg_input.c`: menu and input handling; leave untouched for BG-13.
- `spotify/player.c`: user-EBOOT Spotify Ogg/AES/CDN pipeline; uses Media Engine safe-task Vorbis decoding, 3 MiB streaming ring and a 4-slot audio output ring.
- The BG PRX exits its worker when PSPotify ME EBOOT is detected. The EBOOT cannot be assumed resident while a game runs.

## BG-13 implementation target
Refactor the *WAV-only data path* into two explicit layers:
1. Source API: open/read 1024 stereo PCM frames/close and report EOF or error.
2. Existing audio sink: reserve/release PSP channel, apply independent music volume, and output a 1024-frame buffer.

The WAV source remains the only implemented source in BG-13. Do not build a Spotify downloader, persist credentials, hook kernel networking, or dispatch Media Engine jobs during this refactor.

## Behavioral and safety requirements
- Keep file paths, numbering, looping, pause, next/previous, enabled state, mixer persistence and logs compatible.
- Keep 1024-frame 44.1 kHz stereo PCM output and reserved-channel/SRC fallback behavior.
- No filesystem I/O in display hooks; do not block graphics/input workers.
- Do not introduce new heap allocations or change the GAME PRX / EBOOT separation.
- Verify build with `pspdev/pspdev:latest` and test on PSP-3000 Metal Slug Anthology.
- If the game fails to start or the PRX is unstable, restore the saved PRX; do not modify unrelated firmware or game files.

## Later milestones (require separate hardware tests)
- BG-14: validate networking availability and resource contention inside GAME without requiring Spotify.
- BG-15: assess Spotify authentication/stream and decoder feasibility in GAME, especially Media Engine contention, available RAM, firmware modules and session security. Do not assume that copying `spotify/player.c` into a kernel PRX works.
- BG-16: wire track metadata and controls to a confirmed streaming source.

## Acceptance tests for BG-13
1. Plugin loads on PSP-3000, game boots and the menu opens/closes reliably.
2. WAV playback remains smooth simultaneously with game audio.
3. Next/previous, pause/play, music/game volume and enabled persistence behave as before.
4. Menu has no flicker, game controls resume normally after closing.
5. Disable/stop and PRX unload leave no stale audio channel or syscall hook.

## BG-13 hardware acceptance — 2026-11-19

- Branch: `feature/background-audio-prx`; source integration completed at `e87e797`.
- Built with the existing `pspdev/pspdev:latest` Docker Makefile setup and installed on PSP-3000 running ARK-5.
- Prior PRX preserved as `ms0:/seplugins/pspotify_bg/pspotify_bg.prx.bak_20261119_015150`.
- Metal Slug Anthology user hardware results: **4/4 PASS** — stable flicker-free in-game menu; music with game audio; music continues after closing menu; next/previous and pause controls work.
- BG-14 is the next milestone: investigate and validate network access from GAME without changing accepted audio/overlay behavior.
