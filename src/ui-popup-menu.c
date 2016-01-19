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

/* Widget signal handlers */

/**
 * Propagates the activate signal on mute_item (GtkMenuItem) in the right-click
 * popup menu to the underlying popup_menu_mute_check (GtkCheckButton) as
 * toggled signal.
 */
void
#ifdef WITH_GTK3
popup_menu_on_mute_activated(G_GNUC_UNUSED GtkMenuItem *widget,
#else
popup_menu_on_mute_activated(G_GNUC_UNUSED GtkCheckMenuItem *widget,
#endif
                             G_GNUC_UNUSED PopupMenu *menu)
{
	do_mute(popup_noti);
}

/**
 * Opens the specified mixer application which can be triggered either
 * by clicking the 'Volume Control' GtkImageMenuItem in the context
 * menu, the GtkButton 'Mixer' in the left-click popup_window or
 * by middle-click if the Middle Click Action in the preferences
 * is set to 'Volume Control'.
 */
void
popup_menu_on_mixer_activated(G_GNUC_UNUSED GtkMenuItem *item,
                              G_GNUC_UNUSED PopupMenu *menu)
{
	do_mixer();
}

/**
 * Brings up the preferences window, either triggered by clicking
 * on the GtkImageMenuItem 'Preferences' in the context menu
 * or by middle-click if the Middle Click Action in the preferences
 * is set to 'Show Preferences'.
 */
void
popup_menu_on_prefs_activated(G_GNUC_UNUSED GtkMenuItem *item,
                              G_GNUC_UNUSED PopupMenu *menu)
{
	do_open_prefs();
}

void
popup_menu_on_reload_activated(G_GNUC_UNUSED GtkMenuItem *item,
                               G_GNUC_UNUSED PopupMenu *menu)
{
	do_alsa_reinit();
}

void
popup_menu_on_about_activated(G_GNUC_UNUSED GtkMenuItem *item,
                              G_GNUC_UNUSED PopupMenu *menu)
{
	about_dialog_run();
}

/* Helpers */

/**
 * Update the mute checkbox according to the current alsa state
 *
 * @param widget the checkbox to update
 */
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
		(G_OBJECT(mute_item), popup_menu_on_mute_activated, menu);
	g_assert(n_handlers_blocked == 1);

	gtk_check_menu_item_set_active(mute_item, active);

	g_signal_handlers_unblock_by_func
		(G_OBJECT(mute_item), popup_menu_on_mute_activated, menu);
#endif
}

/* Public functions */

void
popup_menu_update(PopupMenu *menu)
{
	update_mute_check(menu);
}

void
popup_menu_show(PopupMenu *menu, GtkMenuPositionFunc func, gpointer data,
                guint button, guint activate_time)
{
	gtk_menu_popup(GTK_MENU(menu->menu), NULL, NULL,
	               func, data, button, activate_time);
}

void
popup_menu_destroy(PopupMenu *menu)
{
	gtk_widget_destroy(menu->menu);
	g_free(menu);
}

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
