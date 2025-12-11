#!/bin/bash
# Script to commit the "Stop After This Song" feature to the Audacious fork
# 
# This script will:
# 1. Configure git with your user information
# 2. Stage all changed files
# 3. Create a detailed commit message documenting all changes
# 4. Push to the fork on GitHub

# Set your git configuration here
GIT_USER_NAME="GitHub Copilot"
GIT_USER_EMAIL="copilot@github.com"

cd /workspaces/audacious

# Configure git user
git config user.name "$GIT_USER_NAME"
git config user.email "$GIT_USER_EMAIL"

# Verify we have changes to commit
echo "=== Git Status Before Commit ==="
git status
echo ""

# Stage all changes
echo "=== Staging Changes ==="
git add -A
echo "Files staged successfully"
echo ""

# Create commit with detailed message
echo "=== Creating Commit ==="
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
established coding patterns in Audacious.

Files Changed:
- src/libaudcore/config.cc
- src/libaudcore/playback.cc
- src/libaudcore/drct.h
- src/libaudcore/drct.cc
- src/libaudgui/playlist-context.cc (new)
- src/libaudgui/libaudgui.h
- src/libaudgui/meson.build
- src/libaudqt/treeview.h
- src/libaudqt/treeview.cc"

echo ""
echo "=== Commit Created ==="
git log --oneline -1

# Show files in commit
echo ""
echo "=== Files in Commit ==="
git diff-tree --no-commit-id --name-status -r HEAD

# Push to fork
echo ""
echo "=== Pushing to GitHub Fork ==="
git push origin master

echo ""
echo "=== Commit Complete ==="
echo "Your changes have been pushed to: https://github.com/greatquux/audacious/tree/master"
