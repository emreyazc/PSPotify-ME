# BG-14G — Independent network shutdown supervisor and audio diagnostics

Status: code committed; not built or verified on PSP hardware.

## Why
BG-14F established APCTL GOT_IP in Metal Slug Anthology, after saved profile 1 failed and profile 3 succeeded. However the supplied logs did not show the network PRX shutting down. A later GAME session had no BGLOAD startup at all and ended with `BG: audio output failed`. That later session is **not** proof that the network stop timer failed: flags may have been OFF. The first network session likewise did not contain the 45-second stop marker, so the cleanup remains unverified.

## Changes
- Network-only 45-second **kernel supervisor thread**, separate from the BG audio thread. It is launched only after the network PRX has successfully started, using the existing dual flags (`net-loader.txt` and `net-worker-enable.txt`). Canary's existing 8s worker-driven test remains unchanged.
- The network supervisor records `BGLOAD: independent network shutdown requested` then calls the same verified `sceKernelStopModule` / `sceKernelUnloadModule` lifecycle code, protected by a stop semaphore. Supervisor startup failures result in immediate attempted network PRX shutdown rather than unbounded operation.
- GAME teardown requests supervisor exit, joins it (up to 4 seconds), and then performs normal module cleanup. Failed join is logged and no forced termination is attempted. The previous GAME PRX unload caveat still applies; explicit in-game shutdown is preferred.
- In GAME audio code, `BG: audio output failed channel=PCM|SRC code=0x........` reports the exact output error instead of a generic message. It still disables playback on an error, as before.
- No Spotify traffic, account details or authentication. No changes to rendering or input capture.

## Targeted hardware test
1. Build only the GAME PRX (`background`) and back up the PSP's current `pspotify_bg.prx`. Reuse the already-built network companion from BG-14F; its code has not changed for BG-14G.
2. For a SINGLE test boot, `user-loader.txt=0`, `net-probe.txt=0`, `net-loader.txt=1`, `net-worker-enable.txt=1`. Ensure the PSP Wi-Fi hardware switch is ON and a PSP-compatible saved connection exists.
3. Open Metal Slug Anthology. Start music. Remain in the GAME for 70 seconds after music has begun; this exceeds the independent 45s timer. Do not use experimental GAME pause. Exit to XMB.
4. Collect last 30 lines matching `BGLOAD:` from `status.log`, last 35 lines of `net-worker.log` and any `BG: audio output failed` entries from the last GAME session. Afterwards restore both network flags to `0`.
5. **PASS** only if: `BGLOAD: independent network shutdown requested`, `BGNET: companion stopped and owned resources released`, `BGLOAD: stop API return 0x00000000`, `BGLOAD: stop module status 0x00000000`, `BGLOAD: network PRX unloaded`. Log missing or other errors is NOT PASS. Internet reachability is not established by GOT_IP alone.

## Recovery
If GAME becomes unstable: turn both network flags OFF, revert to the timestamped previous GAME PRX, and preserve logs. This is still experimental resource management and may disrupt games using their own networking.
