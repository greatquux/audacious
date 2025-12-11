# GTK Playlist Widget: Implementation Details & Interaction Patterns

## Table of Contents
1. [Widget Architecture](#widget-architecture)
2. [Data Model Design](#data-model-design)
3. [Signal Flow](#signal-flow)
4. [Right-Click Menu Implementation](#right-click-menu-implementation)
5. [Callback Backward Compatibility](#callback-backward-compatibility)
6. [Performance Considerations](#performance-considerations)

---

## Widget Architecture

### High-Level View
```
Application Code (e.g., queue-manager.cc)
            ↓
    audgui_list_new(callbacks, user, rows)
            ↓
    ┌───────────────────────────────────┐
    │   GtkTreeView Widget              │
    │  ┌─────────────────────────────┐  │
    │  │  Custom ListModel           │  │
    │  │  (implements GtkTreeModel)  │  │
    │  │                             │  │
    │  │  - Contains AudguiListCall- │  │
    │  │    backs pointer            │  │
    │  │  - Maintains row count      │  │
    │  │  - Stores highlight/focus   │  │
    │  │                             │  │
    │  └─────────────────────────────┘  │
    │                                   │
    │  Signal Handlers:                 │
    │  - Button press/release           │
    │  - Row activation                 │
    │  - Drag & drop                    │
    │  - Keyboard navigation            │
    └───────────────────────────────────┘
            ↓
    (Emit signals via callbacks)
            ↓
    Application Callbacks (provided by caller)
```

### Why This Design?

**Traditional GtkListStore/GtkTreeStore Approach (NOT used):**
- Data duplicated in model
- All data must be loaded upfront
- Poor for large playlists

**Audacious Custom Model Approach (USED):**
- Model is stateless regarding data
- Data fetched on-demand via `get_value` callback
- Caller maintains data elsewhere (in Playlist object)
- Scales to arbitrary number of rows
- Caller controls what data is displayed

---

## Data Model Design

### ListModel Structure

**File**: [src/libaudgui/list.cc](src/libaudgui/list.cc#L44-L58)

```cpp
struct ListModel {
    GObject parent;                         // GObject instance
    const AudguiListCallbacks * cbs;       // Callback function pointers
    int cbs_size;                          // Size of callback struct (for versioning)
    void * user;                           // User context pointer
    int charwidth;                         // Character width (for sizing)
    int rows, highlight;                   // Row count and highlighted row
    int columns;                           // Column count
    GList * column_types;                  // G_TYPE_* for each column
    bool resizable;                        // Column resize flags
    bool frozen, blocked;                  // Selection state flags
    bool dragging;                         // Drag operation in progress
    int clicked_row, receive_row;          // Event tracking
    int scroll_speed;                      // Auto-scroll velocity
};
```

### GtkTreeModel Interface Implementation

The ListModel is a **GObject subclass** that implements **GtkTreeModel interface**.

**Key Methods**:

| Method | Purpose | Implementation |
|--------|---------|-----------------|
| `get_value()` | Fetch cell data | Calls `cbs->get_value()` callback |
| `get_iter()` | Get iterator from path | Simply converts path→row index |
| `get_path()` | Get path from iterator | Simply converts row index→path |
| `iter_next()` | Move to next row | Increment row in iterator |
| `iter_children()` | Get children | Returns false (list, not tree) |
| `iter_has_child()` | Check children | Returns false |
| `iter_n_children()` | Count children | Returns 0 or row count |
| `get_flags()` | Model flags | Returns GTK_TREE_MODEL_LIST_ONLY |
| `get_n_columns()` | Column count | Returns columns |
| `get_column_type()` | Column type | Returns GType for column |

**Why This Matters**:
- GtkTreeView uses these methods to render and navigate the list
- The model is very lightweight - just state tracking
- All actual data comes from callbacks
- No data is stored in the model itself

---

## Signal Flow

### Initialization Flow

```
audgui_list_new(callbacks, user, 100)
    ↓ (actually calls audgui_list_new_real)
Create ListModel GObject
    ├─ Set cbs = callbacks
    ├─ Set user = user
    ├─ Set rows = 100
    ├─ Set highlight = -1
    └─ Set columns = RESERVED_COLUMNS (1)
    ↓
Create GtkTreeView with ListModel
    ↓
Register Signal Handlers:
    ├─ "destroy" → destroy_cb (cleanup)
    ├─ "button-press-event" → button_press_cb (always)
    ├─ "button-release-event" → button_release_cb (always)
    ├─ "key-press-event" → key_press_cb (always)
    ├─ "motion-notify-event" → motion_notify_cb (always)
    ├─ "leave-notify-event" → leave_notify_cb (always)
    ├─ "cursor-changed" → focus_cb (if focus_change callback)
    ├─ "row-activated" → activate_cb (if activate_row callback)
    ├─ "changed" (selection) → select_cb (if get_selected callback)
    ├─ drag handlers (if drag callbacks)
    └─ drop handlers (if receive_data callback)
    ↓
Return GtkTreeView widget
```

### User Interaction Flow: Left-Click

```
User clicks on row 5
    ↓
button_press_cb() fires with event->button == 1
    ├─ Get path at position
    ├─ If path selected and no modifiers → freeze selection
    ├─ Set cursor to row
    └─ Return false (let GTK handle selection)
    ↓
GTK updates selection
    ↓
selection::changed signal fires
    ↓
select_cb() in ListModel
    ├─ Set model->blocked = true (prevent recursion)
    ├─ Iterate through selection
    │   └─ Call cbs->set_selected(user, row, selected)
    └─ Set model->blocked = false
    ↓
Selection updated
```

### User Interaction Flow: Right-Click

See [Right-Click Menu Implementation](#right-click-menu-implementation) below.

### User Interaction Flow: Double-Click (Activation)

```
User double-clicks on row 7
    ↓
button_press_cb() fires with event->button == 1
    └─ Update cursor/selection
    ↓
GtkTreeView detects double-click
    ↓
"row-activated" signal fires
    ↓
activate_cb() in ListModel (if activate_row callback)
    ├─ Get row index from path
    └─ Call cbs->activate_row(user, row)
    ↓
Application handles activation
```

### Keyboard Navigation Flow

```
User presses Down arrow
    ↓
GtkTreeView handles arrow navigation
    ├─ Moves cursor to next row
    ├─ Updates selection (if in selection mode)
    └─ Model queries new rows via get_value()
    ↓
"cursor-changed" signal fires
    ↓
focus_cb() in ListModel (if focus_change callback)
    ├─ Get focus row
    └─ Call cbs->focus_change(user, row)
    ↓
Application notified of focus change
```

---

## Right-Click Menu Implementation

### Complete Right-Click Flow

```
┌─────────────────────────────────────────────────────────┐
│ STEP 1: Right-Click Event Detected                      │
├─────────────────────────────────────────────────────────┤
│ User right-clicks at pixel (x, y)                       │
│ GtkTreeView receives "button-press-event"               │
│ event->button = 3                                       │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│ STEP 2: button_press_cb() [list.cc:237]               │
├─────────────────────────────────────────────────────────┤
│ gtk_tree_view_get_path_at_pos(x, y, &path, ...)       │
│ if (event->button == 3) {                              │
│   // Respect multi-selection                           │
│   if (PATH_IS_SELECTED(path)) model->frozen = true     │
│   gtk_tree_view_set_cursor(path, NULL, false)         │
│   model->frozen = false                                │
│                                                         │
│   if (MODEL_HAS_CB(model, right_click)) {             │
│     model->cbs->right_click(model->user, event)       │
│   } else {                                              │
│     // Default handler                                  │
│     audgui_playlist_right_click(widget, event)        │
│   }                                                     │
│   gtk_tree_path_free(path)                            │
│   return TRUE  // Event handled                         │
│ }                                                       │
└────────────────┬────────────────────────────────────────┘
                 │
    ┌────────────┴─────────────┐
    │                          │
    ▼                          ▼
Has custom         No custom
right_click         right_click
callback?           callback
    │                    │
    │                ┌───▼──────────────────────────────┐
    │                │ STEP 3: Default Handler         │
    │                │ audgui_playlist_right_click()   │
    │                │ [playlist-context.cc:80-82]    │
    │                │                                │
    │                │ playlist_right_click(widget, e)│
    │                │ {                              │
    │                │   GtkWidget * list = widget    │
    │                │   int row = audgui_list_row_   │
    │                │     at_point(x, y)             │
    │                │   if (row < 0) return          │
    │                │                                │
    │                │   Playlist p = Playlist::      │
    │                │     active_playlist()          │
    │                │   if (!p.exists()) return      │
    │                │                                │
    │                │   GtkWidget * menu =           │
    │                │     audgui_playlist_           │
    │                │     context_menu(p, row)       │
    │                │   gtk_menu_popup_at_pointer()  │
    │                │ }                              │
    │                └───┬──────────────────────────────┘
    │                    │
    └────────┬───────────┘
             │
    ┌────────▼───────────────────────────────────────┐
    │ STEP 4: Create Context Menu                    │
    │ audgui_playlist_context_menu()                 │
    │ [playlist-context.cc:39-58]                   │
    │                                                │
    │ GtkWidget * menu = gtk_menu_new()             │
    │                                                │
    │ GtkWidget * item =                             │
    │   gtk_menu_item_new_with_mnemonic(            │
    │     _("_Stop After This Song"))               │
    │ gtk_widget_show(item)                          │
    │ gtk_menu_shell_append((GtkMenuShell *)menu,   │
    │   item)                                        │
    │                                                │
    │ // Store playlist & entry in menu item        │
    │ Playlist * playlist_ptr = new Playlist(p)    │
    │ g_object_set_data_full((GObject *)item,      │
    │   "playlist", playlist_ptr, cleanup_func)    │
    │ g_object_set_data((GObject *)item, "entry",  │
    │   GINT_TO_POINTER(entry))                    │
    │                                                │
    │ g_signal_connect(item, "activate",            │
    │   (GCallback) stop_after_this,                │
    │   playlist_ptr)                               │
    │                                                │
    │ return menu                                    │
    └────────┬───────────────────────────────────────┘
             │
    ┌────────▼───────────────────────────────────────┐
    │ STEP 5: Display Menu                           │
    │                                                │
    │ gtk_menu_popup_at_pointer((GtkMenu *)menu,   │
    │   (GdkEvent *)event)                          │
    │                                                │
    │ Menu appears at mouse cursor                  │
    └────────┬───────────────────────────────────────┘
             │
    ┌────────▼───────────────────────────────────────┐
    │ STEP 6: User Selection                         │
    │                                                │
    │ User clicks menu item                          │
    │ OR presses Enter/Click on item                │
    └────────┬───────────────────────────────────────┘
             │
    ┌────────▼───────────────────────────────────────┐
    │ STEP 7: Activate Item                          │
    │                                                │
    │ "activate" signal fires on menu item          │
    │ stop_after_this() callback invoked             │
    │ [playlist-context.cc:32-35]                   │
    │                                                │
    │ stop_after_this(item, data) {                │
    │   Playlist * p = (Playlist *)data             │
    │   int entry = g_object_get_data(...,"entry")│
    │   aud_drct_pl_set_stop_after(p->index(),     │
    │     entry)                                    │
    │ }                                              │
    └────────┬───────────────────────────────────────┘
             │
    ┌────────▼───────────────────────────────────────┐
    │ STEP 8: Set Stop Point                         │
    │ [drct.cc or playback.cc]                       │
    │                                                │
    │ Store: stop_after_playlist = playlist_index   │
    │ Store: stop_after_entry = entry_index         │
    │                                                │
    │ On playback: Check if stop_after is set       │
    │ and stop at that entry                         │
    └────────────────────────────────────────────────┘
```

### Code Snippet: Complete Right-Click Chain

**In list.cc (button_press_cb):**
```cpp
static gboolean button_press_cb (GtkWidget * widget, GdkEventButton * event,
 ListModel * model)
{
    // ... path determination ...
    
    if (event->type == GDK_BUTTON_PRESS && event->button == 3)  // RIGHT-CLICK
    {
        // Preserve selection if clicking on already-selected row
        if (path && PATH_IS_SELECTED (widget, path))
            model->frozen = true;
        
        gtk_tree_view_set_cursor ((GtkTreeView *) widget, path, nullptr, false);
        model->frozen = false;

        // Call right-click callback (custom or default)
        if (MODEL_HAS_CB (model, right_click))
            model->cbs->right_click (model->user, event);  // Custom handler
        else
        {
            extern void audgui_playlist_right_click (void * user, GdkEventButton * event);
            audgui_playlist_right_click (widget, event);    // Default handler
        }

        if (path) gtk_tree_path_free (path);
        return true;  // Event handled
    }
    
    // ... other button handling ...
}
```

**In playlist-context.cc (default handler):**
```cpp
static void playlist_right_click (void * user, GdkEventButton * event)
{
    GtkWidget * list = (GtkWidget *)user;
    
    // Get row at click position
    int row = audgui_list_row_at_point(list, event->x, event->y);
    if (row < 0) return;
    
    // Get active playlist
    Playlist playlist = Playlist::active_playlist();
    if (!playlist.exists()) return;
    
    // Create context menu
    GtkWidget * menu = audgui_playlist_context_menu(playlist, row);
    
    // Show menu at mouse cursor
    gtk_menu_popup_at_pointer((GtkMenu *)menu, (GdkEvent *)event);
}

EXPORT void audgui_playlist_right_click (void * user, GdkEventButton * event)
{
    playlist_right_click(user, event);
}
```

**Menu Creation:**
```cpp
EXPORT GtkWidget * audgui_playlist_context_menu (Playlist playlist, int entry)
{
    GtkWidget * menu = gtk_menu_new ();
    
    // Create "Stop After This Song" item
    GtkWidget * item = gtk_menu_item_new_with_mnemonic (_("_Stop After This Song"));
    gtk_widget_show (item);
    gtk_menu_shell_append ((GtkMenuShell *) menu, item);
    
    // Store playlist and entry in menu item
    Playlist * playlist_ptr = new Playlist(playlist);
    g_object_set_data_full ((GObject *) item, "playlist", 
                            playlist_ptr,
                            [] (gpointer data) { delete (Playlist *)data; });
    g_object_set_data ((GObject *) item, "entry", GINT_TO_POINTER(entry));
    
    // Connect activation signal
    g_signal_connect (item, "activate", (GCallback) stop_after_this, playlist_ptr);
    
    return menu;
}
```

---

## Callback Backward Compatibility

### The cbs_size Parameter

**Problem**: How to add new callbacks without breaking existing code?

**Solution**: Version the callback structure size

**How It Works**:

1. When code calls `audgui_list_new(&callbacks, user, rows)`:
   - Macro expands to: `audgui_list_new_real(&callbacks, sizeof(AudguiListCallbacks), user, rows)`
   - `sizeof` is computed at **compile time** with **caller's version** of the struct

2. In `audgui_list_new_real()`:
   - Stores `cbs_size` in the model
   - When checking for a callback, uses: `MODEL_HAS_CB(model, field)`
   
3. `MODEL_HAS_CB` macro checks:
   ```cpp
   #define MODEL_HAS_CB(m, cb) \
    ((m)->cbs_size > (int) offsetof (AudguiListCallbacks, cb) && (m)->cbs->cb)
   ```
   - `offsetof(struct, field)` = byte offset of field in struct
   - If old code has small struct, new callback won't be checked
   - Old binary still works with new library

**Example Timeline**:
```
Year 1: Library v1 has callbacks A, B, C
        Old Code links with v1
        sizeof(Callbacks) = X bytes

Year 2: Library v2 adds callback D
        sizeof(Callbacks) = X + 8 bytes
        
Old Code with Library v2:
  - Still passes X bytes (from compile time)
  - audgui_list_new_real() receives cbs_size = X
  - offsetof(Callbacks, D) = X
  - MODEL_HAS_CB(model, D) checks: X > X (false)
  - Callback D is never called ✓
  - Old code works! ✓

New Code with Library v2:
  - Passes X + 8 bytes (from compile time)
  - audgui_list_new_real() receives cbs_size = X + 8
  - offsetof(Callbacks, D) = X
  - MODEL_HAS_CB(model, D) checks: X + 8 > X (true)
  - Callback D can be called ✓
  - New features work! ✓
```

---

## Performance Considerations

### 1. Fixed Height Mode
```cpp
gtk_tree_view_set_fixed_height_mode ((GtkTreeView *) list, true);
```
- **Purpose**: All rows have same height
- **Benefit**: GtkTreeView doesn't query height for every row
- **Impact**: Massive performance boost for large lists
- **Trade-off**: Can't have variable-height rows

### 2. On-Demand Data Fetching
- **Model**: Stateless regarding data
- **How**: Calls `get_value` callback only when rendering
- **Benefit**: 
  - Only data for visible rows is fetched
  - Can handle playlists with millions of entries
  - Can update data without rebuilding model
- **Trade-off**: Slightly more complex callback setup

### 3. Selection State Optimization
```cpp
model->blocked = true;  // Prevent recursion during bulk updates
// ... update selections ...
model->blocked = false;
```
- **Purpose**: Prevent feedback loops during selection changes
- **Benefit**: Avoid cascading "changed" signals
- **Usage**: Bulk selection updates use this flag

### 4. Frozen Selection
```cpp
if (PATH_IS_SELECTED (widget, path))
    model->frozen = true;  // Don't clear multi-selection during click
gtk_tree_view_set_cursor (...);
model->frozen = false;
```
- **Purpose**: Preserve multi-selection during right-click
- **Benefit**: User can right-click on any selected item
- **Implementation**: Flag prevents selection changes in `select_cb()`

### 5. Character Width Caching
```cpp
model->charwidth = audgui_get_digit_width (list);
```
- **Purpose**: Get font metrics once, not per column
- **Usage**: Column sizing calculations use this cached value
- **Benefit**: Avoid repeated font metric queries

---

## Signal Connection Order

**Important**: Signal handlers registered in `audgui_list_new_real()` follow this order:

1. **Mandatory handlers** (always connected):
   - `destroy` - cleanup
   - `button-press-event` - selection, right-click
   - `button-release-event` - finalize selection
   - `key-press-event` - keyboard shortcuts
   - `motion-notify-event` - drag tracking
   - `leave-notify-event` - drag handling

2. **Conditional handlers** (if callbacks provided):
   - `cursor-changed` → focus_change callback
   - `row-activated` → activate_row callback
   - Selection-related handlers
   - Drag & drop handlers

**Why This Matters**:
- Handler execution order can affect behavior
- GTK fires signals in connection order (usually)
- Selection blocking uses `model->blocked` flag
- Drag handlers modify click behavior

---

## Summary: Widget Type & Creation

| Aspect | Details |
|--------|---------|
| **Widget Type** | `GtkTreeView` (standard GTK list view) |
| **Data Model** | Custom `ListModel` (GtkTreeModel interface) |
| **Data Storage** | NOT in model; fetched via callbacks |
| **Creation Function** | `audgui_list_new()` macro/function |
| **Performance Mode** | Fixed-height rows for speed |
| **Context Menu** | Created on-demand in `audgui_playlist_context_menu()` |
| **Right-Click Handling** | Via `button_press_cb()` → default/custom handler |
| **Backward Compat** | Via `cbs_size` parameter and `offsetof()` checks |
