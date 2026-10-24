# Media Engine implementation notes

PSPotify ME runs two kinds of work on the Media Engine through the same
dispatcher: the original two-operand self-check (`me_loop`, kept because
`scripts/verify_artifacts.py` validates its code generation) and the Ogg
Vorbis frame decoder (`me_vorbis_task`). This document covers how tasks reach
the ME and the cache rules both follow.

## Vorbis decode on the ME

- Setup (`stb_vorbis_open_pushdata`) runs once on the SC, because it needs
  libm. After that the decoder state is written back and invalidated on the
  SC, and the first ME job invalidates it on the ME. From then on only the ME
  touches it.
- Each job carries an input window of the decrypted stream. The SC writes the
  window back before dispatch, and the ME invalidates it before reading. The
  ME then decodes one frame, converts it to interleaved s16 in one of two
  64-byte-aligned PCM buffers, writes the PCM and the job back, and publishes
  DONE.
- `spotify/me_trampoline.S` runs the task on a 64 KB stack of its own. It
  sets Status.CU1 and clears FCSR for the call, and restores both afterwards:
  the firmware dispatcher guarantees neither a large stack nor a usable FPU.
- The SC pipelines the work: it plays frame N while the ME decodes N+1.
- A timeout marks the app unsafe to exit (reboot only).
- `verify_artifacts.py` rejects any `syscall` or `$gp` use in stb_vorbis,
  in memcpy/memset, or in the task itself.
- Dispatching our own ME task breaks the firmware's `sceMp3` decoder, which
  runs on the ME too. The app therefore uses no firmware codec while ME
  tasks are in use.

## CPU → ME → CPU protocol

The user EBOOT retains the normal main CPU UI and PSP APIs. A small embedded
MIT upstream kernel PRX performs privileged SC callbacks and model detection.
`sceKernelGetModel` lives in that PRX, never as a kernel-only user-EBOOT import.
Removing the baseline `pspkernel` link prevents kernel-only import tables from
being selected for user-mode application and probe binaries.

The selected upstream Classic mechanism patches three image-specific firmware
sites. The invalid custom syscall index dispatches `Task.func(Task.param)` on
ME. Code and task storage remain in lower user **system RAM**; they are not
placed in ME local EDRAM. Both are checked at runtime against the shared
`08800000..0A000000` range. The PRX relocator establishes the function address
before startup; `-G0`, no PIC and no LTO prevent reliance on the SC global
pointer or cross-file constant propagation into the task.

`SharedTask` occupies exactly one aligned 64-byte cache line in static storage.
CPU fills magic/version, both signed 32-bit operands, a changing nonzero job
sequence, READY, an invalid result and zero marker. ME invalidates the line,
checks the protocol, publishes RUNNING, loads both operands, checks signed
overflow, performs addition and publishes echoed inputs/result/marker/sequence
and DONE. UI is entirely on SC. CPU verifies all these fields after firmware
has cleared the actual busy flags. It never invokes the task function itself
or substitutes a fallback result. Expected constants are only validation
values on CPU.

## Cache ownership

| Boundary | Operation | Reason |
|---|---|---|
| Initial privileged callback | SC D-cache writeback-all, once | Publish relocated kernel callback code before bridge execution |
| ME activation | SC D-cache writeback-all, once | Publish task/wrappers, mapping and patch data before the patched getEDRAM invalidates ME I-cache |
| Per-job SC submission | `sync`; SC writeback-invalidate of 64-byte task | Put both operands in RAM and remove dirty SC copies |
| ME begins | Mapped ME D-cache invalidate of task; `sync` | Remove previous job's cached input/output |
| ME RUNNING/finish | `sync`; mapped ME writeback of task | Make progress/results visible before firmware completes |
| SC observation | SC D-cache invalidate of task; `sync` | Read ME's published values rather than old sentinels |
| Firmware busy polling | SC invalidate of upstream's separate 64-byte status line | Observe actual ME return without changing busy flags |
| Teardown | ME restore/writeback of saved instructions and ME I-cache invalidation; SC invalidate afterward | Remove pointers into the application before it is unloaded |

No job mixes cached and uncached aliases. Volatile preserves individual protocol
loads/stores; `sync` with a compiler memory clobber orders them. One producer owns
the task at a time; CPU never writes it while ME may still run. A timeout is not
cancellation, so the app retains the object and disables additional dispatches.

The one-time all-cache publication covers relocated application/library code
whose complete runtime extent is not exposed by this framework. Individual jobs
use range operations only. Upstream's kernel bridge also invalidates SC I-cache
when switching to kernel callback code; that existing behavior is preserved.

