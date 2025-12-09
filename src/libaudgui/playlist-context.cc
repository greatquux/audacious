/*
 * playlist-context.c
 * Copyright 2025
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <gtk/gtk.h>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>

#include "libaudgui.h"
#include "libaudgui-gtk.h"

static void stop_after_this (void * data)
{
    auto user_data = (const Playlist *)data;
    int entry = GPOINTER_TO_INT(g_object_get_data((GObject *)data, "entry"));
    
    /* Set the stop-after target to this entry */
    aud_drct_pl_set_stop_after(user_data->index(), entry);
}

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
