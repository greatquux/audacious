# Qt Compilation Error - Final Fix

## Issue
The original code tried to instantiate `Playlist` in the `contextMenuEvent` member function where it was an incomplete type:

```cpp
EXPORT void TreeView::contextMenuEvent(QContextMenuEvent * event)
{
    // ...
    Playlist playlist;  // ❌ ERROR: Playlist is incomplete type here
    if (!m_get_playlist(row, playlist))
        return;
    
    int playlist_index = playlist.index();
    
    connect(stop_after, &QAction::triggered, [playlist_index, row]() {
        aud_drct_pl_set_stop_after(playlist_index, row);
    });
}
```

## Root Cause
The `Playlist` class is forward-declared in `treeview.h` but not included there (only in `treeview.cc`). When the compiler encounters `Playlist playlist;` in `contextMenuEvent`, it doesn't have the full type definition, causing an "incomplete type" error.

## Solution
Move the `Playlist` instantiation inside the lambda, where it will be compiled in a context that has full access to the complete type definition:

```cpp
EXPORT void TreeView::contextMenuEvent(QContextMenuEvent * event)
{
    // ...
    int row = idx.row();
    
    connect(stop_after, &QAction::triggered, [this, row]() {
        Playlist playlist;  // ✅ OK: Full type available in lambda context
        if (m_get_playlist(row, playlist))
        {
            aud_drct_pl_set_stop_after(playlist.index(), row);
        }
    });
}
```

## Why This Works
1. The lambda captures `this` (the TreeView object) to access `m_get_playlist`
2. When the lambda is executed (when the action is triggered), the full `Playlist` type is available
3. No incomplete type in the main function scope
4. Functionally identical - we still call the same API with the same parameters

## Build
Now rebuild with:
```bash
cd /workspaces/audacious/builddir
ninja
```

This should compile successfully.