## Dependency patches

`patches/safe-task.patch` is applied only to the pinned MIT upstream revision.
It retains upstream dispatch/wrappers/cache mapping and adds:

- Real kernel-call return propagation for Classic initialization and dispatch;
  exact PRX file/load errors, truncation when writing the embedded PRX.
- A bounded readiness API that only observes busy state; no flag clearing, no
  forced success, and no leaking interrupt suspension on timeout.
- Bounded DDR flush in Classic submission; two-second job completion timeout.
- Kernel PRX model export and installation of the required nested bridge header.
- Saving/restoring the original six firmware instruction words via a cleanup
  task on ME, so both ME D/I-cache changes occur on the right processor.
- Portable PRX embedding and correct build dependencies for regenerated stubs.

The app never uses the legacy `meSafeTaskWaitReady` or the unchecked upstream
module-load helper. It checks AVCODEC loading and EDRAM acquisition/release
directly. Initialization after patches but before cache activation, failed
dispatch, timeout and failed cleanup leave storage resident and offer reboot.
The firmware core, RPC completion, image mappings and teardown remain real
hardware assumptions that cannot be established by compilation.

`patches/custom-core-build.patch` removes stdout redirection from a tool that
already writes its own stub file, regenerates imports when exports change and
uses the same portable PRX embedding helper. This avoids Windows file-sharing
and shell-quoting failures when generating the dependency archives.
No firmware mapping is invented or changed. Exception handlers, custom reset
cores, VME, DMACPLUS, task threads and pools are not included in the application.

## Constant browse geometry and Turkish typeface trials

`ui/xmb.c:ui_draw()` allocates the same bottom viewport for each browsing category regardless of `ui.now.active`; only the standalone Now Playing category uses the full canvas. Archive reserves 44px, other browsers 34px. `draw_archive_mini()` and `draw_mini()` render an inert *Nothing playing* message in the space when playback is inactive, avoiding layout shrink/expand at the moment a track starts. Regular playing-state mini UI is unchanged.

`scripts/ui_assets.py` selects `PSPOTIFY_FONT=inter|noto|mplus`, default Inter. Inter/Noto variable TTF files are fetched from immutable Google Fonts Git revision and validated against pinned **Git blob SHA-1 hashes**; source OFL licenses are included. The Python fontTools subset pipeline first limits Unicode points to existing PSP glyph slots (including Turkish) and then instantiates full static **400/700/800** weight faces (`opsz=14` for Inter; `wdth=100` for Noto). The PSP only embeds one chosen family, retaining the existing font atlas budget. `scripts/build_preview.py` outputs separate static weights to `.deps/fonts/preview-fonts`, and `tools/preview/main.c`/the web preview preload those same files. Switch font candidates using the environment variable and rebuild rather than carrying both families in PSP RAM. **Hardware acceptance (PSP-3000):** fixed browse geometry `FIX-01..02` and default Inter font readability/Turkish glyphs `FONT-01..02` passed; Noto Sans and fallback M PLUS are supported build options but were not tested in this round.

## Archive View and list artwork implementation

The native PSP UI owns render state in `ui/ui.h` / `ui/xmb.c`; `spotify/ui.c` owns playlist and track metadata. `PREF_DENSITY = UI_DENSITY_ARCHIVE` is a fourth list style serialized as `list_style = archive`. It uses the existing `ui_state.sel[category][level]` and list rows: indices 0–9 draw in column one, 10–19 in column two, with each 20-item page determined by `selection / 20`. This preserves the existing **Up/Down sequential navigation**, **Left/Right category switching**, and opened-playlist level. Archive rendering includes Liked Songs, Recently Played, Playlists, My Playlists and opened playlists; only Search, Queue and Settings retain XMB row rendering. Archive rows use existing `ui_row` metadata. The former 24px selected-item footer is removed; the header shows the list/category title and `archive_total` (the actual Spotify count from `liked_count` or `open_total_count`, falling back to the loaded count) alongside the current cursor position. The regained space increases row height while preserving two columns of 10 items, using 9px (fallback 8px) text centered in the highlight. No new per-track metadata buffers are added.

