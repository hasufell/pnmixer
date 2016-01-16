/* ui-tray-icon.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-tray-icon.c
 * This file holds the ui-related code for the system tray icon.
 * @brief tray icon ui subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "ui-tray-icon.h"
#include "support.h"
#include "prefs.h"
#include "alsa.h"
#include "debug.h"

#include "main.h"

enum {
	VOLUME_MUTED,
	VOLUME_OFF,
	VOLUME_LOW,
	VOLUME_MEDIUM,
	VOLUME_HIGH,
	N_VOLUME_ICONS
};

static GdkPixbuf *icons[N_VOLUME_ICONS] = { NULL };

static GtkStatusIcon *tray_icon;

/* Preferences */
static gboolean mouse_noti;
static gint middle_click_action;
static gboolean icon_system_theme;
static gboolean draw_volume_meter;
static int draw_offset;
static guchar vol_meter_red, vol_meter_green, vol_meter_blue;

/* Value to draw volume meter, depending on icon size and conf */
static float vol_meter_div_factor;
static int vol_meter_width;
static guchar *vol_meter_row;

/* Helpers */

static void
populate_icon_array(void)
{
	guint i;
	int size;

	for (i = 0; i < N_VOLUME_ICONS; i++)
		g_clear_object(&icons[i]);

	size = tray_icon_get_size();

	if (icon_system_theme) {
		icons[VOLUME_MUTED] = get_stock_pixbuf("audio-volume-muted", size);
		icons[VOLUME_OFF] = get_stock_pixbuf("audio-volume-off", size);
		icons[VOLUME_LOW] = get_stock_pixbuf("audio-volume-low", size);
		icons[VOLUME_MEDIUM] = get_stock_pixbuf("audio-volume-medium", size);
		icons[VOLUME_HIGH] = get_stock_pixbuf("audio-volume-high", size);
		/* 'audio-volume-off' is not available in every icon set.
		 * Check freedesktop standard for more info:
		 *   http://standards.freedesktop.org/icon-naming-spec/
		 *   icon-naming-spec-latest.html
		 */
		if (icons[VOLUME_OFF] == NULL)
			icons[VOLUME_OFF] = get_stock_pixbuf("audio-volume-low", size);
	} else {
		icons[VOLUME_MUTED] = create_pixbuf("pnmixer-muted.png");
		icons[VOLUME_OFF] = create_pixbuf("pnmixer-off.png");
		icons[VOLUME_LOW] = create_pixbuf("pnmixer-low.png");
		icons[VOLUME_MEDIUM] = create_pixbuf("pnmixer-medium.png");
		icons[VOLUME_HIGH] = create_pixbuf("pnmixer-high.png");
	}
}

static void
populate_volume_meter(void)
{
	int i, icon_width;

	g_free(vol_meter_row);
	vol_meter_row = NULL;

	if (!draw_volume_meter)
		return;

	icon_width = gdk_pixbuf_get_height(icons[0]);
	vol_meter_div_factor = ((gdk_pixbuf_get_height(icons[0]) - 10) / 100.0);
	vol_meter_width = 1.25 * icon_width;
	if (vol_meter_width % 4 != 0)
		vol_meter_width -= (vol_meter_width % 4);

	vol_meter_row = g_malloc(vol_meter_width * sizeof(guchar));
	for (i = 0; i < vol_meter_width / 4; i++) {
		vol_meter_row[i * 4 + 0] = vol_meter_red;
		vol_meter_row[i * 4 + 1] = vol_meter_green;
		vol_meter_row[i * 4 + 2] = vol_meter_blue;
		vol_meter_row[i * 4 + 3] = 255;
	}
}

/**
 * Draws the volume meter on top of the icon.
 *
 * @param pixbuf the GdkPixbuf icon to draw the volume meter on
 * @param x offset
 * @param y offset
 * @param h height of the volume meter
 */
