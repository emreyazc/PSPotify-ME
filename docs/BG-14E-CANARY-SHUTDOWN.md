# BG-14E — Canary lifecycle test while GAME is running

Status: implemented, **not yet compiled or hardware verified**.

## Motivation
BG-14D demonstrated that the PSP-3000 can load and start a separate user-mode canary PRX while Metal Slug Anthology is running, with stable game/music/menu. Normal return to XMB produced no `BGLOAD: ... unloaded` or `BGCANARY: ... stopped` logs, so unload has **not** been established. Relying solely on GAME PRX `module_stop` is insufficient.

## Implementation
- In `background/bg_user_loader.c`, an opted-in successful canary startup schedules one shutdown attempt 8 seconds later, while the BG audio worker is still running.
- `bg_user_loader_poll()` runs on the existing worker, skips calls when game threads are paused, and invokes the existing `bg_user_loader_stop()` at most once.
- No new thread, network initialization, Spotify authentication, graphics hook, audio sink change or automatic network worker loading.
- Existing GAME shutdown cleanup remains as a failsafe. A failed stop/unload is reported and leaves the module resident; **do not treat a failed stop as safe to load the actual network worker**.

## Build and test
1. Build the GAME PRX (`background`) with Docker; existing `bg_user_canary.prx` need not be rebuilt.
2. Backup existing installed PRX before replacing it, copy only the rebuilt `pspotify_bg.prx`.
3. With PSP unplugged from USB, set ASCII `1` in `ms0:/seplugins/pspotify_bg/user-loader.txt` before starting the game. Keep `net-probe.txt=0`.
4. Open Metal Slug Anthology and keep the game running for at least 15 seconds after BG menu/music is available (worker startup itself waits about 4 seconds). Then exit normally.
5. Read `status.log` filtered to `BGLOAD:`, and `user-canary.log` filtered to `BGCANARY:`.
6. Return `user-loader.txt` to `0` after the test.

Expected:
- `BGLOAD: user-mode canary started`
- `BGLOAD: scheduled canary stop/unload after 8 seconds in GAME`
- `BGLOAD: scheduled canary shutdown requested`
- `BGCANARY: user-mode module stopped`
- `BGLOAD: user-mode canary unloaded`

If shutdown markers do not appear, mark **FAIL/INCONCLUSIVE** and investigate before real networking integration. The mere presence of `scheduled canary shutdown requested` is not evidence of clean unloading.
