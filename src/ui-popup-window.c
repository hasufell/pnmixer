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

enum orientation {
	VERTICAL,
	HORIZONTAL
};

static enum orientation slider_orientation;
static gboolean display_volume_text;
static GtkPositionType volume_text_position;

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

/**
 * Callback function when the vol_scale (GtkScale) in the volume
 * popup window received the change-value signal
 * (either via mouse or keyboard).
 *
 * @param range the GtkRange that received the signal
 * @param scroll the type of scroll action that was performed
 * @param value the new value resulting from the scroll action
 * @param user_data user data set when the signal handler was connected
 * @return TRUE to prevent other handlers from being invoked for the
 * signal, FALSE to propagate the signal further
 */

gboolean
on_vol_scale_changed_value(GtkRange *range, G_GNUC_UNUSED GtkScrollType scroll,
                           gdouble value, G_GNUC_UNUSED gpointer user_data)
{
	GtkAdjustment *gtk_adj;
	int volumeset;

	/* We must ensure that the new value meets the requirement
	 * defined by the GtkAdjustment. We have to do that manually,
	 * because at this moment the value within GtkAdjustment
	 * has not been updated yet, so using gtk_adjustment_get_value()
	 * returns a wrong value.
	 */

	gtk_adj = gtk_range_get_adjustment(range);

	if (value < gtk_adjustment_get_lower(gtk_adj))
		value = gtk_adjustment_get_lower(gtk_adj);
	if (value > gtk_adjustment_get_upper(gtk_adj))
		value = gtk_adjustment_get_upper(gtk_adj);

	volumeset = (int) value;

	setvol(volumeset, 0, popup_noti);
	if (ismuted() == 0)
		setmute(popup_noti);

	do_update_ui();

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
	gchar *uifile;
	GtkBuilder *builder;
	PopupWindow *window;

	if (slider_orientation == HORIZONTAL)
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

	vol_scale = GTK_SCALE(window->vol_scale);

	if (display_volume_text) {
		gtk_scale_set_draw_value(vol_scale, TRUE);
		gtk_scale_set_value_pos(vol_scale, volume_text_position);
	} else
		gtk_scale_set_draw_value(vol_scale, FALSE);
}

/**
 * Update the mute checkbox according to the current alsa state
 *
 * @param window the popup window to update
 */
static void
update_mute_check(PopupWindow *window, int muted)
{
	GtkToggleButton *mute_check;
	gint n_handlers_blocked;

	mute_check = GTK_TOGGLE_BUTTON(window->mute_check);

	n_handlers_blocked = g_signal_handlers_block_by_func
		(G_OBJECT(mute_check), popup_window_on_mute_toggled, window);
	g_assert(n_handlers_blocked == 1);

	if (muted)
		gtk_toggle_button_set_active(mute_check, FALSE);
	else
		gtk_toggle_button_set_active(mute_check, TRUE);

	g_signal_handlers_unblock_by_func
		(G_OBJECT(mute_check), popup_window_on_mute_toggled, window);
}

static void
update_volume_slider(PopupWindow *window, int volume)
{
	GtkAdjustment *vol_adj = window->vol_adj;

	gtk_adjustment_set_value(vol_adj, (double) volume);
}

/*
 * Public functions
 */

void
popup_window_toggle(PopupWindow *window)
{
	GtkWidget *popup_window = window->window;
	GtkWidget *vol_scale = window->vol_scale;

	// Ensure the window is up to date
	popup_window_update(window);

	// If already visible, hide it
	if (gtk_widget_get_visible(popup_window)) {
		gtk_widget_hide(popup_window);
		return;
	}

	// Otherwise, show the window
	gtk_widget_show_now(popup_window);

	// Give focus to volume scale
	gtk_widget_grab_focus(vol_scale);

	// TODO is this code really needed ?
#ifdef WITH_GTK3
	GdkDevice *pointer_dev = gtk_get_current_event_device();

	if (pointer_dev == NULL)
		return;
	
	if (gdk_device_grab(pointer_dev,
	                    gtk_widget_get_window(popup_window),
	                    GDK_OWNERSHIP_NONE,
	                    TRUE,
	                    GDK_BUTTON_PRESS_MASK,
	                    NULL,
	                    GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
		g_warning("Could not grab %s\n",
		          gdk_device_get_name(pointer_dev));

	GdkDevice *keyboard_dev = gdk_device_get_associated_device(pointer_dev);

	if (keyboard_dev == NULL)
		return;
				
	if (gdk_device_grab(keyboard_dev,
	                    gtk_widget_get_window(popup_window),
	                    GDK_OWNERSHIP_NONE,
	                    TRUE,
	                    GDK_KEY_PRESS_MASK,
	                    NULL, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
		g_warning("Could not grab %s\n",
		          gdk_device_get_name(keyboard_dev));
#else
	gdk_pointer_grab(gtk_widget_get_window(popup_window), TRUE,
	                 GDK_BUTTON_PRESS_MASK, NULL, NULL, GDK_CURRENT_TIME);	
	gdk_keyboard_grab(gtk_widget_get_window(popup_window), TRUE,
	                  GDK_CURRENT_TIME);
#endif
}

void
popup_window_hide(PopupWindow *window)
{
	gtk_widget_hide(window->window);
}

void
popup_window_update(PopupWindow *window)
{
	int volume = getvol();
	int muted = ismuted();

	update_vol_text(window);
	update_mute_check(window, muted);
	update_volume_slider(window, volume);
}

void
popup_window_reload_prefs(PopupWindow *window)
{
	// slider orientation
	gchar *orientation;
	orientation = prefs_get_string("SliderOrientation", "vertical");
	if (!g_strcmp0(orientation, "horizontal"))
		slider_orientation = HORIZONTAL;
	else
		slider_orientation = VERTICAL;
	g_free(orientation);

	// volume text display
	display_volume_text = prefs_get_boolean("DisplayTextVolume", TRUE);

	// volume text position
	gint position;
	position = prefs_get_integer("TextVolumePosition", 0);
	volume_text_position =
		position == 0 ? GTK_POS_TOP :
		position == 1 ? GTK_POS_BOTTOM :
		position == 2 ? GTK_POS_LEFT :
		GTK_POS_RIGHT;

	// scroll steps
	gint scroll_step;
	scroll_step = prefs_get_integer("ScrollStep", 5);
	gtk_adjustment_set_page_increment(window->vol_adj, scroll_step);

	gint fine_scroll_step;
	fine_scroll_step = prefs_get_integer("FineScrollStep", 1);
	gtk_adjustment_set_step_increment(window->vol_adj, fine_scroll_step);
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
