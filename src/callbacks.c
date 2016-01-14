/* callbacks.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file callbacks.c
 * This file holds callback functions for various signals
 * received by different GtkWidgets, some of them defined
 * in the gtk-builder glade files.
 * @brief gtk callbacks
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <alsa/asoundlib.h>
#include "alsa.h"
#include "callbacks.h"
#include "main.h"
#include "support.h"
#include "prefs.h"

int volume;
extern int volume;

/**
 * Callback function when the mute_check_popup_window (GtkCheckButton) in the
 * volume popup window received the pressed signal or
 * when the mute_check_popup_menu (GtkCheckMenuItem) in the right-click
 * menu received the activate signal.
 *
 * @param button the object that received the signal
 * @param event the GdkEvent which triggered this signal
 * @param user_data user data set when the signal handler was connected
 */
gboolean
on_mute_clicked(G_GNUC_UNUSED GtkButton *button,
		G_GNUC_UNUSED GdkEvent *event,
		G_GNUC_UNUSED gpointer user_data)
{

	setmute(popup_noti);
	on_volume_has_changed();
	return TRUE;
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
vol_scroll_event(GtkRange *range,
		G_GNUC_UNUSED GtkScrollType scroll,
		gdouble value,
		G_GNUC_UNUSED gpointer user_data)
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
	if (value < gtk_adjustment_get_lower(gtk_adj)) {
		value = gtk_adjustment_get_lower(gtk_adj);
	}
	if (value > gtk_adjustment_get_upper(gtk_adj)) {
		value = gtk_adjustment_get_upper(gtk_adj);
	}

	volumeset = (int) value;

	setvol(volumeset, 0, popup_noti);
	if (ismuted() == 0)
		setmute(popup_noti);

	on_volume_has_changed();

	return FALSE;
}

/**
 * Callback function when the tray_icon receives the scroll-event
 * signal.
 *
 * @param status_icon the object which received the signal
 * @param event the GdkEventScroll which triggered this signal
 * @param user_data user data set when the signal handler was connected
 * @return TRUE to stop other handlers from being invoked for the event.
 * False to propagate the event further
 */
gboolean
on_scroll(G_GNUC_UNUSED GtkStatusIcon *status_icon,
		GdkEventScroll *event,
		G_GNUC_UNUSED gpointer user_data)
{
	int cv = getvol();
	if (event->direction == GDK_SCROLL_UP) {
		setvol(cv + scroll_step, 1, mouse_noti);
	} else if (event->direction == GDK_SCROLL_DOWN) {
		setvol(cv - scroll_step, -1, mouse_noti);
	}

	if (ismuted() == 0)
		setmute(mouse_noti);

	// this will set the slider value
	get_current_levels();

	on_volume_has_changed();

	return TRUE;
}