static void
draw_vol_meter(GdkPixbuf *pixbuf, int x, int y, int h)
{
	int width, height, rowstride, n_channels, i;
	guchar *pixels, *p;

	n_channels = gdk_pixbuf_get_n_channels(pixbuf);

	g_assert(gdk_pixbuf_get_colorspace(pixbuf) == GDK_COLORSPACE_RGB);
	g_assert(gdk_pixbuf_get_bits_per_sample(pixbuf) == 8);
	g_assert(gdk_pixbuf_get_has_alpha(pixbuf));
	g_assert(n_channels == 4);

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);

	g_assert(x >= 0 && x < width);
	g_assert(y + h >= 0 && y < height);

	rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	pixels = gdk_pixbuf_get_pixels(pixbuf);

	y = height - y;
	for (i = 0; i < h; i++) {
		p = pixels + (y - i) * rowstride + x * n_channels;
		memcpy(p, vol_meter_row, vol_meter_width);
	}
}

static void
update_icon(int volume, int muted)
{
	static GdkPixbuf *vol_meter_icon;
	GdkPixbuf *icon;

	g_clear_object(&vol_meter_icon);

	if (muted == 1) {
		if (volume == 0)
			icon = icons[VOLUME_OFF];
		else if (volume < 33)
			icon = icons[VOLUME_LOW];
		else if (volume < 66)
			icon = icons[VOLUME_MEDIUM];
		else
			icon = icons[VOLUME_HIGH];

		if (vol_meter_row) {
			vol_meter_icon = gdk_pixbuf_copy(icon);
			draw_vol_meter(vol_meter_icon, draw_offset, 5,
			               volume * vol_meter_div_factor);
			icon = vol_meter_icon;
		}
	} else {
		icon = icons[VOLUME_MUTED];
	}

	gtk_status_icon_set_from_pixbuf(tray_icon, icon);
}

static void
update_tooltip(int volume, int muted)
{
	const char *card_name;
	const char *channel;
	char tooltip[64];

	card_name = (alsa_get_active_card())->name;
	channel = alsa_get_active_channel();

	if (muted == 1)
		snprintf(tooltip, sizeof tooltip, _("%s (%s)\nVolume: %d %%"),
		         card_name, channel, volume);
	else
		snprintf(tooltip, sizeof tooltip, _("%s (%s)\nVolume: %d %%\nMuted"),
		         card_name, channel, volume);

	gtk_status_icon_set_tooltip_text(tray_icon, tooltip);
}








/* Signal handlers */

/**
 * Handles the 'size-changed' signal on the tray_icon by
 * calling update_status_icons().
 *
 * @param status_icon the object which received the signal
 * @param size the new size
 * @param user_data set when the signal handler was connected
 * @return FALSE, so Gtk+ scales the icon as necessary
 */
static gboolean
on_size_changed(G_GNUC_UNUSED GtkStatusIcon *status_icon, G_GNUC_UNUSED gint size,
                G_GNUC_UNUSED gpointer user_data)
{
	populate_icon_array();
	populate_volume_meter();
	tray_icon_update();
	return TRUE;
}

/**
 * Callback function when the tray_icon receives the scroll-event signal.
 *
 * @param status_icon the object which received the signal
 * @param event the GdkEventScroll which triggered this signal
 * @param user_data user data set when the signal handler was connected
 * @return TRUE to stop other handlers from being invoked for the event.
 * False to propagate the event further
 */
static gboolean
on_scroll_event(G_GNUC_UNUSED GtkStatusIcon *status_icon, GdkEventScroll *event,
                G_GNUC_UNUSED gpointer user_data)
{
	int cv;

	cv = getvol();

	if (event->direction == GDK_SCROLL_UP)
		setvol(cv + scroll_step, 1, mouse_noti);
        else if (event->direction == GDK_SCROLL_DOWN)
		setvol(cv - scroll_step, -1, mouse_noti);

	if (ismuted() == 0)
		setmute(mouse_noti);

	// this will set the slider value
	get_current_levels();

	on_volume_has_changed();

	return TRUE;
}

/**
 * Handles the 'popup-menu' signal on the tray_icon, which brings
 * up the context menu, usually activated by right-click.
 *
 * @param status_icon the object which received the signal
 * @param button the button that was pressed, or 0 if the signal
 * is not emitted in response to a button press event
 * @param activate_time the timestamp of the event that triggered
 * the signal emission
 * @param menu user data set when the signal handler was connected
 */
