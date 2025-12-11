#!/bin/bash
# Rebuild and capture full error output

cd /workspaces/audacious/builddir

echo "=== Starting clean rebuild ==="
echo "Removing cached object files..."
rm -f src/libaudqt/libaudqt.so.4.0.0.p/treeview.cc.o*
rm -f src/libaudgui/libaudgui.so.6.0.0.p/playlist-context.cc.o*
rm -f src/libaudcore/libaudcore.so.6.0.0.p/playback.cc.o*
rm -f src/libaudcore/libaudcore.so.6.0.0.p/drct.cc.o*

echo ""
echo "=== Rebuilding (limiting to 1 job to see errors clearly) ==="
ninja -j1 2>&1 | tee build_output.log

echo ""
echo "=== Build Complete ==="
echo "Exit code: $?"
echo "Full output saved to build_output.log"

# Show any errors
echo ""
echo "=== Extracting errors ==="
grep -i "error:" build_output.log || echo "No 'error:' lines found"
