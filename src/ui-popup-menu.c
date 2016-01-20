/* popup-menu.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file popup-menu.c
 * This file holds the ui-related code for the popup menu.
 * @brief Popup menu subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "support.h"
#include "ui-popup-menu.h"
#include "ui-about-dialog.h"

#include "alsa.h"
#include "main.h"

#ifdef WITH_GTK3
#define POPUP_MENU_UI_FILE "popup-menu-gtk3.glade"
#else
#define POPUP_MENU_UI_FILE "popup-menu-gtk2.glade"
#endif

struct popup_menu {
	GtkWidget *menu;
#ifdef WITH_GTK3
	GtkWidget *mute_check;
#else
	GtkWidget *mute_item;
#endif
};

/* Helpers */

/* Updates the mute checkbox according to the current audio state. */
static void
update_mute_check(PopupMenu *menu)
{
	gboolean active;

	if (ismuted() == 1)
		active = FALSE;
	else
		active = TRUE;

#ifdef WITH_GTK3
	/* On Gtk3 version, we listen for the signal sent by the GtkMenuItem.
	 * So, when we change the value of the GtkToggleButton, we don't have
	 * to block the signal handler, since there's no signal handler anyway
	 * on the GtkToggleButton.
	 */
	GtkToggleButton *mute_check;

	mute_check = GTK_TOGGLE_BUTTON(menu->mute_check);
	gtk_toggle_button_set_active(mute_check, active);
#else
	/* On Gtk2 version, we must block the signal sent by the GtkCheckMenuItem
	 * before we update it manually.
	 */
	GtkCheckMenuItem *mute_item;
	gint n_handlers_blocked;

	mute_item = GTK_CHECK_MENU_ITEM(menu->mute_item);

	n_handlers_blocked = g_signal_handlers_block_by_func
		(G_OBJECT(mute_item), on_mute_item_activate, menu);
	g_assert(n_handlers_blocked == 1);

	gtk_check_menu_item_set_active(mute_item, active);

	g_signal_handlers_unblock_by_func
		(G_OBJECT(mute_item), on_mute_item_activate, menu);
#endif
}

/* Signal handlers */

/**
 * Handles a click on 'mute_item', toggling the mute audio state.
 * 
 * @param menuitem the object which received the signal.
 * @param menu PopupMenu instance set when the signal handler was connected.
 */
void
#ifdef WITH_GTK3
on_mute_item_activate(G_GNUC_UNUSED GtkMenuItem *menuitem,
#else
on_mute_item_activate(G_GNUC_UNUSED GtkCheckMenuItem *menuitem,
#endif
                      G_GNUC_UNUSED PopupMenu *menu)
{
	do_mute(popup_noti);
}

/**
 * Handles a click on 'mixer_item', opening the specified mixer application.
 *
 * @param menuitem the object which received the signal.
 * @param menu PopupMenu instance set when the signal handler was connected.
 */
void
on_mixer_item_activate(G_GNUC_UNUSED GtkMenuItem *item,
                       G_GNUC_UNUSED PopupMenu *menu)
{
	do_mixer();
}

/**
 * Handles a click on 'prefs_item', opening the preferences window.
 *
 * @param menuitem the object which received the signal.
 * @param menu PopupMenu instance set when the signal handler was connected.
 */
void
on_prefs_item_activate(G_GNUC_UNUSED GtkMenuItem *item,
                       G_GNUC_UNUSED PopupMenu *menu)
{
	do_open_prefs();
}

/**
 * Handles a click on 'reload_item', re-initializing alsa.
 *
 * @param menuitem the object which received the signal.
 * @param menu PopupMenu instance set when the signal handler was connected.
 */
void
on_reload_item_activate(G_GNUC_UNUSED GtkMenuItem *item,
                        G_GNUC_UNUSED PopupMenu *menu)
{
	do_alsa_reinit();
}

/**
 * Handles a click on 'about_item', opening the About dialog.
 *
 * @param menuitem the object which received the signal.
 * @param menu PopupMenu instance set when the signal handler was connected.
 */
void
on_about_item_activate(G_GNUC_UNUSED GtkMenuItem *item,
                       G_GNUC_UNUSED PopupMenu *menu)
{
	about_dialog_run();
}

/* Public functions */

/**
 * Shows the popup menu.
 * The weird prototype of this function comes from the underlying
 * gtk_menu_popup() that is used to display the popup menu.
 *
 * @param menu a PopupMenu instance.
 * @param func a user supplied function used to position the menu, or NULL.
 * @param data user supplied data to be passed to func.
 * @param button the mouse button which was pressed to initiate the event.
 * @param activate_time the time at which the activation event occurred.
 */
void
popup_menu_show(PopupMenu *menu, GtkMenuPositionFunc func, gpointer data,
                guint button, guint activate_time)
{
	gtk_menu_popup(GTK_MENU(menu->menu), NULL, NULL,
	               func, data, button, activate_time);
}

/**
 * Updates the popup menu according to the audio status.
 * This has to be called after volume has been changed or muted.
 *
 * @param menu a PopupMenu instance.
 */
void
popup_menu_update(PopupMenu *menu)
{
	update_mute_check(menu);
}

/**
 * Destroys the popup menu, freeing any resources.
 *
 * @param menu a PopupMenu instance.
 */
void
popup_menu_destroy(PopupMenu *menu)
{
	gtk_widget_destroy(menu->menu);
	g_free(menu);
}

/**
 * Creates the popup menu and connects all the signals.
 *
 * @return the newly created PopupMenu instance.
 */
PopupMenu *
popup_menu_create(void)
{
	gchar *uifile;
	GtkBuilder *builder;
	PopupMenu *menu;

	menu = g_new0(PopupMenu, 1);

	/* Build UI file */
	uifile = get_ui_file(POPUP_MENU_UI_FILE);
	g_assert(uifile);

	DEBUG("Building popup menu from ui file '%s'", uifile);
	builder = gtk_builder_new_from_file(uifile);

	/* Save some widgets for later use */
	assign_gtk_widget(builder, menu, menu);
#ifdef WITH_GTK3
	assign_gtk_widget(builder, menu, mute_check);
#else
	assign_gtk_widget(builder, menu, mute_item);
#endif

	/* Connect signal handlers */
	gtk_builder_connect_signals(builder, menu);

	/* Cleanup */
	g_object_unref(builder);
	g_free(uifile);

	return menu;
}
