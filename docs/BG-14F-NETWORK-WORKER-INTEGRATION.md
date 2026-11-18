# BG-14F — User-mode network companion integrated behind two explicit flags

Status: **committed, not yet compiled or tested on PSP hardware**. BG-14E user-mode canary load/stop/unload passed on hardware with correct `PSP_EXPORT_FUNC(module_stop)`. This does **not** establish that networking works in-game.

## Runtime gates (both required)
- `ms0:/seplugins/pspotify_bg/net-loader.txt`: ASCII first byte `1` to enable GAME loading.
- `ms0:/seplugins/pspotify_bg/net-worker-enable.txt`: ASCII first byte `1` to allow the worker's actual network calls.
- `user-loader.txt`: **0**. If canary and network loader are both requested, both are refused.
- `net-probe.txt`: **0**, to isolate network behavior.

Default without flags is OFF; the preexisting BG GAME music/overlay remain active and independent of networking. Module paths are `pspotify_bg.prx` (GAME) and `pspotify_bg_net.prx` (user companion) under `ms0:/seplugins/pspotify_bg/`.

The GAME worker waits ~4s after startup, then loads `pspotify_bg_net.prx` into user memory, starts it, and schedules a **one-shot stop/unload after 45s** of GAME runtime (not 45s after launch). It avoids performing module operations while the existing game pause mechanism is active.

The user worker:
- Loads network modules and initializes PSP Net/Inet/Resolver/APCTL, tracking resources it believes it owns.
- If APCTL has GOT_IP already, does not connect again. If APCTL is in another non-disconnected state, refuses to take over.
- Uses the WLAN hardware switch; tries saved infrastructure profiles 1–10 in order, at most 15s each; a 45s stop may interrupt this sequence. Success is only `PSP_NET_APCTL_STATE_GOT_IP`.
- Logs to `net-worker.log`, including which saved profile it attempts; no credentials, Spotify request, audio streaming or account login.
- When stopped, requests worker thread to exit, waits up to 3s, and declines unload if stop fails. Resource teardown logs are diagnostic; compare with module manager logs.
- This is a real network initialization experiment and may conflict with a game's network use. Test only offline/local single-player title.

## Build
```powershell
cd D:\Projects\PSPotify-ME
git pull --ff-only origin feature/background-audio-prx
docker run --rm `
  -v "D:\Projects\PSPotify-ME:/workspace" `
  -w /workspace/background `
  pspdev/pspdev:latest sh -c 'make -j4'
docker run --rm `
  -v "D:\Projects\PSPotify-ME:/workspace" `
  -w /workspace/background/net_worker `
  pspdev/pspdev:latest sh -c 'make clean && make -j4'
```

Do not replace working PRX files unless both builds succeed. Keep time-stamped backups of the existing GAME PRX; compare copied hashes. Install `background/pspotify_bg.prx` and `background/net_worker/pspotify_bg_net.prx`.

## Acceptance
1. On PSP with Wi-Fi hardware switch ON, use a known PSP-compatible saved profile (e.g. previously tested phone hotspot). Ensure `user-loader.txt=0`; enable `net-loader.txt=1` and `net-worker-enable.txt=1` only for this test.
2. Run Metal Slug Anthology with normal music and overlay. Remain in game **at least 60 seconds after music is available** without using experimental GAME pause.
3. Exit normally, USB connect, collect last ~35 `BGLOAD:` lines from `status.log` and last ~50 lines from `net-worker.log`.
4. Immediately return both network flags to `0`. If a freeze occurs, disable flags and restore backed-up GAME PRX as needed.

Expected for lifecycle: `BGLOAD: network PRX started`; `BGLOAD: scheduled network shutdown requested`; stop API and status >=0; `BGLOAD: network PRX unloaded`. Worker should report initialization, profile attempt/GOT_IP or exact failure code, then `BGNET: companion stopped and owned resources released`.

Do not claim Wi-Fi connectivity from PRX loading alone. Do not enable Spotify streaming until a separate in-game successful GOT_IP and clean owned-resource cleanup have been demonstrated. Failure to unload or missing cleanup markers is **FAIL/INCONCLUSIVE**, requiring code revision.
