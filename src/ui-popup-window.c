/* ui-popup-window.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-popup-window.c
 * This file holds the ui-related code for the popup window.
 * @brief Popup window subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#ifndef WITH_GTK3
#include <gdk/gdkkeysyms.h>
#endif

#include "audio.h"
#include "prefs.h"
#include "support-log.h"
#include "ui-support.h"
#include "ui-popup-window.h"

#include "main.h"

#ifdef WITH_GTK3
#define POPUP_WINDOW_HORIZONTAL_UI_FILE "popup-window-horizontal-gtk3.glade"
#define POPUP_WINDOW_VERTICAL_UI_FILE   "popup-window-vertical-gtk3.glade"
#else
#define POPUP_WINDOW_HORIZONTAL_UI_FILE "popup-window-horizontal-gtk2.glade"
#define POPUP_WINDOW_VERTICAL_UI_FILE   "popup-window-vertical-gtk2.glade"
#endif

/* Helpers */

/* Configure the appearance of the text that is shown around the volume slider,
 * according to the current preferences.
 */
static void
configure_vol_text(GtkScale *vol_scale)
{
	gboolean enabled;
	gint position;
	GtkPositionType gtk_position;

	enabled = prefs_get_boolean("DisplayTextVolume", TRUE);
	position = prefs_get_integer("TextVolumePosition", 0);

	gtk_position =
		position == 0 ? GTK_POS_TOP :
		position == 1 ? GTK_POS_BOTTOM :
		position == 2 ? GTK_POS_LEFT :
		GTK_POS_RIGHT;

	if (enabled) {
		gtk_scale_set_draw_value(vol_scale, TRUE);
		gtk_scale_set_value_pos(vol_scale, gtk_position);
	} else {
		gtk_scale_set_draw_value(vol_scale, FALSE);
	}
}

/* Configure the page and step increment of the volume slider,
 * according to the current preferences.
 */
static void
configure_vol_increment(GtkAdjustment *vol_scale_adj)
{
	gdouble scroll_step;
	gdouble fine_scroll_step;

	scroll_step = prefs_get_double("ScrollStep", 5);
	fine_scroll_step =  prefs_get_double("FineScrollStep", 1);

	gtk_adjustment_set_page_increment(vol_scale_adj, scroll_step);
	gtk_adjustment_set_step_increment(vol_scale_adj, fine_scroll_step);
}

/* Update the mute checkbox according to the current audio state. */
static void
update_mute_check(GtkToggleButton *mute_check, GCallback handler_func,
                  gpointer handler_data, gboolean muted)
{
	gint n_handlers_blocked;

	n_handlers_blocked = g_signal_handlers_block_by_func
			     (G_OBJECT(mute_check), handler_func, handler_data);
	g_assert(n_handlers_blocked == 1);

	gtk_toggle_button_set_active(mute_check, muted);

	g_signal_handlers_unblock_by_func
	(G_OBJECT(mute_check), handler_func, handler_data);
}

/* Update the volume slider according to the current audio state. */
static void
update_volume_slider(GtkAdjustment *vol_scale_adj, gdouble volume)
{
	gtk_adjustment_set_value(vol_scale_adj, volume);
}

/* Public functions & signal handlers */

struct popup_window {
	Audio *audio;
	GtkWidget *popup_window;
	GtkWidget *vol_scale;
	GtkAdjustment *vol_scale_adj;
	GtkWidget *mute_check;
};

/**
 * Handles 'button-press-event', 'key-press-event' and 'grab-broken-event' signals,
 * on the GtkWindow. Used to hide the volume popup window.
 *
 * @param widget the object which received the signal.
 * @param event the GdkEvent which triggered this signal.
 * @param window user data set when the signal handler was connected.
 * @return TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
gboolean
on_popup_window_event(G_GNUC_UNUSED GtkWidget *widget, GdkEvent *event,
		      PopupWindow *window)
{
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
			popup_window_hide(window);
		break;
	}

	/* If 'Esc' is pressed, hide popup */
	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_KEY_Escape)
			popup_window_hide(window);
		break;

	/* Broken grab, hide popup */
	case GDK_GRAB_BROKEN:
		popup_window_hide(window);
		break;

	/* Unhandle event, do nothing */
	default:
		break;
	}

	return FALSE;
}

/**
 * Handles the 'change-value' signal on the GtkRange 'vol_scale',
 * changing the voume accordingly.
 *
 * @param range the GtkRange that received the signal.
 * @param scroll the type of scroll action that was performed.
 * @param value the new value resulting from the scroll action.
 * @param window user data set when the signal handler was connected.
 * @return TRUE to prevent other handlers from being invoked for the signal.
 * FALSE to propagate the signal further.
 */
gboolean
on_vol_scale_change_value(GtkRange *range, G_GNUC_UNUSED GtkScrollType scroll,
			  gdouble value, PopupWindow *window)
{
	GtkAdjustment *gtk_adj;

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

	audio_set_volume(window->audio, AUDIO_USER_POPUP, value);

	return FALSE;
}

/**
 * Handles the 'toggled' signal on the GtkToggleButton 'mute_check',
 * changing the mute status accordingly.
 *
 * @param button the GtkToggleButton that received the signal.
 * @param window user data set when the signal handler was connected.
 */
