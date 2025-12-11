# Audacious GTK Playlist Widget Display Analysis

## Overview
This document provides a comprehensive analysis of how Audacious displays playlist entries in its GTK interface, including the widget types used, creation methods, and context menu handling.

---

## 1. Main Playlist View Widget Creation

### Widget Type: **GtkTreeView**
The main GTK playlist view uses a custom **GtkTreeView** widget created through Audacious's abstraction layer.

### Creation Function: `audgui_list_new()`

**Location:** [src/libaudgui/list.h](src/libaudgui/list.h) (macro definition)
**Implementation:** [src/libaudgui/list.cc](src/libaudgui/list.cc) (lines 572+)

#### Macro Definition:
```cpp
#define audgui_list_new(c, u, r) \
 audgui_list_new_real (c, sizeof (AudguiListCallbacks), u, r)
```

#### Function Signature:
```cpp
GtkWidget * audgui_list_new_real (
    const AudguiListCallbacks * cbs,  // Callback structure
    int cbs_size,                     // Size of callback struct
    void * user,                      // User data/context
    int rows                          // Initial row count
)
```

#### What It Does:
1. Creates a **custom ListModel** (GObject derivative) that implements **GtkTreeModel interface**
2. Creates a **GtkTreeView** with this custom model
3. Sets up the tree view with fixed height mode for performance
4. Registers signal handlers for user interactions
5. Sets up drag-and-drop support if callbacks are provided
6. Returns the **GtkTreeView widget** to the caller

#### Key Code from [src/libaudgui/list.cc](src/libaudgui/list.cc#L572-L650):
```cpp
EXPORT GtkWidget * audgui_list_new_real (
    const AudguiListCallbacks * cbs, int cbs_size,
    void * user, int rows)
{
    // Create custom tree model
    ListModel * model = (ListModel *) g_object_new (list_model_get_type (), nullptr);
    model->cbs = cbs;
    model->cbs_size = cbs_size;
    model->user = user;
    model->rows = rows;
    // ... initialization ...

    // Create GtkTreeView with the custom model
    GtkWidget * list = gtk_tree_view_new_with_model ((GtkTreeModel *) model);
    gtk_tree_view_set_fixed_height_mode ((GtkTreeView *) list, true);
    
    // Register signal handlers
    g_signal_connect (list, "button-press-event", (GCallback) button_press_cb, model);
    g_signal_connect (list, "button-release-event", (GCallback) button_release_cb, model);
    g_signal_connect (list, "row-activated", (GCallback) activate_cb, model);
    // ... more signal handlers ...
    
    return list;
}
```

---

## 2. Custom Tree Model: ListModel

