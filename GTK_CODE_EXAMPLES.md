# Code Examples: Using audgui_list_new() for Playlist Display

This document provides practical code examples for creating and managing playlist views using Audacious's `audgui_list_new()` function.

---

## Table of Contents
1. [Basic Playlist View](#basic-playlist-view)
2. [With Selection Support](#with-selection-support)
3. [With Right-Click Menu](#with-right-click-menu)
4. [Real Example: Queue Manager](#real-example-queue-manager)
5. [Custom Right-Click Handler](#custom-right-click-handler)
6. [Complete Example](#complete-example)

---

## Basic Playlist View

### Minimal Example: Display-Only List

```cpp
#include <gtk/gtk.h>
#include "list.h"
#include "libaudgui.h"

// Required callback: get cell value
static void get_value_cb(void * user, int row, int column, GValue * value)
{
    // user = pointer to your data
    MyPlaylistData * data = (MyPlaylistData *)user;
    
    g_value_init(value, G_TYPE_STRING);
    
    switch (column) {
        case 0:  // Title column
            g_value_set_string(value, data->songs[row].title);
            break;
        case 1:  // Artist column
            g_value_set_string(value, data->songs[row].artist);
            break;
        case 2:  // Duration column
            g_value_set_string(value, data->songs[row].duration);
            break;
    }
}

// Create the widget
GtkWidget * create_playlist_view(MyPlaylistData * playlist_data)
{
    // Define callbacks
    static AudguiListCallbacks callbacks = {
        .get_value = get_value_cb,
        // All other callbacks NULL (optional)
    };
    
    // Create list widget
    int num_songs = playlist_data->num_songs;
    GtkWidget * list = audgui_list_new(&callbacks, playlist_data, num_songs);
    
    // Add columns
    audgui_list_add_column(list, "Title",    0, G_TYPE_STRING, 50);
    audgui_list_add_column(list, "Artist",   1, G_TYPE_STRING, 30);
    audgui_list_add_column(list, "Duration", 2, G_TYPE_STRING, 10);
    
    // Wrap in scrolled window
    GtkWidget * scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type((GtkScrolledWindow *)scrolled, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy((GtkScrolledWindow *)scrolled,
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add((GtkContainer *)scrolled, list);
    
    return scrolled;
}
```

---

## With Selection Support

### Multi-Select Playlist

```cpp
#include <gtk/gtk.h>
#include "list.h"
#include "libaudgui.h"
#include <libaudcore/playlist.h>

// Data structure
struct PlaylistState {
    Playlist playlist;
    int num_entries;
};

// get_value callback (same as before)
static void get_value_cb(void * user, int row, int column, GValue * value)
{
    PlaylistState * state = (PlaylistState *)user;
    Playlist pl = state->playlist;
    
    g_value_init(value, G_TYPE_STRING);
    
    String title = pl.entry_title(row);
    g_value_set_string(value, title);
}

// Selection callbacks
static bool get_selected_cb(void * user, int row)
{
    PlaylistState * state = (PlaylistState *)user;
    Playlist pl = state->playlist;
    return pl.entry_selected(row);
}

static void set_selected_cb(void * user, int row, bool selected)
{
    PlaylistState * state = (PlaylistState *)user;
    Playlist pl = state->playlist;
    pl.entry_set_selected(row, selected);
}

static void select_all_cb(void * user, bool selected)
{
    PlaylistState * state = (PlaylistState *)user;
    Playlist pl = state->playlist;
    
    for (int i = 0; i < pl.n_entries(); i++)
        pl.entry_set_selected(i, selected);
}

// Activation callback (double-click)
static void activate_row_cb(void * user, int row)
{
    PlaylistState * state = (PlaylistState *)user;
    Playlist pl = state->playlist;
    
    // Play the selected entry
    pl.set_position(row);
    aud_drct_play();
}

// Create the widget with full support
GtkWidget * create_selectable_playlist(Playlist playlist)
{
    // Create state
    PlaylistState * state = new PlaylistState{
        .playlist = playlist,
        .num_entries = playlist.n_entries()
    };
    
    // Define callbacks with selection support
    static AudguiListCallbacks callbacks = {
        .get_value = get_value_cb,
        .get_selected = get_selected_cb,
        .set_selected = set_selected_cb,
        .select_all = select_all_cb,
        .activate_row = activate_row_cb,
    };
    
    // Create list with selection support
    GtkWidget * list = audgui_list_new(
        &callbacks,                    // Callbacks
        state,                        // User data
        playlist.n_entries()          // Initial row count
    );
    
    // Add column
    audgui_list_add_column(list, "Title", 0, G_TYPE_STRING, -1);
    
    // Wrap and return
    GtkWidget * scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type((GtkScrolledWindow *)scrolled, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy((GtkScrolledWindow *)scrolled,
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add((GtkContainer *)scrolled, list);
    
    return scrolled;
}
```

---

## With Right-Click Menu

### Context Menu Support

```cpp
#include <gtk/gtk.h>
#include "list.h"
#include "libaudgui.h"
#include <libaudcore/playlist.h>

struct PlaylistState {
    Playlist playlist;
};

// Callbacks (get_value, selection, activation from above)
// ...

// Custom right-click handler
static void right_click_cb(void * user, GdkEventButton * event)
{
    PlaylistState * state = (PlaylistState *)user;
    Playlist playlist = state->playlist;
    
    // Get row at click position
    GtkWidget * list = /* need to get from somewhere */;
    int row = audgui_list_row_at_point(list, event->x, event->y);
    
    if (row < 0)
        return;  // Didn't click on a row
    
    // Get the entry
    String title = playlist.entry_title(row);
    
    // Create context menu
    GtkWidget * menu = gtk_menu_new();
    
    // Menu item 1: Queue
    GtkWidget * queue_item = gtk_menu_item_new_with_label("Queue After This");
    g_signal_connect_swapped(queue_item, "activate",
        (GCallback)queue_after_this_cb, GINT_TO_POINTER(row));
    gtk_widget_show(queue_item);
    gtk_menu_shell_append((GtkMenuShell *)menu, queue_item);
    
    // Menu item 2: Remove
    GtkWidget * remove_item = gtk_menu_item_new_with_label("Remove From Playlist");
    Playlist * pl_ptr = new Playlist(playlist);
    g_object_set_data_full((GObject *)remove_item, "playlist", pl_ptr,
        [](gpointer p) { delete (Playlist *)p; });
    g_object_set_data((GObject *)remove_item, "row", GINT_TO_POINTER(row));
    g_signal_connect(remove_item, "activate",
        (GCallback)remove_entry_cb, nullptr);
    gtk_widget_show(remove_item);
    gtk_menu_shell_append((GtkMenuShell *)menu, remove_item);
    
    // Separator
    GtkWidget * sep = gtk_separator_menu_item_new();
    gtk_widget_show(sep);
    gtk_menu_shell_append((GtkMenuShell *)menu, sep);
    
    // Menu item 3: Properties
    GtkWidget * props_item = gtk_menu_item_new_with_label("Properties...");
    g_signal_connect_swapped(props_item, "activate",
        (GCallback)show_properties_cb, GINT_TO_POINTER(row));
    gtk_widget_show(props_item);
    gtk_menu_shell_append((GtkMenuShell *)menu, props_item);
    
    // Show menu at cursor
    gtk_menu_popup_at_pointer((GtkMenu *)menu, (GdkEvent *)event);
}

// Create widget with context menu
GtkWidget * create_playlist_with_menu(Playlist playlist)
{
    PlaylistState * state = new PlaylistState{.playlist = playlist};
    
    static AudguiListCallbacks callbacks = {
        .get_value = get_value_cb,
        .get_selected = get_selected_cb,
        .set_selected = set_selected_cb,
        .select_all = select_all_cb,
        .activate_row = activate_row_cb,
        .right_click = right_click_cb,  // CUSTOM handler
    };
    
    GtkWidget * list = audgui_list_new(&callbacks, state, playlist.n_entries());
    audgui_list_add_column(list, "Title", 0, G_TYPE_STRING, -1);
    
    // Rest as before...
}
```

---

## Real Example: Queue Manager

### From audacious/src/libaudgui/queue-manager.cc

```cpp
// Callbacks
static void get_value_cb(void * user, int row, int column, GValue * value)
{
    Playlist playlist = Playlist::active_playlist();
    
    g_value_init(value, (column == 0) ? G_TYPE_INT : G_TYPE_STRING);
    
    if (column == 0) {
        // Queue position number
        g_value_set_int(value, row + 1);
    } else {
        // Song title
        int entry = playlist.get_queued(row);
        if (entry >= 0 && entry < playlist.n_entries()) {
            String title = playlist.entry_title(entry);
            g_value_set_string(value, title);
        }
    }
}

static bool get_selected_cb(void * user, int row)
{
    return false;  // Single-item selection only
}

static void activate_row_cb(void * user, int row)
{
    Playlist playlist = Playlist::active_playlist();
    int entry = playlist.get_queued(row);
    
    if (entry >= 0)
        playlist.set_position(entry);
}

// Window creation
static GtkWidget * create_queue_manager()
{
    int dpi = audgui_get_dpi();
    
    // Main window
    GtkWidget * qm_win = gtk_dialog_new();
    gtk_window_set_title((GtkWindow *)qm_win, _("Queue Manager"));
    gtk_window_set_role((GtkWindow *)qm_win, "queue-manager");
    gtk_window_set_default_size((GtkWindow *)qm_win, 3 * dpi, 2 * dpi);
    
    GtkWidget * vbox = gtk_dialog_get_content_area((GtkDialog *)qm_win);
    
    // Scrolled window
    GtkWidget * scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type((GtkScrolledWindow *)scrolled, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy((GtkScrolledWindow *)scrolled,
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start((GtkBox *)vbox, scrolled, true, true, 0);
    
    // Create list
    static AudguiListCallbacks callbacks = {
        .get_value = get_value_cb,
        .activate_row = activate_row_cb,
    };
    
    int count = Playlist::active_playlist().n_queued();
    GtkWidget * qm_list = audgui_list_new(&callbacks, nullptr, count);
    gtk_tree_view_set_headers_visible((GtkTreeView *)qm_list, false);
    
    // Add columns
    audgui_list_add_column(qm_list, nullptr, 0, G_TYPE_INT,    7);   // Queue position
    audgui_list_add_column(qm_list, nullptr, 1, G_TYPE_STRING, -1);  // Title
    gtk_container_add((GtkContainer *)scrolled, qm_list);
    
    // Buttons
    GtkWidget * button1 = audgui_button_new(_("_Unqueue"), "list-remove",
        remove_selected, nullptr);
    GtkWidget * button2 = audgui_button_new(_("_Close"), "window-close",
        (AudguiCallback)gtk_widget_destroy, qm_win);
    
    gtk_dialog_add_action_widget((GtkDialog *)qm_win, button1, GTK_RESPONSE_NONE);
    gtk_dialog_add_action_widget((GtkDialog *)qm_win, button2, GTK_RESPONSE_NONE);
    
    // Connect signals
    g_signal_connect(qm_win, "destroy", (GCallback)destroy_cb, nullptr);
    
    return qm_win;
}

EXPORT void audgui_queue_manager_show()
{
    if (!audgui_reshow_unique_window(AUDGUI_QUEUE_MANAGER_WINDOW))
        audgui_show_unique_window(AUDGUI_QUEUE_MANAGER_WINDOW, 
            create_queue_manager());
}
```

---

## Custom Right-Click Handler

### Full Implementation Example

```cpp
// Menu item callbacks
static void on_queue_after_this(GtkWidget * item, void * user)
{
    int row = GPOINTER_TO_INT(user);
    Playlist playlist = Playlist::active_playlist();
    
    // Add this entry to queue
    int entry = playlist.get_queued(row);
    if (entry >= 0)
        playlist.queue_insert(row + 1, entry);
}

static void on_remove_entry(GtkWidget * item, void * user)
{
    Playlist * pl_ptr = (Playlist *)g_object_get_data((GObject *)item, "playlist");
    int row = GPOINTER_TO_INT(g_object_get_data((GObject *)item, "row"));
    
    if (pl_ptr && pl_ptr->exists())
        pl_ptr->remove_entry(row);
}

static void on_show_properties(GtkWidget * item, void * user)
{
    int row = GPOINTER_TO_INT(user);
    Playlist playlist = Playlist::active_playlist();
    
    audgui_infowin_show(playlist, row);
}

// Right-click handler
static void right_click_handler(void * user, GdkEventButton * event)
{
    // Get the list widget
    GtkWidget * list = (GtkWidget *)user;
    
    // Get row at click position
    int row = audgui_list_row_at_point(list, event->x, event->y);
    if (row < 0)
        return;
    
    Playlist playlist = Playlist::active_playlist();
    if (!playlist.exists())
        return;
    
    // Create menu
    GtkWidget * menu = gtk_menu_new();
    
    // Queue option
    GtkWidget * queue_item = gtk_menu_item_new_with_label("Queue After This");
    g_signal_connect(queue_item, "activate", (GCallback)on_queue_after_this,
        GINT_TO_POINTER(row));
    gtk_widget_show(queue_item);
    gtk_menu_shell_append((GtkMenuShell *)menu, queue_item);
    
    // Remove option
    GtkWidget * remove_item = gtk_menu_item_new_with_label("Remove");
    Playlist * pl_ptr = new Playlist(playlist);
    g_object_set_data_full((GObject *)remove_item, "playlist", pl_ptr,
        [](gpointer p) { delete (Playlist *)p; });
    g_object_set_data((GObject *)remove_item, "row", GINT_TO_POINTER(row));
    g_signal_connect(remove_item, "activate", (GCallback)on_remove_entry, nullptr);
    gtk_widget_show(remove_item);
    gtk_menu_shell_append((GtkMenuShell *)menu, remove_item);
    
    // Properties option
    GtkWidget * props_item = gtk_menu_item_new_with_label("Properties...");
    g_signal_connect(props_item, "activate", (GCallback)on_show_properties,
        GINT_TO_POINTER(row));
    gtk_widget_show(props_item);
    gtk_menu_shell_append((GtkMenuShell *)menu, props_item);
    
    // Show menu
    gtk_menu_popup_at_pointer((GtkMenu *)menu, (GdkEvent *)event);
}

// Set the handler
static AudguiListCallbacks callbacks = {
    .get_value = get_value_cb,
    .right_click = right_click_handler,  // Custom handler
};
```

---

## Complete Example

### Full Playlist Window with All Features

```cpp
#include <gtk/gtk.h>
#include "list.h"
#include "libaudgui.h"
#include <libaudcore/playlist.h>
#include <libaudcore/drct.h>

// Data structure
struct PlaylistWindow {
    Playlist playlist;
    GtkWidget * list;
    GtkWidget * window;
};

// ===== CALLBACKS =====

static void get_value_cb(void * user, int row, int column, GValue * value)
{
    PlaylistWindow * win = (PlaylistWindow *)user;
    Playlist playlist = win->playlist;
    
    g_value_init(value, G_TYPE_STRING);
    
    if (row < 0 || row >= playlist.n_entries())
        return;
    
    switch (column) {
        case 0: {
            // Track number
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", row + 1);
            g_value_set_string(value, buf);
            break;
        }
        case 1: {
            // Title
            String title = playlist.entry_title(row);
            g_value_set_string(value, title);
            break;
        }
        case 2: {
            // Artist
            String artist = playlist.entry_artist(row);
            g_value_set_string(value, artist);
            break;
        }
    }
}

static bool get_selected_cb(void * user, int row)
{
    PlaylistWindow * win = (PlaylistWindow *)user;
    return win->playlist.entry_selected(row);
}

static void set_selected_cb(void * user, int row, bool selected)
{
    PlaylistWindow * win = (PlaylistWindow *)user;
    win->playlist.entry_set_selected(row, selected);
}

static void select_all_cb(void * user, bool selected)
{
    PlaylistWindow * win = (PlaylistWindow *)user;
    Playlist pl = win->playlist;
    
    for (int i = 0; i < pl.n_entries(); i++)
        pl.entry_set_selected(i, selected);
}

static void activate_row_cb(void * user, int row)
{
    PlaylistWindow * win = (PlaylistWindow *)user;
    Playlist pl = win->playlist;
    
    // Jump to and play the entry
    pl.set_position(row);
    aud_drct_play();
}

static void right_click_cb(void * user, GdkEventButton * event)
{
    PlaylistWindow * win = (PlaylistWindow *)user;
    int row = audgui_list_row_at_point(win->list, event->x, event->y);
    
    if (row < 0)
        return;
    
    // Create menu
    GtkWidget * menu = gtk_menu_new();
    
    // Play item
    GtkWidget * play_item = gtk_menu_item_new_with_label("Play This");
    g_signal_connect(play_item, "activate", (GCallback)[](GtkWidget *, gpointer user) {
        PlaylistWindow * w = (PlaylistWindow *)user;
        int r = audgui_list_get_focus(w->list);
        w->playlist.set_position(r);
        aud_drct_play();
    }, win);
    gtk_widget_show(play_item);
    gtk_menu_shell_append((GtkMenuShell *)menu, play_item);
    
    // Remove item
    GtkWidget * remove_item = gtk_menu_item_new_with_label("Remove This Entry");
    g_signal_connect(remove_item, "activate", (GCallback)[](GtkWidget *, gpointer user) {
        PlaylistWindow * w = (PlaylistWindow *)user;
        int r = audgui_list_get_focus(w->list);
        w->playlist.remove_entry(r);
    }, win);
    gtk_widget_show(remove_item);
    gtk_menu_shell_append((GtkMenuShell *)menu, remove_item);
    
    // Show menu
    gtk_menu_popup_at_pointer((GtkMenu *)menu, (GdkEvent *)event);
}

static void on_window_destroy(GtkWidget * widget, PlaylistWindow * win)
{
    delete win;
}

// ===== CREATION =====

GtkWidget * create_playlist_window(Playlist playlist)
{
    PlaylistWindow * win = new PlaylistWindow{
        .playlist = playlist,
        .list = nullptr,
        .window = nullptr
    };
    
    int dpi = audgui_get_dpi();
    
    // Main window
    GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title((GtkWindow *)window, _("Playlist"));
    gtk_window_set_role((GtkWindow *)window, "playlist");
    gtk_window_set_default_size((GtkWindow *)window, 6 * dpi, 4 * dpi);
    win->window = window;
    
    // Main container
    GtkWidget * vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width((GtkContainer *)vbox, 6);
    gtk_container_add((GtkContainer *)window, vbox);
    
    // Scrolled window
    GtkWidget * scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type((GtkScrolledWindow *)scrolled, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy((GtkScrolledWindow *)scrolled,
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start((GtkBox *)vbox, scrolled, true, true, 0);
    
    // Create list
    static AudguiListCallbacks callbacks = {
        .get_value = get_value_cb,
        .get_selected = get_selected_cb,
        .set_selected = set_selected_cb,
        .select_all = select_all_cb,
        .activate_row = activate_row_cb,
        .right_click = right_click_cb,
    };
    
    GtkWidget * list = audgui_list_new(&callbacks, win, playlist.n_entries());
    win->list = list;
    
    // Add columns
    audgui_list_add_column(list, "#",      0, G_TYPE_STRING, 5);
    audgui_list_add_column(list, "Title",  1, G_TYPE_STRING, 40);
    audgui_list_add_column(list, "Artist", 2, G_TYPE_STRING, 30);
    
    gtk_container_add((GtkContainer *)scrolled, list);
    gtk_widget_show_all(vbox);
    
    // Cleanup signal
    g_signal_connect(window, "destroy", (GCallback)on_window_destroy, win);
    
    return window;
}
```

---

## Tips & Best Practices

### 1. Memory Management
```cpp
// ✓ CORRECT: Use smart pointers for complex data
struct PlaylistState {
    Playlist playlist;  // Playlist handles its own refcount
};

// ✓ CORRECT: Use g_object_set_data_full() for cleanup
Playlist * pl = new Playlist(active);
g_object_set_data_full((GObject *)item, "playlist", pl,
    [](gpointer p) { delete (Playlist *)p; });

// ✗ WRONG: Don't use C arrays with g_object_set_data
char * data = new char[100];
g_object_set_data(..., data);  // Will leak!
```

### 2. Row Indices
```cpp
// ✓ CORRECT: Always check bounds
int row = audgui_list_row_at_point(list, x, y);
if (row < 0) return;  // No row at this position

// ✓ CORRECT: Use audgui_list_row_count()
int total = audgui_list_row_count(list);
for (int i = 0; i < total; i++) { ... }

// ✗ WRONG: Don't assume rows are stable during callbacks
```

### 3. Selection State
```cpp
// ✓ CORRECT: Check selection in callback
static bool get_selected_cb(void * user, int row) {
    return my_data->is_selected[row];
}

// ✓ CORRECT: Update internal state when requested
static void set_selected_cb(void * user, int row, bool selected) {
    my_data->is_selected[row] = selected;
}

// ✗ WRONG: Don't modify list directly in callback
```

### 4. Context Menu
```cpp
// ✓ CORRECT: Determine row from event position
int row = audgui_list_row_at_point(list, event->x, event->y);

// ✓ CORRECT: Store data in menu item
g_object_set_data((GObject *)item, "row", GINT_TO_POINTER(row));

// ✗ WRONG: Don't rely on cursor position
// (user might right-click on unselected row)
```

### 5. Column Types
```cpp
// Supported GTypes
audgui_list_add_column(list, "Title",    0, G_TYPE_STRING, 50);  // Text
audgui_list_add_column(list, "#",        1, G_TYPE_INT,    5);   // Numbers
audgui_list_add_column(list, "Description", 2, G_TYPE_STRING, -1, true); // Markup
```

---

## Summary

| Task | Function | Location |
|------|----------|----------|
| Create list | `audgui_list_new()` | [list.h#L60](src/libaudgui/list.h#L60) |
| Add column | `audgui_list_add_column()` | [list.cc#L690](src/libaudgui/list.cc#L690) |
| Get row at point | `audgui_list_row_at_point()` | [list.cc](src/libaudgui/list.cc) |
| Get selected rows | `get_selected` callback | [list.h#L35](src/libaudgui/list.h#L35) |
| Set selected rows | `set_selected` callback | [list.h#L36](src/libaudgui/list.h#L36) |
| Handle double-click | `activate_row` callback | [list.h#L41](src/libaudgui/list.h#L41) |
| Handle right-click | `right_click` callback | [list.h#L42](src/libaudgui/list.h#L42) |
| Show context menu | `gtk_menu_popup_at_pointer()` | GTK docs |
| Update rows | `audgui_list_update_rows()` | [list.cc](src/libaudgui/list.cc) |
