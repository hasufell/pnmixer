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

#include "ui-popup-menu.h"
#include "debug.h"

#include "support.h"
#include "main.h"
#include "alsa.h"

#ifdef WITH_GTK3
#define POPUP_MENU_UI_FILE "popup-menu-gtk3.glade"
#define ABOUT_UI_FILE      "about-gtk3.glade"
#else
#define POPUP_MENU_UI_FILE "popup-menu-gtk2.glade"
#define ABOUT_UI_FILE      "about-gtk2.glade"
#endif

struct popup_menu {
	GtkMenu *menu;
	GtkWidget *mute_check;
};

static GtkAboutDialog *build_about_dialog(void);

/*
 * Widget signal handlers
 */

/**
 * Propagates the activate signal on mute_item (GtkMenuItem) in the right-click
 * popup menu to the underlying popup_menu_mute_check (GtkCheckButton) as
 * toggled signal.
 */
void
popup_menu_on_mute_clicked(G_GNUC_UNUSED GtkWidget *widget,
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
popup_menu_on_mixer_clicked(G_GNUC_UNUSED GtkMenuItem *item,
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
popup_menu_on_prefs_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                            G_GNUC_UNUSED PopupMenu *menu)
{
	do_open_prefs();
}

void
popup_menu_on_reload_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                             G_GNUC_UNUSED PopupMenu *menu)
{
	do_alsa_reinit();
}

void
popup_menu_on_about_clicked(G_GNUC_UNUSED GtkMenuItem *item,
                            G_GNUC_UNUSED PopupMenu *menu)
{
	GtkAboutDialog *about_dialog;

	about_dialog = build_about_dialog();
	gtk_dialog_run(GTK_DIALOG(about_dialog));
	gtk_widget_destroy(GTK_WIDGET(about_dialog));
}

/*
 * Helpers
 */

static GtkAboutDialog *
build_about_dialog(void)
{
	gchar *uifile;
	GtkBuilder *builder;
	GtkAboutDialog *about;

	uifile = get_ui_file(ABOUT_UI_FILE);
	g_assert(uifile);

	builder = gtk_builder_new_from_file(uifile);

	about = GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "about_dialog"));
	g_assert(about);

	gtk_about_dialog_set_version(about, VERSION);

	gtk_builder_connect_signals(builder, NULL);

	g_object_unref(G_OBJECT(builder));
	g_free(uifile);

	return about;
}

static PopupMenu *
build_popup_menu(void)
{
	gchar *uifile;
	GtkBuilder *builder;
	PopupMenu *menu;

	uifile = get_ui_file(POPUP_MENU_UI_FILE);
	g_assert(uifile);

	DEBUG_PRINT("Building popup menu from ui file '%s'", uifile);
	builder = gtk_builder_new_from_file(uifile);

	menu = g_slice_new(PopupMenu);
	menu->menu = GTK_MENU(gtk_builder_get_object(builder, "popup_menu"));
	menu->mute_check = GTK_WIDGET(gtk_builder_get_object(builder, "mute_check"));

	gtk_builder_connect_signals(builder, menu);

	g_object_unref(G_OBJECT(builder));
	g_free(uifile);

	return menu;
}

static void
destroy_popup_menu(PopupMenu *menu)
{
	gtk_widget_destroy(GTK_WIDGET(menu->menu));
	g_slice_free(PopupMenu, menu);
}

/**
 * Update the mute checkbox according to the current alsa state
 *
 * @param widget the checkbox to update
 */
static void
update_mute_check(PopupMenu *menu)
{
	GtkWidget *mute_check;
	gboolean active;

	mute_check = GTK_WIDGET(menu->mute_check);

	if (ismuted() == 1)
		active = FALSE;
	else
		active = TRUE;

	/* On Gtk3 version, we listen for the signal sent by the GtkMenuItem.
	 * So, when we change the value of the GtkToggleButton, we don't have
	 * to block the signal handler, since there's no signal handler anyway
	 * on the GtkToggleButton.
	 * Yeah, I know it's a little bit tricky, but this is due to the dual
	 * Gtk2/Gtk3 thing. This should be improved for the sake of clarity
	 * when the Gtk2 version is gone.
	 */

#ifndef WITH_GTK3
	gint n_handlers_blocked;
	n_handlers_blocked = g_signal_handlers_block_by_func
		(G_OBJECT(mute_check), popup_menu_on_mute_clicked, menu);
	g_assert(n_handlers_blocked == 1);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mute_check), active);

	g_signal_handlers_unblock_by_func
		(G_OBJECT(mute_check), popup_menu_on_mute_clicked, menu);
#else
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mute_check), active);
#endif
}

/*
 * Public functions
 */

void
popup_menu_show(PopupMenu *menu, GtkStatusIcon *status_icon,
                guint button, guint activate_time)
{
	gtk_menu_popup(menu->menu, NULL, NULL,
	               gtk_status_icon_position_menu, status_icon,
		       button, activate_time);
}

void
popup_menu_update(PopupMenu *menu)
{
	update_mute_check(menu);
}

void
popup_menu_destroy(PopupMenu *menu)
{
	destroy_popup_menu(menu);
}

PopupMenu *
popup_menu_create(void)
{
	return build_popup_menu();
}
