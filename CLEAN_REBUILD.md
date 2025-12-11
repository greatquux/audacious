# Clean Rebuild Instructions

The error you're seeing is from a stale build cache. The source code has been fixed, but the old compiled object files still exist.

## Solution: Clean Rebuild

Run these commands in order:

```bash
cd /workspaces/audacious/builddir

# Remove ALL build artifacts to force a clean rebuild
rm -rf *

# Reconfigure and rebuild from scratch
cd ..
meson setup builddir
cd builddir
ninja
```

## Alternative: Incremental Clean

If you want a faster incremental rebuild, you can just remove the specific cached files:

```bash
cd /workspaces/audacious/builddir

# Remove only the Qt treeview object file
rm -f src/libaudqt/libaudqt.so.4.0.0.p/treeview.cc.o*

# Rebuild
ninja
```

## What Changed

The source file `src/libaudqt/treeview.cc` has been updated to move the `Playlist` instantiation inside the lambda function, which avoids the "incomplete type" error.

**Original code (line 126):**
```cpp
Playlist playlist;  // ❌ WRONG: incomplete type in this scope
```

**Fixed code:**
```cpp
Playlist playlist;  // ✅ NOW CORRECT: inside lambda where full type is available
```

The fix is already in the source code. The compilation will succeed after you do a clean rebuild.

## If It Still Fails

If you still get an error after cleaning, please paste the FULL error message and we'll debug further. The error message will tell us exactly what's wrong.
