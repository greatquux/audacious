# Quick Reference: GTK Playlist Widget Code Locations

## Core Components

### 1. Widget Creation
- **Main Function**: `audgui_list_new()` macro
  - **Definition**: [src/libaudgui/list.h#L60-L61](src/libaudgui/list.h#L60-L61)
  - **Implementation**: [src/libaudgui/list.cc#L572](src/libaudgui/list.cc#L572)
  - **Returns**: GtkTreeView widget

### 2. Custom Tree Model
- **Structure Definition**: [src/libaudgui/list.cc#L44-L58](src/libaudgui/list.cc#L44-L58)
- **GtkTreeModel Interface**: [src/libaudgui/list.cc#L60-L180](src/libaudgui/list.cc#L60-L180)

### 3. Callback Structure
- **Definition**: [src/libaudgui/list.h#L30-L56](src/libaudgui/list.h#L30-L56)
- **Required callback**: `get_value` - fetches cell data
- **Optional callbacks**: 
  - Selection: `get_selected`, `set_selected`, `select_all`
  - Interaction: `activate_row`, `right_click`, `shift_rows`
  - Drag/Drop: `get_data`, `receive_data`
  - Mouse: `mouse_motion`, `mouse_leave`
  - Focus: `focus_change`

### 4. Right-Click Handling

#### Event Handler (Button Press)
- **Location**: [src/libaudgui/list.cc#L237-L272](src/libaudgui/list.cc#L237-L272)
- **Function**: `button_press_cb()`
- **Checks for**: Button 3 (right-click)
- **Calls**: Either custom `right_click` callback or default handler

#### Default Right-Click Handler
- **Public Function**: [src/libaudgui/list.h#L75](src/libaudgui/list.h#L75) (declaration)
- **Function**: `audgui_playlist_right_click(void * user, GdkEventButton * event)`
- **Location**: [src/libaudgui/playlist-context.cc#L80-L82](src/libaudgui/playlist-context.cc#L80-L82)
- **Implementation**: [src/libaudgui/playlist-context.cc#L61-L77](src/libaudgui/playlist-context.cc#L61-L77)

### 5. Context Menu Creation
- **Function**: `audgui_playlist_context_menu()`
- **Location**: [src/libaudgui/playlist-context.cc#L39-L58](src/libaudgui/playlist-context.cc#L39-L58)
- **Returns**: GtkMenu widget
- **Parameters**: 
  - `Playlist playlist` - which playlist
  - `int entry` - entry index
- **Menu Items**:
  - "Stop After This Song" (with callback to `stop_after_this()`)

### 6. Column Management
- **Add Column**: [src/libaudgui/list.cc#L690](src/libaudgui/list.cc#L690)
- **Function**: `audgui_list_add_column(GtkWidget * list, const char * title, int column, GType type, int width, bool use_markup = false)`

### 7. Selection Management
- **Selection Callbacks**: [src/libaudgui/list.cc#L182-L215](src/libaudgui/list.cc#L182-L215)
- **Update Selection**: [src/libaudgui/list.cc#L551-L563](src/libaudgui/list.cc#L551-L563)
- **Set/Get Highlight**: Row emphasis for current playback position
- **Set/Get Focus**: Keyboard selection position

## Signal Connections

All signal handlers registered in `audgui_list_new_real()` at [src/libaudgui/list.cc#L608-L640](src/libaudgui/list.cc#L608-L640):

```
destroy              → destroy_cb
cursor-changed       → focus_cb (if focus_change callback provided)
row-activated        → activate_cb (if activate_row callback provided)
button-press-event   → button_press_cb (ALWAYS connected)
button-release-event → button_release_cb (ALWAYS connected)
key-press-event      → key_press_cb (ALWAYS connected)
motion-notify-event  → motion_notify_cb (ALWAYS connected)
leave-notify-event   → leave_notify_cb (ALWAYS connected)
selection::changed   → select_cb (if get_selected callback provided)
drag-begin           → drag_begin (if drag callbacks provided)
drag-end             → drag_end (if drag callbacks provided)
drag-motion          → drag_motion (if drag callbacks provided)
drag-leave           → drag_leave (if drag callbacks provided)
drag-drop            → drag_drop (if drag callbacks provided)
drag-data-get        → drag_data_get (if get_data callback provided)
drag-data-received   → drag_data_received (if receive_data callback provided)
```

## Usage Examples

### Queue Manager
- **File**: [src/libaudgui/queue-manager.cc](src/libaudgui/queue-manager.cc)
- **Creation**: [src/libaudgui/queue-manager.cc#L188-L190](src/libaudgui/queue-manager.cc#L188-L190)
- **Usage Pattern**:
  ```cpp
  GtkWidget * qm_list = audgui_list_new (& callbacks, nullptr, count);
  gtk_tree_view_set_headers_visible ((GtkTreeView *) qm_list, false);
  audgui_list_add_column (qm_list, nullptr, 0, G_TYPE_INT, 7);
  audgui_list_add_column (qm_list, nullptr, 1, G_TYPE_STRING, -1);
  gtk_container_add ((GtkContainer *) scrolled, qm_list);
  ```

### Equalizer Presets
- **File**: [src/libaudgui/eq-preset.cc](src/libaudgui/eq-preset.cc)
- **Creation**: [src/libaudgui/eq-preset.cc#L320](src/libaudgui/eq-preset.cc#L320)

### Jump to Track
- **File**: [src/libaudgui/jump-to-track.cc](src/libaudgui/jump-to-track.cc)
- **Creation**: [src/libaudgui/jump-to-track.cc#L260](src/libaudgui/jump-to-track.cc#L260)

## Data Flow for Right-Click

```
User right-clicks on playlist entry
    ↓
GtkTreeView receives button-press-event
    ↓
button_press_cb() checks if event->button == 3
    ↓
Set tree cursor to clicked row (respecting selection)
    ↓
Check if widget has custom right_click callback:
    ├─ YES → Call model->cbs->right_click(model->user, event)
    └─ NO  → Call audgui_playlist_right_click(widget, event)
    ↓
audgui_playlist_right_click():
    1. Get row at event position
    2. Get active playlist
    3. Call audgui_playlist_context_menu(playlist, row)
    ↓
audgui_playlist_context_menu():
    1. Create empty GtkMenu
    2. Create "Stop After This Song" menu item
    3. Attach callbacks and data
    4. Return menu
    ↓
Show menu at cursor with gtk_menu_popup_at_pointer()
    ↓
User clicks menu item → Callback fired → Action performed
```

## Key Macros & Helpers

### In list.c:
- `MODEL_HAS_CB(m, cb)` - Check if callback is available
- `PATH_IS_SELECTED(w, p)` - Check if path is selected

### In list.h:
- `audgui_list_new(c, u, r)` - Create new list widget macro

### Row Location:
- `audgui_list_row_at_point(list, x, y)` - Get row index at pixel position
- Used in [src/libaudgui/playlist-context.cc#L65](src/libaudgui/playlist-context.cc#L65)

## Import/Headers

To use these functions, include:
```cpp
#include "list.h"                  // For audgui_list_new
#include "libaudgui-gtk.h"         // For GtkWidget functions
#include <libaudcore/playlist.h>   // For Playlist class
```

## Summary

| Aspect | Type | Location |
|--------|------|----------|
| **Widget Type** | GtkTreeView | [src/libaudgui/list.cc#L595](src/libaudgui/list.cc#L595) |
| **Data Model** | Custom ListModel (GtkTreeModel impl.) | [src/libaudgui/list.cc#L44-L170](src/libaudgui/list.cc#L44-L170) |
| **Creation** | `audgui_list_new()` macro | [src/libaudgui/list.h#L60](src/libaudgui/list.h#L60) |
| **Callbacks** | `AudguiListCallbacks` struct | [src/libaudgui/list.h#L30](src/libaudgui/list.h#L30) |
| **Right-Click** | `button_press_cb()` | [src/libaudgui/list.cc#L237](src/libaudgui/list.cc#L237) |
| **Context Menu** | `audgui_playlist_context_menu()` | [src/libaudgui/playlist-context.cc#L39](src/libaudgui/playlist-context.cc#L39) |
| **Menu Handler** | `audgui_playlist_right_click()` | [src/libaudgui/playlist-context.cc#L80](src/libaudgui/playlist-context.cc#L80) |
