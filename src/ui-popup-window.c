/* popup-window.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file popup-window.c
 * This file holds the ui-related code for the popup window.
 * @brief Popup window subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "ui-popup-window.h"
#include "prefs.h"
#include "debug.h"

#include "support.h"
#include "main.h"
#include "alsa.h"

#ifdef WITH_GTK3
#define POPUP_WINDOW_HORIZONTAL_UI_FILE "popup-window-horizontal-gtk3.glade"
#define POPUP_WINDOW_VERTICAL_UI_FILE   "popup-window-vertical-gtk3.glade"
#else
#define POPUP_WINDOW_HORIZONTAL_UI_FILE "popup-window-horizontal-gtk2.glade"
#define POPUP_WINDOW_VERTICAL_UI_FILE   "popup-window-vertical-gtk2.glade"
#endif

/*
 * Widget signal handlers
 */

/**
 * Hides the volume popup_window, connected via the signals
 * button-press-event, key-press-event and grab-broken-event.
 *
 * @param widget the object which received the signal
 * @param event the GdkEventButton which triggered the signal
 * @param user_data user data set when the signal handler was connected
 * @return TRUE to stop other handlers from being invoked for the evend,
 * FALSE to propagate the event further
 */
gboolean
popup_window_on_event(G_GNUC_UNUSED GtkWidget *widget, GdkEvent *event,
                      PopupWindow *popup_window)
{
	GtkWidget *window = popup_window->window;

	switch (event->type) {

	/* If a click happens outside of the popup, hide it */
	case GDK_BUTTON_PRESS: {
		gint x, y;
#ifdef WITH_GTK3
		GdkDevice *device = gtk_get_current_event_device();

		if (!gdk_device_get_window_at_position(device, &x, &y))
#else
		if (!gdk_window_at_pointer(&x, &y))
#endif
			gtk_widget_hide(window);
		break;
	}

	/* If 'Esc' is pressed, hide popup */
	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_KEY_Escape)
			gtk_widget_hide(window);
		break;

	/* Broken grab, hide popup */
	case GDK_GRAB_BROKEN:
		gtk_widget_hide(window);
		break;

	/* Unhandle event, do nothing */
	default:
		break;
	}

	return FALSE;
}

void
popup_window_on_mute_toggled(G_GNUC_UNUSED GtkToggleButton *button,
                             G_GNUC_UNUSED PopupWindow *window)
{
	do_mute(popup_noti);
}

void
popup_window_on_mixer_pressed(G_GNUC_UNUSED GtkButton *button,
                              G_GNUC_UNUSED PopupWindow *window)
{
	do_mixer();
}

/*
 * Helpers
 */

static PopupWindow *
build_popup_window(void)
{
	gchar *orientation;
	gchar *uifile;
	GtkBuilder *builder;
	PopupWindow *window;

	orientation = prefs_get_string("SliderOrientation", "vertical");

	if (!g_strcmp0(orientation, "horizontal"))
		uifile = get_ui_file(POPUP_WINDOW_HORIZONTAL_UI_FILE);
	else
		uifile = get_ui_file(POPUP_WINDOW_VERTICAL_UI_FILE);
	g_assert(uifile);

	DEBUG_PRINT("Building popup window from ui file '%s'", uifile);
	builder = gtk_builder_new_from_file(uifile);

	window = g_slice_new(PopupWindow);
	window->window = GTK_WIDGET(gtk_builder_get_object(builder, "popup_window"));
	window->vol_scale = GTK_WIDGET(gtk_builder_get_object(builder, "vol_scale"));
	window->vol_adj = GTK_ADJUSTMENT(gtk_builder_get_object(builder, "vol_scale_adjustment"));
	window->mute_check = GTK_WIDGET(gtk_builder_get_object(builder, "mute_check"));

	gtk_builder_connect_signals(builder, window);

	g_object_unref(G_OBJECT(builder));
	g_free(uifile);
	g_free(orientation);

	return window;
}

static void
destroy_popup_window(PopupWindow *window)
{
	gtk_widget_destroy(GTK_WIDGET(window->window));
	g_slice_free(PopupWindow, window);
}

/**
 * Update the visibility and the position of the volume text which is
 * shown around the volume slider.
 *
 * @param window the popup window to update
 */
static void
update_vol_text(PopupWindow *window)
{
	GtkScale *vol_scale;
	gboolean display_text;
	gint text_pos;

	vol_scale = GTK_SCALE(window->vol_scale);

	display_text = prefs_get_boolean("DisplayTextVolume", TRUE);
	text_pos = prefs_get_integer("TextVolumePosition", 0);

	if (display_text) {
		GtkPositionType pos;

		pos =   text_pos == 0 ? GTK_POS_TOP :
			text_pos == 1 ? GTK_POS_BOTTOM :
			text_pos == 2 ? GTK_POS_LEFT :
			GTK_POS_RIGHT;

		gtk_scale_set_draw_value(vol_scale, TRUE);
		gtk_scale_set_value_pos(vol_scale, pos);
	} else
		gtk_scale_set_draw_value(vol_scale, FALSE);
}

/**
 * Update the mute checkbox according to the current alsa state
 *
 * @param window the popup window to update
 */
static void
update_mute_check(PopupWindow *window)
{
	GtkToggleButton *mute_check;
	gint n_handlers_blocked;

	mute_check = GTK_TOGGLE_BUTTON(window->mute_check);

	n_handlers_blocked = g_signal_handlers_block_by_func
		(G_OBJECT(mute_check), popup_window_on_mute_toggled, window);
	g_assert(n_handlers_blocked == 1);

	if (ismuted() == 1)
		gtk_toggle_button_set_active(mute_check, FALSE);
	else
		gtk_toggle_button_set_active(mute_check, TRUE);

	g_signal_handlers_unblock_by_func
		(G_OBJECT(mute_check), popup_window_on_mute_toggled, window);
}

/*
 * Public functions
 */

void
popup_window_update(PopupWindow *window)
{
	update_vol_text(window);
	update_mute_check(window);
}

void
popup_window_destroy(PopupWindow *window)
{
	destroy_popup_window(window);
}

PopupWindow *
popup_window_create(void)
{
	return build_popup_window();
}