`PREF_LIST_COVERS` persists as `list_covers = on/off`; effective thumbnail work is disabled if it is Off **or** if Archive View is active. The thumbnail worker checks this state, clears unused textures on transitions and avoids queueing new work. The now-playing cover thread remains independent. The *list-row* covers and blurred cover backdrop are suppressed in Archive View, but its new 44px mini-player **reuses the same `ui.now.cover` texture already downloaded independently for Now Playing**. This displays one small album image without invoking the list-thumbnail worker, even when the saved list-covers preference is Off. It also displays a single two-line title/artist, play/pause glyph, time and progress bar. Other list modes keep their original 34px mini-player. The separate Now Playing screen is now refreshed in the Classic layout only: 156px album cover at left, clearly separated title/artist/source/Next: rows at right, seek bar and elapsed/total labels above the playback controls. The selected XMB category icon was replaced across screens with horizontally scrollable 26px compact tabs. Categories still come from `cat_order` and are moved with the existing `cat_pos` animation; each tab has a text/icon width derived from the current language, and the active tab uses the theme accent pill. Navigation, content scroll, mini-player rules and playback input mappings are unchanged. Small tab font atlases are pre-baked to avoid stalls on the PSP.

The UI core treats SELECT as a quick jump only when a track is active and the current list does not already reserve SELECT for moving/reordering. While moving queue rows or Settings category rows, existing SELECT semantics take precedence. The shortcut saves **category, opened level and selected row index** as a fallback while Now Playing is open. On SELECT return, `ui/xmb.c` restores the saved cursor, emits `UI_EV_QUICK_RETURN`, and `spotify/ui.c` searches the returned list's currently loaded rows for the **actually playing** `queue[current_index].id`. It does not rely on `queue_key`, source-generation counters or `request_index`: those can change independently of the actually playing song and previously caused the selection to remain on the wrong row. If a matching track is visible, both `sel` and `scroll` jump to it; otherwise the prior cursor remains. Duplicate track IDs resolve to the nearest matching row. The shortcut also suppresses the otherwise automatic Recently Played re-entry refresh once, preventing the history list and cursor from resetting on a simple SELECT round-trip. Normal navigations to Recently Played still refresh as before. ID matching—not queue position—is necessary because local sorting and shuffle reorder tracks. It is intentionally available for all list styles, not only Archive View. The music-playing state is independent of browsing focus.

For behavior and control documentation see [FEATURES_EN.md](FEATURES_EN.md) and [FEATURES_TR.md](FEATURES_TR.md). The change still needs hardware acceptance testing; static compilation is not hardware proof.

## Interface localization

The user-facing UI uses English canonical strings in `ui/xmb.c:ui_tr()` so preference storage, Spotify metadata and themes remain independent of the selected language. `spotify/prefs.c` provides canonical labels and option names; these are translated at the `spotify/ui.c` rendering boundary. `spotify/ui.c:on_status()` now translates the session statuses from `spotify/session.c` without changing the actual authentication protocol. The first-run device pairing screen also looks up its text at draw time and bounds the result to the PSP's 480px width. The Now Playing 'Next:' prefix is localized at render time.

Static source auditing covered literal `ui_tr()` calls, session status literals and user-visible preference names. There are 212 distinct keys with no duplicate English keys in the shared dictionary. Formatted strings are selected through existing English/Turkish format branches where necessary; track and playlist titles remain opaque UTF-8 from Spotify. Preferences continue to write stable language-independent tokens (`language = en/tr`, `list_style = archive`, etc.). Native PSP error/debug output in `main.c` occurs before the full UI/settings bootstraps and is **not** covered by this translator.

## Verification scope## Verification scope

The final local build uses GCC 15.2.0, Windows CMake/Ninja and the MSYS2-hosted
[pspdev-win v2](https://github.com/dmang-dev/pspdev-win/releases/tag/v2) toolchain.
Its ZIP SHA-256 was verified:
`037f902acd90cfcf4711aeee6462479a6d0eabee6faf93ff44fe1081e653d512`.

`scripts/verify_artifacts.py` checks ELF/PBP/SFO/PRX structure, shared alignment,
actual optimized ME loads from offsets 12/16 and register addition/store to 20,
absence of ME syscalls or `$gp`, absence of a direct CPU call to `me_loop`, and
absence of kernel-only imports in the user EBOOT. It saves disassembly and a
JSON report beside the unstripped ELF/map/PRX and kernel bridge ELF. These are
static checks, **not a simulation or hardware result**.

Application warnings are errors. Third-party SDK/library headers are system
headers; their legacy prototypes are not edited or disguised as app warnings.
Upstream prints an informational warning that its kernel PRX is uncompressed;
that unsigned PRX is expected to be loaded by the target CFW. No encryption or
ARK-specific patch is added. CI uses a dated PSPDEV image and fixed ME commits.
