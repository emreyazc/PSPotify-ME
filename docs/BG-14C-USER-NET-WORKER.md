# BG-14C — separate user-mode Wi-Fi companion (prototype)

## Status
Code and independent build target committed; **not yet compiled or hardware-tested**. It is intentionally **not loaded or linked** into the GAME kernel plugin. The accepted PCM/audio/menu build remains unchanged.

### Motivation
The standalone EBOOT initializes networking from user mode; the GAME plugin is a kernel PRX. `sceUtilityLoadNetModule` is specified as a user-mode entry point in the PSP SDK. A separate user-mode companion is therefore the next architecture boundary to test.

### Included implementation
- `net_worker.c`: user-mode `PSP_MODULE_INFO`, opt-in `net-worker-enable.txt`, own diagnostic file `net-worker.log`.
- Network module initialization (COMMON + INET), network, inet, resolver and APCTL initializers; tracks successfully acquired resources, does not terminate pre-existing resources.
- Reuses an existing GOT_IP connection without connecting/disconnecting it.
- Scans saved profiles 1–10; asynchronous connects are bounded to 15 seconds/profile with 100 ms polling and a stop flag, and only GOT_IP is accepted as success.
- If the worker initiates a connection, it owns its eventual disconnection.
- Stops and cleans up its own resources on module unload; if worker shutdown exceeds the bounded wait, unload refuses rather than force-terminating in the middle of networking code.
- No sockets, Spotify credentials, game hooks or modifications to audio/render/control code.

### Build only — no installation yet

```powershell
cd D:\Projects\PSPotify-ME
git pull --ff-only origin feature/background-audio-prx
docker run --rm `
    -v "D:\Projects\PSPotify-ME:/workspace" `
    -w /workspace/background/net_worker `
    pspdev/pspdev:latest `
    sh -c 'make -j4'
```

Expected output: `background/net_worker/pspotify_bg_net.prx`. A successful build does **not** prove that a user-mode module can be loaded alongside a particular game's user partitions, or that network initialization is safe in-game.

### Next integration checkpoint
Validate a user-mode PRX loader/owner mechanism, memory partition and lifecycle inside GAME with an opt-in canary, **before** loading this network worker. Never make loading automatic on boot, and do not deploy `pspotify_bg_net.prx` from this commit into the GAME plugin path.

After loader safety is established, validate this user worker without touching Spotify; test network startup and cleanup on actual PSP-3000 and ARK-5.

### References
- PSP SDK `psputility_netmodules.h`: `sceUtilityLoadNetModule` user-mode contract
- PSP SDK `pspnet_apctl.h`: `sceNetApctlConnect`, `sceNetApctlGetState` / GOT_IP
- `main.c`: existing user-mode PSPotify network initialization pattern
