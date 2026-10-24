# PSPotify ME — Features and User Guide (English)

> This document describes validated features on `master` plus a new player-layout experiment on `feature/player-ui-polish`. The new layout has not yet been tested on a PSP. Turkish version: [FEATURES_TR.md](FEATURES_TR.md).

## Music library and navigation

- **Liked Songs:** Browse and play saved tracks from your Spotify library.
- **Playlists:** Browse playlists, sort by Spotify order/name/number of tracks, and pin or unpin playlists through the Triangle menu.
- **My Playlists:** Separately browse playlists owned by your account, alphabetically by default with pinned playlists first.
- **Inside a playlist:** Browse and play its tracks without altering the playing queue when merely navigating.
- **Search:** Square opens the Spotify search keyboard.
- **Recently Played:** Real track history, newest first, refreshed when revisited; server updates may take some time to appear.
- **Queue:** Add and remove tracks; reorder upcoming tracks with SELECT and Up/Down.

## Language support: English and Turkish

Select **English** or **Türkçe** in `Settings > Appearance > Language`. The choice persists as `language = en/tr` in `settings.ini`. Application category names, settings, hints, notifications, error messages, list details and sign-in/device-pairing screens follow the selected language. Spotify-supplied track, artist, playlist and theme names are never translated. The PSP's built-in search keyboard also depends on the PSP system language.

24/12-hour display choices, 25/50/75/100 songs-per-load choices and the **Next:** label on Now Playing are localized. The update mechanism itself is unchanged; only its user-facing status texts have been translated.

**Audit and limits:** The shared UI dictionary contains 212 unique entries. A static check found no unmatched `ui_tr()` literal, settings choice or session sign-in status. This is not a substitute for real-device testing of dynamic strings. Low-level PSP/Media Engine diagnostic and recovery screens shown **before the application UI is initialized** remain in English; the saved language may not yet be loaded there.

**Short device tests:** (1) Switch to Turkish, restart, and verify About, Now Playing and songs-per-load settings. (2) Pairing instructions appear only on first sign-in; do not delete credentials solely to test translations. (3) Switch to English and confirm these screens have no untranslated labels or clipping. (4) Try unusually long Turkish playlist names.

## List styles and artwork

`Settings > Appearance > List style`:

| Option | Behavior |
|---|---|
| Comfortable | Larger standard rows |
| Compact | Smaller standard rows |
| Text only | Single-column text rows with no artwork |
| **Archive View** | **Up to 20 entries per page, two columns of ten, no list artwork** |

**Archive View** uses the same compact visual style in Liked Songs, **Recently Played**, Playlists, My Playlists and within playlists. Entry numbers 1–10 appear in the left column and 11–20 on the right. Advancing the selection beyond the last entry of a 20-item page displays the next page. **Up/Down keep their existing sequential selection behavior. Left/Right still change categories**—they are not reassigned to column jumping.

The selected row remains highlighted. **The old selected-item footer has been removed**, so the same playing-song information no longer appears twice. The archive heading now shows the list/category name, actual total track or playlist count, and cursor position (e.g. `50 songs · 1/50`). Open playlists show their name here. A single **Now Playing mini-bar** contains song title, artist, play/pause status, progress, time and the existing Now Playing cover. No mini-bar appears when nothing is playing.

`Settings > Appearance > List artwork` toggles thumbnail artwork and persists as `list_covers = on/off`. When Off, the thumbnail worker stops queueing small **list** artwork, releases the thumbnail cache and omits the cover on the standard mini-player.

**Archive View always hides per-row list artwork**, regardless of the separately remembered artwork preference. The Archive mini-player reuses the **already-loaded Now Playing cover** without making thumbnail download requests (or draws a color placeholder until available); the list-artwork preference stays saved for standard list styles. The blurred album-cover backdrop is still suppressed outside Now Playing in Archive mode; theme wallpapers and the full Now Playing screen remain unchanged.

Search, Queue and Settings retain their previous row layout rather than adopting two columns. Archive View now also covers Recently Played, so the same two-column layout is used in all five browsing contexts. Small artwork downloads remain suppressed everywhere in Archive mode.