void
on_mute_check_toggled(G_GNUC_UNUSED GtkToggleButton *button, PopupWindow *window)
{
	audio_toggle_mute(window->audio, AUDIO_USER_POPUP);
}

/**
 * Handles the 'clicked' signal on the GtkButton 'mixer_button',
 * therefore opening the mixer application.
 *
 * @param button the GtkButton that received the signal.
 * @param window user data set when the signal handler was connected.
 */
void
on_mixer_button_clicked(G_GNUC_UNUSED GtkButton *button, PopupWindow *window)
{
	popup_window_hide(window);
	do_mixer();
}

static void
on_audio_changed(G_GNUC_UNUSED Audio *audio, AudioEvent *event, gpointer data)
{
	PopupWindow *window = (PopupWindow *) data;

	update_mute_check(GTK_TOGGLE_BUTTON(window->mute_check),
	                  (GCallback) on_mute_check_toggled, window, event->muted);
	update_volume_slider(window->vol_scale_adj, event->volume);
}

/* Public functions */

/**
 * Return a pointer toward the internal GtkWindow instance.
 * Some parts of the code needs to know about it.
 *
 * @param menu a PopupWindow instance.
 */
GtkWindow *
popup_window_get_gtk_window(PopupWindow *window)
{
	return GTK_WINDOW(window->popup_window);
}

/**
 * Shows the popup window, and grab the focus.
 *
 * @param menu a PopupWindow instance.
 */
void
popup_window_show(PopupWindow *window)
{
	GtkWidget *popup_window = window->popup_window;
	GtkWidget *vol_scale = window->vol_scale;

	/* Show the window */
	gtk_widget_show_now(popup_window);

	/* Give focus to volume scale */
	gtk_widget_grab_focus(vol_scale);

	/* Now grab keyboard */
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
		WARN("Could not grab %s\n", gdk_device_get_name(pointer_dev));

	GdkDevice *keyboard_dev = gdk_device_get_associated_device(pointer_dev);

	if (keyboard_dev == NULL)
		return;

	if (gdk_device_grab(keyboard_dev,
			    gtk_widget_get_window(popup_window),
			    GDK_OWNERSHIP_NONE,
			    TRUE,
			    GDK_KEY_PRESS_MASK,
			    NULL, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
		WARN("Could not grab %s\n", gdk_device_get_name(keyboard_dev));
#else
	gdk_pointer_grab(gtk_widget_get_window(popup_window), TRUE,
			 GDK_BUTTON_PRESS_MASK, NULL, NULL, GDK_CURRENT_TIME);
	gdk_keyboard_grab(gtk_widget_get_window(popup_window), TRUE,
			  GDK_CURRENT_TIME);
#endif
}

/**
 * Hides the popup window.
 *
 * @param menu a PopupWindow instance.
 */
void
popup_window_hide(PopupWindow *window)
{
	gtk_widget_hide(window->popup_window);
}

/**
 * Toggle the popup window (aka hide or show).
 *
 * @param menu a PopupWindow instance.
 */
void
popup_window_toggle(PopupWindow *window)
{
	GtkWidget *popup_window = window->popup_window;

	if (gtk_widget_get_visible(popup_window))
		popup_window_hide(window);
	else
		popup_window_show(window);
}

/**
 * Destroys the popup window, freeing any resources.
 *
 * @param menu a PopupWindow instance.
 */
void
popup_window_destroy(PopupWindow *window)
{
	audio_signals_disconnect(window->audio, on_audio_changed, window);
	gtk_widget_destroy(window->popup_window);
	g_free(window);
}

/**
 * Creates the popup window and connects all the signals.
 *
 * @return the newly created PopupWindow instance.
 */
PopupWindow *
popup_window_create(Audio *audio)
{
	gchar *uifile;
	GtkBuilder *builder;
	PopupWindow *window;

	window = g_new0(PopupWindow, 1);

	/* Build UI file depending on slider orientation */
	gchar *orientation;
	orientation = prefs_get_string("SliderOrientation", "vertical");
	if (!g_strcmp0(orientation, "horizontal"))
		uifile = ui_get_builder_file(POPUP_WINDOW_HORIZONTAL_UI_FILE);
	else
		uifile = ui_get_builder_file(POPUP_WINDOW_VERTICAL_UI_FILE);
	g_free(orientation);

	DEBUG("Building popup window from ui file '%s'", uifile);
	builder = gtk_builder_new_from_file(uifile);

	/* Save some widgets for later use */
	assign_gtk_widget(builder, window, popup_window);
	assign_gtk_widget(builder, window, mute_check);
	assign_gtk_widget(builder, window, vol_scale);
	assign_gtk_adjustment(builder, window, vol_scale_adj);

	/* Configure some widgets */
	configure_vol_text(GTK_SCALE(window->vol_scale));
	configure_vol_increment(GTK_ADJUSTMENT(window->vol_scale_adj));

	/* Connect ui signal handlers */
	gtk_builder_connect_signals(builder, window);

	/* Connect audio signal handlers */
	window->audio = audio;
	audio_signals_connect(audio, on_audio_changed, window);

	/* Cleanup */
	g_object_unref(builder);
	g_free(uifile);

	return window;
}
