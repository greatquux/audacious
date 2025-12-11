# Right-Click Callback Analysis for Audacious

## Overview
This document provides a comprehensive analysis of where and how the `right_click` callback function is defined, registered, and implemented in the Audacious codebase.

---

## 1. Callback Structure Definition

### File: [src/libaudgui/list.h](src/libaudgui/list.h#L34)
**Line: 34-47**

```cpp
struct AudguiListCallbacks {
    void (* get_value) (void * user, int row, int column, GValue * value);

    /* selection (optional) */
    bool (* get_selected) (void * user, int row);
    void (* set_selected) (void * user, int row, bool selected);
    void (* select_all) (void * user, bool selected);

    void (* activate_row) (void * user, int row); /* optional */
    void (* right_click) (void * user, GdkEventButton * event); /* optional */
    void (* shift_rows) (void * user, int row, int before); /* optional */

    /* cross-widget drag and drop (optional) */
    const char * data_type;
    Index<char> (* get_data) (void * user);
    void (* receive_data) (void * user, int row, const char * data, int len);

    void (* mouse_motion) (void * user, GdkEventMotion * event, int row); /* optional */
    void (* mouse_leave) (void * user, GdkEventMotion * event, int row); /* optional */

    void (* focus_change) (void * user, int row); /* optional */
};
```

**Key Point**: `right_click` is an optional callback that receives:
- `void * user` - user-provided data pointer
- `GdkEventButton * event` - the GTK button event with coordinates and button information

---

## 2. Right-Click Callback Invocation

### File: [src/libaudgui/list.cc](src/libaudgui/list.cc#L250-L268)
**Lines: 250-268 (in button_press_cb function)**

```cpp
static gboolean button_press_cb (GtkWidget * widget, GdkEventButton * event,
 ListModel * model)
{
    GtkTreePath * path = nullptr;
    gtk_tree_view_get_path_at_pos ((GtkTreeView *) widget, event->x, event->y,
     & path, nullptr, nullptr, nullptr);

    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        /* Only allow GTK to select this row if it is not already selected.  We
         * don't want to clear a multiple selection. */
        if (path)
        {
            if (PATH_IS_SELECTED (widget, path))
                model->frozen = true;
            gtk_tree_view_set_cursor ((GtkTreeView *) widget, path, nullptr, false);
            model->frozen = false;
        }

        if (MODEL_HAS_CB (model, right_click))
            model->cbs->right_click (model->user, event);
        else
        {
            /* Default: show playlist context menu */
            extern void audgui_playlist_right_click (void * user, GdkEventButton * event);
            audgui_playlist_right_click (widget, event);
        }

        if (path)
            gtk_tree_path_free (path);
        return true;
    }
    // ... rest of function
}
```

**How it works**:
1. Detects right-click (button 3) on list widget
2. Sets cursor to clicked row without clearing selection
3. **If a custom right_click callback is registered**: Calls `model->cbs->right_click(model->user, event)`
4. **If no custom callback**: Falls back to default `audgui_playlist_right_click()`

### Helper Macro: [src/libaudgui/list.cc](src/libaudgui/list.cc#L35-L37)
**Lines: 35-37**

```cpp
#define MODEL_HAS_CB(m, cb) \
 ((m)->cbs_size > (int) offsetof (AudguiListCallbacks, cb) && (m)->cbs->cb)
```

This macro checks if a callback function is both:
- Present in the callbacks struct based on its size
- Non-null

---

## 3. Default Right-Click Implementation

### File: [src/libaudgui/playlist-context.cc](src/libaudgui/playlist-context.cc)

#### 3a. Default Handler Function
**Lines: 61-77**

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
```

This function:
1. Converts the `user` pointer to a `GtkWidget` (the list widget)
2. Gets the row index at the click point
3. Gets the active playlist
4. Creates the context menu
5. Displays it at the pointer location

#### 3b. Public Export Wrapper
**Lines: 79-82**

```cpp
EXPORT void audgui_playlist_right_click (void * user, GdkEventButton * event)
{
    playlist_right_click(user, event);
}
```

This is the public function that's declared in the header and called as a fallback.

#### 3c. Context Menu Creation
**Lines: 39-58**

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

This creates the context menu with:
- A "Stop After This Song" menu item
- Signal connections for menu item activation
- User data storage for callback handlers

---

## 4. Public API Declaration

### File: [src/libaudgui/libaudgui.h](src/libaudgui/libaudgui.h#L85-L91)
**Lines: 85-91**

```cpp
/* playlist-context.c */
/* Creates a context menu for a selected playlist entry. The caller must
 * populate and display the menu. Returns a new GtkMenu. */
