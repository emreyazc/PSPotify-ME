# BG-14 — Passive in-game Wi-Fi diagnostic

Status: staged for Docker build and PSP-3000 test; **not hardware verified**.

Scope: `background/bg_net_probe.c` checks whether an APCTL export is already present in GAME and, if so, reads its current state. This is not a network initialization or Spotify streaming implementation.

## Safety
- Disabled by default. To enable, create `ms0:/seplugins/pspotify_bg/net-probe.txt` with the single character `1`.
- Do not edit EBOOT, game files, or `pause-game.txt`.
- No `sceNetInit`, `sceNetApctlInit`, `sceNetApctlConnect`, `sceNetApctlDisconnect`, or socket operations are called.
- The diagnostic queries at most once every 10 seconds. It logs state changes only.
- If the APCTL export cannot be resolved, the probe logs once and stops trying for the session.
- An unavailable APCTL module is expected when a game does not initialize networking: this does not prove Wi-Fi hardware cannot be used.
- For stability testing, start with `net-probe.txt` absent, then opt in during a second launch. If anything hangs, restore the previous saved PRX and remove `net-probe.txt`.

## Test
1. Docker build with `pspdev/pspdev:latest` using `make -j4` in `background`.
2. Preserve the currently accepted PRX before copying the new one.
3. Start Metal Slug Anthology, verify old menu/music/controls with probe OFF.
4. Put `1` in `net-probe.txt`, fully restart game, repeat old tests.
5. Read `ms0:/seplugins/pspotify_bg/status.log`, filtering `BGNET:` lines. Result may be `APCTL export unavailable`, `APCTL state not connected`, or `GOT_IP`.
6. Report both network state and whether game/music remained normal.

**Interpretation:** Passive APCTL state does not establish whether a background PRX can safely initialize Wi-Fi or stream Spotify while a game runs. Those must be separately designed and tested.

## PSP-3000 hardware result — 2026-11-19

- PSP-3000 / ARK-5 / Metal Slug Anthology: **game, menu, concurrent music and stability PASS** with `net-probe.txt=1`.
- `status.log` (twice): `BGNET: passive probe enabled; no connection will be started`, followed by `BGNET: APCTL export unavailable; passive probe disabled`.
- **Interpretation:** No APCTL export was resolved via the diagnostic's two candidate module/library names. This does **not** establish that networking is impossible in a GAME PRX; networking is not initialized by this probe. Verify loaded module identity, privilege boundary and initialization/ownership before any connection implementation.
- Next: a separately gated, bounded networking test; do not alter the accepted audio, input or framebuffer hooks.

## BG-14B: kernel/user boundary and next networking work package

**Why we do not call `sceUtilityLoadNetModule` from the existing GAME kernel PRX:** PSP SDK documents that this utility entry point loads network PRXs **from user mode**. BG's `PSP_MODULE_INFO(..., 0x1000, ...)` makes it a kernel PRX, whereas `main.c` in the standalone PSPotify EBOOT is a user-mode application. Blindly linking or calling the user networking initialization functions from BG could break game loading or collide with a game's own network stack.

BG-14B performs a one-time, opt-in preflight inventory of loaded `sceNet*` modules and searches multiple APCTL module/library export name combinations. It never initializes, tears down or connects networking and never modifies game threads.

**Next integrated implementation boundary:** a bounded *user-mode network worker* for infrastructure initialization and profile fallback, with a narrow command/status interface to the kernel BG PRX. The worker must be checked against PSP game module loading/partition constraints before deployment. It should (1) detect existing GAME network ownership and never terminate another owner's modules, (2) initialize only missing services, (3) try saved profiles with bounded GOT_IP waits, (4) report error codes/states, (5) disconnect/terminate only resources it acquired and (6) leave all existing audio/overlay/input code unchanged. This architecture is a design goal, **not implemented** by BG-14B.

### BG-14B quick test

Keep `net-probe.txt=1` and build/copy the new PRX with a backup as before. Open Metal Slug Anthology, verify menu and music remain normal, exit and filter `status.log` for `BGNET:` to inspect the module inventory and APCTL result. Output `ABSENT` for all network modules is expected in a game with no network initialization.