static void
on_popup_menu(GtkStatusIcon *status_icon, guint button,
              guint activate_time, GtkMenu *menu)
{
	do_show_popup_menu(tray_icon, button, activate_time);
}

/**
 * Handles the 'activate' signal on the tray_icon,
 * usually opening the popup_window and grabbing pointer and keyboard.
 *
 * @param status_icon the object which received the signal
 * @param user_data user data set when the signal handler was connected
 */
static void
on_activate(G_GNUC_UNUSED GtkStatusIcon *status_icon,
            G_GNUC_UNUSED gpointer user_data)
{
	do_toggle_popup_window();
}

/* FIXME: return type should be gboolean */
/**
 * Handles button-release-event' signal on the tray_icon, currently
 * only used for middle-click.
 *
 * @param status_icon the object which received the signal
 * @param event the GdkEventButton which triggered this signal
 * @param user_data user data set when the signal handler was
 * connected
 */
static void
on_button_release_event(G_GNUC_UNUSED GtkStatusIcon *status_icon, GdkEventButton *event,
                        G_GNUC_UNUSED gpointer user_data)
{
	if (event->button != 2)
		return;

	switch (middle_click_action) {
	case 0:
		do_mute(mouse_noti);
		break;
	case 1:
		do_open_prefs();
		break;
	case 2:
		do_mixer();
		break;
	case 3:
		do_custom_command();
		break;
	default: {
	}	// nothing
	}
}

/* Public functions */

/**
 * Returns the size of the tray icon.
 *
 * @return size of the tray icon or 48 if there is none
 */
gint
tray_icon_get_size(void)
{
	gint size;

	// gtk_status_icon_is_embedded returns false until the prefs
	// window is opened on gtk3

	if (tray_icon && GTK_IS_STATUS_ICON(tray_icon))
		size = gtk_status_icon_get_size(tray_icon);
	else
		size = 48;

	return size;
}

/**
 * Updates the tray icon. Usually called after volume has been muted
 * or changed.
 */
void
tray_icon_update(void)
{
	int volume = getvol();
	int muted = ismuted();

	update_icon(volume, muted);
	update_tooltip(volume, muted);
}

void
tray_icon_reload_prefs(void)
{
	icon_system_theme = prefs_get_boolean("SystemTheme", FALSE);
	middle_click_action = prefs_get_integer("MiddleClickAction", 0);
	draw_volume_meter = prefs_get_boolean("DrawVolMeter", FALSE);
	draw_offset = prefs_get_integer("VolMeterPos", 0);

	mouse_noti = prefs_get_boolean("MouseNotifications", TRUE);

	gdouble *vol_meter_clrs;
	vol_meter_clrs = prefs_get_vol_meter_colors();
	vol_meter_red = vol_meter_clrs[0] * 255;
	vol_meter_green = vol_meter_clrs[1] * 255;
	vol_meter_blue = vol_meter_clrs[2] * 255;
	g_free(vol_meter_clrs);

	populate_icon_array();
	populate_volume_meter();

	tray_icon_update();
}

void
tray_icon_destroy(void)
{
	g_clear_object(&tray_icon);
}

/**
 * Creates the tray icon and connects the signals 'scroll_event'
 * and 'size-changed'.
 *
 * @return the newly created tray icon
 */
void
tray_icon_create(void)
{
	DEBUG_PRINT("Creating tray icon");

	tray_icon = gtk_status_icon_new();

	// Handles the left click
	g_signal_connect(tray_icon, "activate",
			 G_CALLBACK(on_activate), NULL);
	// Handles the right click
	g_signal_connect(tray_icon, "popup-menu",
			 G_CALLBACK(on_popup_menu), NULL);
	// Handles the middle click
	g_signal_connect(tray_icon, "button-release-event",
			 G_CALLBACK(on_button_release_event), NULL);
	// Handles mouse scrolling on the icon
	g_signal_connect(tray_icon, "scroll_event",
			 G_CALLBACK(on_scroll_event), NULL);
	// Handles change of size
	g_signal_connect(tray_icon, "size-changed",
			 G_CALLBACK(on_size_changed), NULL);

	gtk_status_icon_set_visible(tray_icon, TRUE);
}