## SELECT shortcut: Now Playing ↔ previous browsing position

When a track is active, press **SELECT** while browsing to open **Now Playing**. Press **SELECT** again to return to the previous category and playlist depth. If the **actually playing track ID** occurs among the returned list's currently loaded visible entries, the cursor follows that track (including L/R track changes): if the playback indicator is on track 7, the selection is on track 7 too. Otherwise the cursor saved before SELECT is restored. The implementation matches track IDs rather than mutable playback source keys or queue positions. In Recently Played, this shortcut does not trigger an unnecessary history refresh that would reset the cursor.

- The shortcut does nothing if there is no active track.
- **Queue** and **Settings > Categories** retain their existing SELECT-to-move behavior; dragging/reordering takes priority over the shortcut.
- **START** is still the exit key.
- The shortcut works across **all list styles**, not only Archive View.
- **Recently Played** (history) and **Now Playing** (current player) are different destinations.

## Existing controls

| Button | Browsing/lists | Now Playing |
|---|---|---|
| Up / Down | Previous / next entry | Volume |
| Left / Right | Previous / next category | Change category |
| Cross | Open / play | Cycle repeat |
| Circle | Go back a playlist level | — |
| Square | Play/pause; search keyboard in Search | Play/pause |
| Triangle | Options; pin a playlist at its top level | Shuffle |
| L / R | Skip or hold to seek, according to settings | Same |
| SELECT | Now Playing ↔ previous list; keep original move behavior in Queue/Settings Categories | Return to previous list |
| START | Exit safely | Same |

The confirm/back keys can be swapped in Settings, without changing their configured semantics.

## Sorting, pagination and limits

- `Settings > Layout > Sort songs`: **Newest first (default), Oldest first, Title A-Z, Artist A-Z**. The discontinued *Spotify order* applies to **song sorting only**; playlist sorting still has it.
- `Settings > Layout > Songs per load`: **25 / 50 / 75 / 100**. Next batches load as the user approaches the end of currently visible tracks.
- Sorting is local UI behavior; it does not reorder Spotify playlists remotely.
- The in-memory track ceiling for an individual list remains **500 tracks**. Archive View changes presentation, not that limit.

## Playback, sound and Now Playing

The app streams Ogg Vorbis and uses the PSP Media Engine for decoding. Playback includes pause/resume, next/previous, seek, shuffle, repeat, queue editing, saving/removing Liked Songs, audio normalization, EQ presets and custom five-band EQ. Now Playing offers Classic/Centered/Text only layouts, an optional visualizer and track metadata. When enabled, **Cover colors** can tint the UI from the playing track’s artwork.

## Connectivity and personalization

- Saved Wi-Fi profiles are tried in turn; a failed connection/IP attempt falls back to the next usable profile, subject to the PSP's Wi-Fi hardware/security compatibility.
- Spotify Premium and initial device pairing are required.
- Settings include themes, wallpaper/background, cover shape, fonts, list density, animation, clock, hints, Turkish/English language, startup category, category order/visibility, playback/sound options, battery saver, sleep timer, controls and headphone remote.
- Preferences persist in `settings.ini`. `list_style = archive` selects Archive View; `list_covers = off` disables list thumbnails.
- Spotify Connect is **not** a completed/hardware-accepted feature on this branch.

## Scope and validation

The PSP screen is **480 × 272 pixels**. Archive View targets two columns of ten entries using compact typography and truncates long titles to cell width. Initial real-device testing reported text extending outside the green selection highlight; row text now adapts to the actual line height and is vertically centered. **The 24 October PSP tests passed the Recently Played layout, green-highlight text alignment and the corrected SELECT follow-track behavior. Turkish/English UI tests `LANG-01..05` passed on PSP-3000. The newly redesigned Archive mini-player still requires hardware testing.** For implementation details see [IMPLEMENTATION.md](IMPLEMENTATION.md); for installation and troubleshooting see [SETUP.md](SETUP.md).
