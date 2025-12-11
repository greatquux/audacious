# Commit Instructions for "Stop After This Song" Feature

This document describes the changes made to implement the "Stop after this song" feature for the Audacious media player.

## Summary

Added a new feature allowing users to right-click on any playlist entry and select "Stop After This Song" to stop playback after that specific entry finishes playing (not just the currently playing song).

## Files Modified

### Backend (Core Playback Logic)

1. **src/libaudcore/config.cc**
   - Added two new config keys to `core_defaults[]`:
     - `"stop_after_playlist", "-1"` - Stores the playlist index of the stop-after target
     - `"stop_after_entry", "-1"` - Stores the entry index within that playlist
   - These replace the limitation of the old `stop_after_current_song` boolean which only worked for the currently playing song

2. **src/libaudcore/playback.cc** 
   - Modified `end_cb()` function (called when a song finishes) to check if the finishing song is the stop-after target
   - If `stop_after_entry >= 0 && stop_after_playlist >= 0 && target_playlist == playlist.index() && target_entry == playlist.get_position()`:
     - Call `do_stop()` to stop playback
     - Clear the target by setting both config keys back to -1
     - Call `do_next()` to prevent advancing to the next song
   - Maintains backward compatibility with `stop_after_current_song` boolean

3. **src/libaudcore/drct.h** and **src/libaudcore/drct.cc**
   - Added three new public API functions:
     - `aud_drct_pl_set_stop_after(int playlist_index, int entry)` - Sets the stop-after target
     - `aud_drct_pl_clear_stop_after()` - Clears the target and legacy boolean
     - `aud_drct_pl_get_stop_after(int & playlist_index, int & entry)` - Queries the current target
   - These are wrapper functions around the config get/set operations

### GTK UI (libaudgui)

4. **src/libaudgui/playlist-context.cc** (NEW FILE)
   - Created new helper function `audgui_playlist_context_menu(Playlist playlist, int entry)`
   - Creates a GtkMenu with a "Stop After This Song" menu item
   - Connects the menu item to a callback that calls `aud_drct_pl_set_stop_after()`
   - Properly manages Playlist object lifetime using g_object_set_data_full()

5. **src/libaudgui/libaudgui.h**
   - Added declaration: `GtkWidget * audgui_playlist_context_menu (Playlist playlist, int entry);`

6. **src/libaudgui/meson.build**
   - Added `'playlist-context.cc'` to the `libaudgui_sources` list

### Qt UI (libaudqt)

7. **src/libaudqt/treeview.h**
   - Added `Q_OBJECT` macro to enable Qt signals/slots
   - Added method: `void setPlaylistContextMenu(bool (*getPlaylist)(int row, class Playlist & playlist_out))`
   - Added override: `void contextMenuEvent(QContextMenuEvent * event)`
   - Added member variable: `bool (*m_get_playlist)(int row, class Playlist & playlist_out)`

8. **src/libaudqt/treeview.cc**
   - Implemented `contextMenuEvent()` to show a context menu on right-click
   - Menu includes a "Stop After This Song" action
   - When action is triggered, calls `aud_drct_pl_set_stop_after(playlist.index(), row)`

## How It Works

1. User right-clicks on a playlist entry (in either GTK or Qt UI)
2. Context menu appears with "Stop After This Song" option
3. User selects the option
4. Backend stores the playlist index and entry index as the stop-after target
5. When that entry finishes playing, the `end_cb()` callback detects it matches the target
6. Playback is stopped, the target is cleared, and the next song is not played
7. The player remains paused at the end of the selected song

## Testing

To test this feature:

1. Build Audacious with these changes using Meson:
   ```bash
   meson setup builddir
   cd builddir
   ninja
   sudo ninja install
   ```

2. Launch Audacious and open a playlist with multiple songs

3. Right-click on any song (not the currently playing one)

4. Select "Stop After This Song"

5. Start playback from a different song

6. Verify that playback stops after the selected song completes

## Git Commit

To commit these changes:

```bash
cd /workspaces/audacious
git config user.name "Your Name"
git config user.email "your.email@example.com"
git add -A
git commit -m "Add 'Stop after this song' feature for playlist entries

This commit implements a new feature allowing users to right-click on any
playlist entry and select 'Stop After This Song' to stop playback after
that specific entry finishes playing (not just the currently playing song).

Backend Changes:
- Added two new config keys in src/libaudcore/config.cc:
  'stop_after_playlist' and 'stop_after_entry' (default: -1)
- Modified src/libaudcore/playback.cc end_cb() function to check if the
  finishing song is the stop-after target and stop playback if matched
- Properly clear the stop-after target after stopping

Public API:
- Added three new functions to src/libaudcore/drct.h and drct.cc:
  * aud_drct_pl_set_stop_after(int playlist_index, int entry)
  * aud_drct_pl_clear_stop_after()
  * aud_drct_pl_get_stop_after(int & playlist_index, int & entry)

GTK UI (libaudgui):
- Created new src/libaudgui/playlist-context.cc with audgui_playlist_context_menu()
  helper function that creates a context menu with 'Stop After This Song' item
- Updated src/libaudgui/libaudgui.h with function declaration
- Updated src/libaudgui/meson.build to include new source file

Qt UI (libaudqt):
- Extended src/libaudqt/treeview.h with:
  * setPlaylistContextMenu() method to enable feature
  * contextMenuEvent() override to show menu on right-click
  * Member variable to track playlist getter callback
- Updated src/libaudqt/treeview.cc to implement context menu with same action

All changes maintain compatibility with existing codebase and follow
established coding patterns in Audacious."
git push origin master
```

## Files Changed Summary

- Modified: 8 files
  - src/libaudcore/config.cc
  - src/libaudcore/playback.cc
  - src/libaudcore/drct.h
  - src/libaudcore/drct.cc
  - src/libaudgui/libaudgui.h
  - src/libaudgui/meson.build
  - src/libaudqt/treeview.h
  - src/libaudqt/treeview.cc
  
- Created: 1 file
  - src/libaudgui/playlist-context.cc

Total: 9 files affected