GtkWidget * audgui_playlist_context_menu (Playlist playlist, int entry);
/* Default right-click handler for playlist list widgets */
void audgui_playlist_right_click (void * user, GdkEventButton * event);
```

---

## 5. List Widget Creation with Callbacks

### File: [src/libaudgui/eq-preset.cc](src/libaudgui/eq-preset.cc#L89-L104)
**Lines: 89-104** (Example with NULL right_click)

```cpp
static const AudguiListCallbacks callbacks = {
    get_value,
    get_selected,
    set_selected,
    select_all,
    activate_row,
    nullptr, // right_click
    nullptr, // shift_rows
    nullptr, // data_type
    nullptr, // get_data
    nullptr, // receive_data
    nullptr, // mouse_motion
    nullptr, // mouse_leave
    focus_change
};
```

### File: [src/libaudgui/queue-manager.cc](src/libaudgui/queue-manager.cc#L100-L107)
**Lines: 100-107** (Example with NULL right_click)

```cpp
static const AudguiListCallbacks callbacks = {
    get_value,
    get_selected,
    set_selected,
    select_all,
    0,  // activate_row
    0,  // right_click
    shift_rows
};
```

### File: [src/libaudgui/jump-to-track.cc](src/libaudgui/jump-to-track.cc#L237-L239)
**Lines: 237-239** (Minimal callbacks - only get_value)

```cpp
static const AudguiListCallbacks callbacks = {
    list_get_value
};
```

When `right_click` is `nullptr` (or omitted), the default handler `audgui_playlist_right_click` is invoked.

### List Creation Calls:
- [eq-preset.cc](src/libaudgui/eq-preset.cc#L320): `list = audgui_list_new (& callbacks, nullptr, preset_list.len ());`
- [queue-manager.cc](src/libaudgui/queue-manager.cc#L180): Similar pattern
- [jump-to-track.cc](src/libaudgui/jump-to-track.cc#L260): `treeview = audgui_list_new (& callbacks, nullptr, 0);`

---

## 6. Signal Flow Diagram

```
User right-clicks on list widget
           ↓
GTK emits "button_press_event"
           ↓
list.cc: button_press_cb() detects button == 3
           ↓
Check if custom right_click callback exists (MODEL_HAS_CB)
           ↓
    ┌──────┴──────┐
    ↓             ↓
YES: Call custom     NO: Call default
callback            audgui_playlist_right_click()
    ↓                       ↓
Custom handler      playlist_right_click()
logic                       ↓
                   Get row at point
                            ↓
                   Get active playlist
                            ↓
                   audgui_playlist_context_menu()
                            ↓
                   Create GtkMenu with items
                            ↓
                   gtk_menu_popup_at_pointer()
                            ↓
                   Display context menu
```

---

## 7. Summary Table

| Aspect | Location |
|--------|----------|
| **Struct Definition** | [src/libaudgui/list.h:43](src/libaudgui/list.h#L43) |
| **Invocation Logic** | [src/libaudgui/list.cc:256-257](src/libaudgui/list.cc#L256-L257) |
| **Default Handler** | [src/libaudgui/playlist-context.cc:61-77](src/libaudgui/playlist-context.cc#L61-L77) |
| **Public Wrapper** | [src/libaudgui/playlist-context.cc:79-82](src/libaudgui/playlist-context.cc#L79-L82) |
| **Context Menu Creation** | [src/libaudgui/playlist-context.cc:39-58](src/libaudgui/playlist-context.cc#L39-L58) |
| **Header Declaration** | [src/libaudgui/libaudgui.h:85-91](src/libaudgui/libaudgui.h#L85-L91) |
| **Fallback Logic** | [src/libaudgui/list.cc:256-262](src/libaudgui/list.cc#L256-L262) |

---

## 8. Key Implementation Details

### Context Menu Items
Currently implemented:
1. **"Stop After This Song"** - Sets the stop-after marker for the selected entry

The menu is created by `audgui_playlist_context_menu()` and can be extended with additional menu items.

### Callback Signature
```cpp
void (*right_click)(void * user, GdkEventButton * event)
```

- `user`: Widget or user data passed to `audgui_list_new()`
- `event`: GTK button event containing:
  - `event->x`, `event->y`: Cursor coordinates
  - `event->button`: Button number (3 for right-click)
  - `event->type`: Event type (GDK_BUTTON_PRESS)
  - `event->state`: Modifier keys (Shift, Control, etc.)

### Optional vs Required
- `right_click` callback is **optional**
- If not provided, the framework uses a **default handler**
- Other callbacks like `get_value` are **required**

