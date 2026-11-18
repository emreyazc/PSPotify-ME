# BG-14D — opt-in user PRX load/unload canary

Status: coded, **not yet built or PSP hardware tested**.

This stage is a separate **module-loading** test. It does not load `pspotify_bg_net.prx` and does not start WLAN, IP, or Spotify traffic. The real network worker remains separately built but unintegrated.

## Changes
- `background/bg_user_loader.c/.h` — GAME kernel worker checks `user-loader.txt` for exact first byte `1`. Default OFF. If enabled, it looks only for `ms0:/seplugins/pspotify_bg/bg_user_canary.prx` and calls the PSP module manager to load it into user partition 2, then starts it. Attempts cleanup on failures, and stop/unload when GAME BG terminates. Does not touch rendering or audio paths.
- `background/user_canary/` — a minimal user-mode PRX with `module_start/module_stop`, writing `BGCANARY:` markers to `user-canary.log`, no networking, new threads, or credentials.
- The loader writes `BGLOAD:` lines to existing `status.log`. No automatic retry.
- `net-probe.txt` continues to control the separate passive APCTL probe. Prefer setting it to `0` during the canary test to reduce noise.

## Compile both PRXs

```powershell
cd D:\Projects\PSPotify-ME
git pull --ff-only origin feature/background-audio-prx

docker run --rm `
  -v "D:\Projects\PSPotify-ME:/workspace" `
  -w /workspace/background `
  pspdev/pspdev:latest sh -c 'make -j4'

docker run --rm `
  -v "D:\Projects\PSPotify-ME:/workspace" `
  -w /workspace/background/user_canary `
  pspdev/pspdev:latest sh -c 'make -j4'
```

Do not deploy if either build fails.

## PSP-3000 staged acceptance

1. Preserve the previous GAME PRX. Copy the new `background/pspotify_bg.prx` to `ms0:/seplugins/pspotify_bg/pspotify_bg.prx`, and `background/user_canary/bg_user_canary.prx` to `ms0:/seplugins/pspotify_bg/bg_user_canary.prx`.
2. First boot with **no `user-loader.txt`** (or content `0`): verify Metal Slug Anthology still plays with menu and music normally.
3. Then put ASCII `1` into `ms0:/seplugins/pspotify_bg/user-loader.txt`, fully exit/restart the game, verify game/menu/audio again.
4. On USB, check `status.log` for `BGLOAD:` and `user-canary.log` for `BGCANARY:`. Both start and stop are needed to accept load/unload.
5. Restore `user-loader.txt` to `0` after testing. If game boot stalls, remove that flag and restore your saved previous GAME PRX. A user-mode PRX loaded into game user memory may conflict with some titles.

## Acceptance boundaries

Pass only if: user canary starts, runs in GAME, game and audio remain stable, and canary stops/unloads on returning to XMB; otherwise preserve exact error logs and mark inconclusive/fail. Logs from two files may differ if the game abruptly terminates.

**No automatic network-worker loading is authorized by this test.** Once this is hardware-validated, a separately opt-in BG-14E may replace the canary with `pspotify_bg_net.prx` and test IP acquisition and resource ownership.

## Hardware result — 2026-11-19

PSP-3000 / ARK-5 / Metal Slug Anthology, `user-loader.txt=1`:

- Game launch, interactive play, BG music and overlay were reported working.
- `status.log`: `BGLOAD: loading user-mode canary into user partition`, `BGLOAD: user-mode canary started`.
- `user-canary.log`: `BGCANARY: user-mode module started`.
- No `BGLOAD: user-mode canary unloaded` or `BGCANARY: user-mode module stopped` in the supplied output after normal game exit.

**Result:** user-mode module load/start **PASS** on hardware. Stop/unload **NOT VERIFIED**. Do not imply a clean shutdown from successful game exit. Investigate why GAME kernel PRX worker teardown did not produce explicit unload markers before attaching the network worker (which owns networking resources). Keep automatic network worker loading disabled.