### Structure Definition
**Location:** [src/libaudgui/list.cc](src/libaudgui/list.cc#L44-L58)

```cpp
struct ListModel {
    GObject parent;
    const AudguiListCallbacks * cbs;    // Callback handlers
    int cbs_size;                        // Size of callback struct
    void * user;                         // User-provided context
    int charwidth;                       // Character width for sizing
    int rows, highlight;                 // Row count and highlighted row
    int columns;                         // Column count
    GList * column_types;                // Column type information
    bool resizable;                      // Whether columns are resizable
    bool frozen, blocked;                // State flags
    bool dragging;                       // Drag operation in progress
    int clicked_row, receive_row;        // Row indices for interactions
    int scroll_speed;                    // Autoscroll speed
};
```

### GtkTreeModel Interface Implementation
The ListModel implements the GtkTreeModel interface with:
- `list_model_get_flags()` - Returns GTK_TREE_MODEL_LIST_ONLY
- `list_model_get_n_columns()` - Returns column count
- `list_model_get_column_type()` - Returns type for each column
- `list_model_get_iter()` - Gets iterator for a path
- `list_model_get_path()` - Gets path from iterator
- `list_model_get_value()` - Fetches actual cell data (calls callback)
- `list_model_iter_next()` - Iterates to next row
- `list_model_iter_children()` - Returns children (none for list)
- `list_model_iter_has_child()` - Returns false (no tree structure)
- `list_model_iter_n_children()` - Returns row count or 0

---

## 3. Callback Structure: AudguiListCallbacks

**Location:** [src/libaudgui/list.h](src/libaudgui/list.h#L30-L56)

```cpp
struct AudguiListCallbacks {
    // Required callback
    void (* get_value) (void * user, int row, int column, GValue * value);

    // Selection callbacks (optional)
    bool (* get_selected) (void * user, int row);
    void (* set_selected) (void * user, int row, bool selected);
    void (* select_all) (void * user, bool selected);

    // Activation and interaction (optional)
    void (* activate_row) (void * user, int row);
    void (* right_click) (void * user, GdkEventButton * event);
    void (* shift_rows) (void * user, int row, int before);

    // Drag and drop (optional)
    const char * data_type;
    Index<char> (* get_data) (void * user);
    void (* receive_data) (void * user, int row, const char * data, int len);

    // Mouse events (optional)
    void (* mouse_motion) (void * user, GdkEventMotion * event, int row);
    void (* mouse_leave) (void * user, GdkEventMotion * event, int row);

    // Keyboard focus (optional)
    void (* focus_change) (void * user, int row);
};
```

**Note:** The macro approach allows for backward compatibility when callbacks are added - the `cbs_size` parameter indicates which callbacks are available.

---

## 4. Context Menu & Right-Click Handling

### Right-Click Event Flow

**Location of event handler:** [src/libaudgui/list.cc](src/libaudgui/list.cc#L237-L272)

#### Step 1: Button Press Detection
```cpp
static gboolean button_press_cb (GtkWidget * widget, GdkEventButton * event,
 ListModel * model)
{
    // Get the clicked row
    GtkTreePath * path = nullptr;
    gtk_tree_view_get_path_at_pos ((GtkTreeView *) widget, event->x, event->y,
     & path, nullptr, nullptr, nullptr);

    // Check for right-click (button 3)
    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        // Only allow GTK to select this row if it is not already selected
        if (path)
        {
            if (PATH_IS_SELECTED (widget, path))
                model->frozen = true;  // Don't clear selection
            gtk_tree_view_set_cursor ((GtkTreeView *) widget, path, nullptr, false);
            model->frozen = false;
        }

        // Call custom right_click callback if registered
        if (MODEL_HAS_CB (model, right_click))
            model->cbs->right_click (model->user, event);
        else
        {
            // Default: show playlist context menu
            extern void audgui_playlist_right_click (void * user, GdkEventButton * event);
            audgui_playlist_right_click (widget, event);
        }

        if (path)
            gtk_tree_path_free (path);
        return true;  // Event handled
    }
    
    // ... handle other button events ...
}
```

### Context Menu Creation

**Location:** [src/libaudgui/playlist-context.cc](src/libaudgui/playlist-context.cc)

#### Function: `audgui_playlist_context_menu()`
```cpp
EXPORT GtkWidget * audgui_playlist_context_menu (Playlist playlist, int entry)
{
    GtkWidget * menu = gtk_menu_new ();
    
    /* Stop after this song */
    GtkWidget * item = gtk_menu_item_new_with_mnemonic (_("_Stop After This Song"));
    gtk_widget_show (item);
    gtk_menu_shell_append ((GtkMenuShell *) menu, item);
    
    /* Store playlist and entry as user data */
    Playlist * playlist_ptr = new Playlist(playlist);
    g_object_set_data_full ((GObject *) item, "playlist", 
                            playlist_ptr,
                            [] (gpointer data) { delete (Playlist *)data; });
    g_object_set_data ((GObject *) item, "entry", GINT_TO_POINTER(entry));
    
    g_signal_connect (item, "activate", (GCallback) stop_after_this, playlist_ptr);
    
    return menu;
}
```

#### Default Right-Click Handler: `audgui_playlist_right_click()`
```cpp
/* Default right-click handler for playlist lists */
static void playlist_right_click (void * user, GdkEventButton * event)
{
    GtkWidget * list = (GtkWidget *)user;
    
    /* Get the clicked row */
    int row = audgui_list_row_at_point(list, event->x, event->y);
    if (row < 0)
        return;
    
    /* Get the active playlist */
    Playlist playlist = Playlist::active_playlist();
    if (!playlist.exists())
        return;
    
    /* Create and show the context menu */
    GtkWidget * menu = audgui_playlist_context_menu(playlist, row);
    gtk_menu_popup_at_pointer((GtkMenu *)menu, (GdkEvent *)event);
}

EXPORT void audgui_playlist_right_click (void * user, GdkEventButton * event)
{
    playlist_right_click(user, event);
}
```

---

## 5. Usage Examples

### Queue Manager Window
**Location:** [src/libaudgui/queue-manager.cc](src/libaudgui/queue-manager.cc#L172-L210)

```cpp
static GtkWidget * create_queue_manager ()
{
    int dpi = audgui_get_dpi ();

    GtkWidget * qm_win = gtk_dialog_new ();
    gtk_window_set_title ((GtkWindow *) qm_win, _("Queue Manager"));
    gtk_window_set_role ((GtkWindow *) qm_win, "queue-manager");
    gtk_window_set_default_size ((GtkWindow *) qm_win, 3 * dpi, 2 * dpi);

    GtkWidget * vbox = gtk_dialog_get_content_area ((GtkDialog *) qm_win);

    GtkWidget * scrolled = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scrolled, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scrolled,
     GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start ((GtkBox *) vbox, scrolled, true, true, 0);

    // Create the playlist list widget
    int count = Playlist::active_playlist ().n_queued ();
    GtkWidget * qm_list = audgui_list_new (& callbacks, nullptr, count);
    gtk_tree_view_set_headers_visible ((GtkTreeView *) qm_list, false);
    
    // Add columns
    audgui_list_add_column (qm_list, nullptr, 0, G_TYPE_INT, 7);
    audgui_list_add_column (qm_list, nullptr, 1, G_TYPE_STRING, -1);
    gtk_container_add ((GtkContainer *) scrolled, qm_list);

    // Add buttons
    GtkWidget * button1 = audgui_button_new (_("_Unqueue"), "list-remove", remove_selected, nullptr);
    GtkWidget * button2 = audgui_button_new (_("_Close"), "window-close",
     (AudguiCallback) gtk_widget_destroy, qm_win);

    gtk_dialog_add_action_widget ((GtkDialog *) qm_win, button1, GTK_RESPONSE_NONE);
    gtk_dialog_add_action_widget ((GtkDialog *) qm_win, button2, GTK_RESPONSE_NONE);

    return qm_win;
}
```

---

## 6. Signal Handlers Registered by audgui_list_new()

**Location:** [src/libaudgui/list.cc](src/libaudgui/list.cc#L608-L650)

| Signal | Handler Function | Purpose |
|--------|------------------|---------|
| `destroy` | `destroy_cb` | Cleanup model when list is destroyed |
| `cursor-changed` | `focus_cb` | Notify about keyboard focus changes |
| `row-activated` | `activate_cb` | Handle double-click or Enter activation |
| `button-press-event` | `button_press_cb` | Handle mouse button presses (right-click) |
| `button-release-event` | `button_release_cb` | Handle mouse button releases |
| `key-press-event` | `key_press_cb` | Handle keyboard input |
| `motion-notify-event` | `motion_notify_cb` | Handle mouse movement |
| `leave-notify-event` | `leave_notify_cb` | Handle mouse leaving widget |
| `changed` (selection) | `select_cb` | Handle selection changes |
| `drag-begin` | `drag_begin` | Handle drag start |
| `drag-end` | `drag_end` | Handle drag end |
| `drag-motion` | `drag_motion` | Handle drag over |
| `drag-leave` | `drag_leave` | Handle drag leave |
| `drag-drop` | `drag_drop` | Handle drop |
| `drag-data-get` | `drag_data_get` | Provide drag data |
| `drag-data-received` | `drag_data_received` | Handle dropped data |

---

## 7. Column Addition

**Function:** `audgui_list_add_column()`
**Location:** [src/libaudgui/list.cc](src/libaudgui/list.cc#L690+)

```cpp
EXPORT void audgui_list_add_column (
    GtkWidget * list,           // The tree view widget
    const char * title,         // Column header title
    int column,                 // Column index
    GType type,                 // GTK type (G_TYPE_STRING, G_TYPE_INT, etc.)
    int width,                  // Column width in characters (-1 = expand)
    bool use_markup             // Whether to use markup (bold, color, etc.)
)
```

### Features:
- Dynamically adds columns after creation
- Uses `GtkCellRendererText` for rendering
- Supports markup for formatting
- Can highlight rows based on `PANGO_WEIGHT_BOLD`
- Handles column resizing and sizing modes
- Width parameter controls expansion behavior

---

## 8. Key Features of the audgui_list System

### 1. **Abstraction Layer**
- Provides a higher-level GTK interface
- Hides complexity of GtkTreeModel
- Allows code to work with simple callbacks

### 2. **Flexible Data Source**
- Model itself doesn't store data
- All data comes from callbacks (`get_value`)
- Allows displaying dynamic data without copying

### 3. **Selection Management**
- Supports single and multiple selection
- Optional custom selection callbacks
- Automatic selection updates

### 4. **Drag & Drop Support**
- Built-in drag source and drop destination
- Can be enabled via callbacks
- Supports custom data types

### 5. **Focus & Activation**
- Keyboard navigation support
- Double-click activation
- Cursor change notifications

### 6. **Performance Optimization**
- Fixed height mode for fast rendering
- Character width-based column sizing
- Frozen state during selection changes

---

## 9. Interaction with Playlists

### Right-Click Callback Chain:
1. User right-clicks on playlist entry
2. `button_press_cb()` detects button 3 press
3. Sets cursor to clicked row (respecting multi-selection)
4. If widget has custom `right_click` callback → calls it
5. Otherwise → calls `audgui_playlist_right_click()` (default)
6. `audgui_playlist_right_click()` determines clicked row
7. Creates context menu via `audgui_playlist_context_menu()`
8. Shows menu at mouse position with `gtk_menu_popup_at_pointer()`

### Context Menu Items:
Currently includes:
- **"Stop After This Song"** - Sets stop point after selected entry

The menu structure is extensible for adding more items.

---

## 10. Architecture Summary

```
┌─────────────────────────────────────────┐
│  Caller (e.g., queue-manager.cc)        │
│  Wants: Playlist display widget         │
└────────────────┬────────────────────────┘
                 │ audgui_list_new(&callbacks, user, rows)
                 ▼
┌─────────────────────────────────────────┐
│  audgui_list_new_real()                 │
│  - Creates ListModel (GObject)          │
│  - Creates GtkTreeView                  │
│  - Registers signal handlers            │
└────────────────┬────────────────────────┘
                 │
                 ▼
        ┌────────────────────┐
        │    GtkTreeView     │
        │ ┌────────────────┐ │
        │ │  ListModel     │ │
        │ │ (GtkTreeModel) │ │
        │ └────────────────┘ │
        │ - Implements       │
        │   get_value →      │
        │   Callback         │
        └────────────────────┘
                 │
         Right-click event
                 │
                 ▼
        ┌────────────────────┐
        │ button_press_cb()  │
        │ (in list.cc)       │
        └────────┬───────────┘
                 │
          Has custom callback?
         ┌────────┴────────┐
         │ YES             │ NO
         │                 │
         ▼                 ▼
      Callback    audgui_playlist_right_click()
                         │
                         ▼
                 Create context menu
                (audgui_playlist_context_menu)
                         │
                         ▼
                    Show with GTK
```

---

## 11. Files Modified for "Stop After" Feature

The "Stop After This Song" feature was implemented across:

1. **[src/libaudcore/drct.h](src/libaudcore/drct.h)** - Declares `aud_drct_pl_set_stop_after()`
2. **[src/libaudcore/drct.cc](src/libaudcore/drct.cc)** - Implements stop-after logic
3. **[src/libaudcore/playback.cc](src/libaudcore/playback.cc)** - Checks stop-after condition
4. **[src/libaudgui/playlist-context.cc](src/libaudgui/playlist-context.cc)** - Context menu UI
5. **[src/libaudgui/libaudgui.h](src/libaudgui/libaudgui.h)** - Declares public functions
6. **[src/libaudgui/meson.build](src/libaudgui/meson.build)** - Added to build sources

---

## 12. Related Functions

### Column Sizing
- `audgui_list_add_column()` - Add a column with custom width
- `audgui_get_digit_width()` - Get character width for sizing

### Selection Management
- `audgui_list_get_highlight()` - Get currently highlighted row
- `audgui_list_set_highlight()` - Set highlighted row
- `audgui_list_get_focus()` - Get focused row
- `audgui_list_set_focus()` - Set focused row

### Row Management
- `audgui_list_row_count()` - Get number of rows
- `audgui_list_insert_rows()` - Insert rows
- `audgui_list_update_rows()` - Update rows
- `audgui_list_delete_rows()` - Delete rows
- `audgui_list_row_at_point()` - Get row at screen position

### User Data
- `audgui_list_get_user()` - Get user data pointer

---

## Conclusion

Audacious uses a sophisticated abstraction layer (`audgui_list_new()`) built on top of **GtkTreeView** to display playlist entries. The system:

- Decouples data storage from UI presentation
- Provides flexible callback-based interaction
- Handles context menus through an extensible system
- Supports rich features like drag-and-drop and multi-selection
- Maintains backward compatibility through callback struct versioning

The main window's playlist view is created using this same `audgui_list_new()` function, making it consistent with other list views throughout the application.
