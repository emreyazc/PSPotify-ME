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

## Archive View and list artwork implementation

The native PSP UI owns render state in `ui/ui.h` / `ui/xmb.c`; `spotify/ui.c` owns playlist and track metadata. `PREF_DENSITY = UI_DENSITY_ARCHIVE` is a fourth list style serialized as `list_style = archive`. It uses the existing `ui_state.sel[category][level]` and list rows: indices 0–9 draw in column one, 10–19 in column two, with each 20-item page determined by `selection / 20`. This preserves the existing **Up/Down sequential navigation**, **Left/Right category switching**, and opened-playlist level. Archive selection, list names and footer information reuse existing data; the per-track in-memory row buffers are not expanded to cache album names.

`PREF_LIST_COVERS` persists as `list_covers = on/off`; effective thumbnail work is disabled if it is Off **or** if Archive View is active. The thumbnail worker checks this state, clears unused textures on transitions and avoids queueing new work. The now-playing cover thread remains independent. The mini-player cover and blurred cover backdrop are suppressed in Archive View; theme wallpapers and Now Playing artwork are not changed.

The UI core treats SELECT as a quick jump only when a track is active and the current list does not already reserve SELECT for moving/reordering. While moving queue rows or Settings category rows, existing SELECT semantics take precedence. The shortcut saves a category ID; individual per-category level, selection and scroll fields never change during the jump, so they naturally survive a return from Now Playing. It is intentionally available for all list styles, not only Archive View. The music-playing state is independent of browsing focus.

For behavior and control documentation see [FEATURES_EN.md](FEATURES_EN.md) and [FEATURES_TR.md](FEATURES_TR.md). The change still needs hardware acceptance testing; static compilation is not hardware proof.

## Verification scope

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
