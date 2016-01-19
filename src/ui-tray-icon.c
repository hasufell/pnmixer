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

struct tray_icon_prefs {
	gboolean mouse_noti;
	gint middle_click_action;
	gint scroll_step;
};

typedef struct tray_icon_prefs TrayIconPrefs;

struct vol_meter {
	/* Configuration */
	guchar red;
	guchar green;
	guchar blue;
	gint x_offset;
	gint y_offset;
	/* Dynamic stuff */
	GdkPixbuf *pixbuf;
	gint width;
	guchar *row;
};

typedef struct vol_meter VolMeter;

struct tray_icon {
	TrayIconPrefs *prefs;
	VolMeter *vol_meter;
	GtkStatusIcon *status_icon;
	gint status_icon_size;
	GdkPixbuf **pixbufs;
};

/* Tray icon preferences */

static void
tray_icon_prefs_free(TrayIconPrefs *prefs)
{
	g_free(prefs);
}

static TrayIconPrefs *
tray_icon_prefs_new(void)
{
	TrayIconPrefs *prefs;

	prefs = g_new0(TrayIconPrefs, 1);

	prefs->mouse_noti = prefs_get_boolean("MouseNotifications", TRUE);
	prefs->middle_click_action = prefs_get_integer("MiddleClickAction", 0);
	prefs->scroll_step = prefs_get_integer("ScrollStep", 5);

	return prefs;
}

/*
 * Tray icon pixbuf array.
 * The array contains all the icons currently in use, stored as GdkPixbuf.
 * The array is dynamic. It's built "just-in-time", that's to say the first
 * time it's accessed. It avoids building it multiple times at startup.
 * Also, when we need to reload the array, we just have to free it.
 */

static void
pixbuf_array_free(GdkPixbuf **pixbufs)
{
	gsize i;

	if (!pixbufs)
		return;

	for (i = 0; i < N_VOLUME_ICONS; i++)
		g_object_unref(pixbufs[i]);

	g_free(pixbufs);
}

static GdkPixbuf **
pixbuf_array_new(int size)
{
	GdkPixbuf *pixbufs[N_VOLUME_ICONS];
	gboolean system_theme;

	DEBUG_PRINT("Building pixbuf array for size %d)", size);

	system_theme = prefs_get_boolean("SystemTheme", FALSE);

	if (system_theme) {
		pixbufs[VOLUME_MUTED] = get_stock_pixbuf("audio-volume-muted", size);
		pixbufs[VOLUME_OFF] = get_stock_pixbuf("audio-volume-off", size);
		pixbufs[VOLUME_LOW] = get_stock_pixbuf("audio-volume-low", size);
		pixbufs[VOLUME_MEDIUM] = get_stock_pixbuf("audio-volume-medium", size);
		pixbufs[VOLUME_HIGH] = get_stock_pixbuf("audio-volume-high", size);
		/* 'audio-volume-off' is not available in every icon set.
		 * Check freedesktop standard for more info:
		 *   http://standards.freedesktop.org/icon-naming-spec/
		 *   icon-naming-spec-latest.html
		 */
		if (pixbufs[VOLUME_OFF] == NULL)
			pixbufs[VOLUME_OFF] = get_stock_pixbuf("audio-volume-low", size);
	} else {
		pixbufs[VOLUME_MUTED] = create_pixbuf("pnmixer-muted.png");
		pixbufs[VOLUME_OFF] = create_pixbuf("pnmixer-off.png");
		pixbufs[VOLUME_LOW] = create_pixbuf("pnmixer-low.png");
		pixbufs[VOLUME_MEDIUM] = create_pixbuf("pnmixer-medium.png");
		pixbufs[VOLUME_HIGH] = create_pixbuf("pnmixer-high.png");
	}

	return g_memdup(pixbufs, sizeof pixbufs);
}

/* Tray icon volume meter */

static void
vol_meter_free(VolMeter *vol_meter)
{
	if (!vol_meter)
		return;

	if (vol_meter->pixbuf)
		g_object_unref(vol_meter->pixbuf);

	g_free(vol_meter->row);
	g_free(vol_meter);
}

static VolMeter*
vol_meter_new(void)
{
	VolMeter *vol_meter;
	gboolean vol_meter_enabled;
	gdouble *vol_meter_clrs;

	vol_meter_enabled = prefs_get_boolean("DrawVolMeter", FALSE);
	if (vol_meter_enabled == FALSE)
		return NULL;

	vol_meter = g_new0(VolMeter, 1);

	vol_meter->x_offset = prefs_get_integer("VolMeterPos", 0);
	vol_meter->y_offset = 5;

	vol_meter_clrs = prefs_get_double_list("VolMeterColor", NULL);
	vol_meter->red = vol_meter_clrs[0] * 255;
	vol_meter->green = vol_meter_clrs[1] * 255;
	vol_meter->blue = vol_meter_clrs[2] * 255;
	g_free(vol_meter_clrs);

	return vol_meter;
}

/**
 * Draws the volume meter on top of the icon.
 *
 * @param pixbuf the GdkPixbuf icon to draw the volume meter on
 * @param x offset
 * @param y offset
 * @param h height of the volume meter
 */
static GdkPixbuf *
vol_meter_draw(VolMeter *vol_meter, GdkPixbuf *pixbuf, int volume)
{
	int icon_width, icon_height;
	int vol_meter_width;
	int x, y, h;
	gdouble div_factor;
	int rowstride, i;
	guchar *pixels, *p;

	// TODO: make this feature cool
	// TODO: remove the comment about "more CPU usage"

	/* Ensure the pixbuf is as expected */
	g_assert(gdk_pixbuf_get_colorspace(pixbuf) == GDK_COLORSPACE_RGB);
	g_assert(gdk_pixbuf_get_bits_per_sample(pixbuf) == 8);
	g_assert(gdk_pixbuf_get_has_alpha(pixbuf));
	g_assert(gdk_pixbuf_get_n_channels(pixbuf) == 4);

	icon_width = gdk_pixbuf_get_width(pixbuf);
	icon_height = gdk_pixbuf_get_height(pixbuf);
	printf("icon_width: %d, icon_height: %d\n", icon_width, icon_height);

	/* Cache the pixbuf passed in parameter */
	if (vol_meter->pixbuf)
		g_object_unref(vol_meter->pixbuf);
	vol_meter->pixbuf = pixbuf = gdk_pixbuf_copy(pixbuf);

	/* Let's check if the icon width changed, in which case we
	 * must reinit our internal row of pixels.
	 */
	vol_meter_width = icon_width / 8;
	if (vol_meter_width != vol_meter->width) {
		vol_meter->width = vol_meter_width;
		g_free(vol_meter->row);
		vol_meter->row = NULL;
	}

	if (vol_meter->row == NULL) {
		DEBUG_PRINT("Allocating vol meter row (%d)", vol_meter->width);
		vol_meter->row = g_malloc(vol_meter->width * sizeof(guchar) * 4);
		for (i = 0; i < vol_meter->width; i++) {
			vol_meter->row[i * 4 + 0] = vol_meter->red;
			vol_meter->row[i * 4 + 1] = vol_meter->green;
			vol_meter->row[i * 4 + 2] = vol_meter->blue;
			vol_meter->row[i * 4 + 3] = 255;
		}
	}

	/* Get the volume meter coordinates and height */
	x = vol_meter->x_offset;
	y = vol_meter->y_offset;
	printf("x: %d, y: %d\n", x, y);

	div_factor = (icon_height - (y * 2)) / 100.0;
	h = volume * div_factor;
	printf("div_factor: %lf, h: %d\n", div_factor, h);

	g_assert(x >= 0 && x < icon_width);
	g_assert(y >= 0 && y + h < icon_height);

	/* Draw the volume meter.
	 * Rows in the image are stored top to bottom.
	 */
	y = icon_height - y;
	rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	pixels = gdk_pixbuf_get_pixels(pixbuf);

	for (i = 0; i < h; i++) {
		p = pixels + (y - i) * rowstride + x * 4;
		memcpy(p, vol_meter->row, vol_meter->width * 4);
	}

	return pixbuf;
}

/* Helpers */

static void
update_icon(TrayIcon *icon, int volume, int muted)
{
	GdkPixbuf **pixbufs;
	GdkPixbuf *pixbuf;

	if (icon->pixbufs == NULL)
		icon->pixbufs = pixbuf_array_new(icon->status_icon_size);

	pixbufs = icon->pixbufs;

	if (muted == 1) {
		if (volume == 0)
			pixbuf = pixbufs[VOLUME_OFF];
		else if (volume < 33)
			pixbuf = pixbufs[VOLUME_LOW];
		else if (volume < 66)
			pixbuf = pixbufs[VOLUME_MEDIUM];
		else
			pixbuf = pixbufs[VOLUME_HIGH];
	} else {
		pixbuf = pixbufs[VOLUME_MUTED];
	}

	if (muted == 1 && icon->vol_meter)
		pixbuf = vol_meter_draw(icon->vol_meter, pixbuf, volume);

	gtk_status_icon_set_from_pixbuf(icon->status_icon, pixbuf);
}

static void
update_tooltip(TrayIcon *icon, int volume, int muted)
{
	const char *card_name;
	const char *channel;
	char tooltip[64];

	card_name = (alsa_get_active_card())->name;
	channel = alsa_get_active_channel();
	
	if (muted == 1)
		snprintf(tooltip, sizeof tooltip, _("%s (%s)\n%s: %d %%"),
		         card_name, channel, _("Volume"), volume);
	else
		snprintf(tooltip, sizeof tooltip, _("%s (%s)\n%s: %d %%\n%s"),
		         card_name, channel, _("Volume"), volume, _("Muted"));

	gtk_status_icon_set_tooltip_text(icon->status_icon, tooltip);
}

/* Signal handlers */

/**
 * Handles the 'activate' signal on the tray_icon,
 * usually opening the popup_window and grabbing pointer and keyboard.
 *
 * @param status_icon the object which received the signal
 * @param user_data user data set when the signal handler was connected
 */
static void
on_activate(G_GNUC_UNUSED GtkStatusIcon *status_icon,
            G_GNUC_UNUSED TrayIcon *icon)
{
	do_toggle_popup_window();
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
              guint activate_time, G_GNUC_UNUSED TrayIcon *icon)
{
	do_show_popup_menu(gtk_status_icon_position_menu, status_icon, button, activate_time);
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
on_button_release_event(G_GNUC_UNUSED GtkStatusIcon *status_icon,
                        GdkEventButton *event, TrayIcon *icon)
{
	if (event->button != 2)
		return;

	switch (icon->prefs->middle_click_action) {
	case 0:
		do_mute(icon->prefs->mouse_noti);
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
                TrayIcon *icon)
{
	int volume;
	gint scroll_step;
	gboolean mouse_noti;

	volume = getvol();
	scroll_step = icon->prefs->scroll_step;
	mouse_noti = icon->prefs->mouse_noti;

	if (event->direction == GDK_SCROLL_UP)
		setvol(volume + scroll_step, 1, mouse_noti);
        else if (event->direction == GDK_SCROLL_DOWN)
		setvol(volume - scroll_step, -1, mouse_noti);

	if (ismuted() == 0)
		setmute(mouse_noti);

	do_update_ui();

	return TRUE;
}

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
on_size_changed(G_GNUC_UNUSED GtkStatusIcon *status_icon, gint size,
                TrayIcon *icon)
{
	DEBUG_PRINT("Tray icon size is now %d", size);

	icon->status_icon_size = size;

	pixbuf_array_free(icon->pixbufs);
	icon->pixbufs = NULL;

	if (icon->vol_meter) {
		vol_meter_free(icon->vol_meter);
		icon->vol_meter = vol_meter_new();
	}

	tray_icon_update(icon);

	return FALSE;
}


/* Public functions */

/**
 * Returns the size of the tray icon.
 *
 * @return size of the tray icon or 48 if there is none
 */
gint
tray_icon_get_size(TrayIcon *icon)
{
	// TODO: remove that when no more useful

	GtkStatusIcon *status_icon = icon->status_icon;
	gint size;

	// gtk_status_icon_is_embedded returns false until the prefs
	// window is opened on gtk3

	printf("gtk_status_icon_get_size(status_icon) = %d\n",
	       gtk_status_icon_get_size(status_icon));

	if (status_icon && GTK_IS_STATUS_ICON(status_icon))
		size = gtk_status_icon_get_size(status_icon);
	else
		size = 48;

	return size;
}

/**
 * Updates the tray icon. Usually called after volume has been muted
 * or changed.
 */
void
tray_icon_update(TrayIcon *icon)
{
	int volume = getvol();
	int muted = ismuted();

	update_icon(icon, volume, muted);
	update_tooltip(icon, volume, muted);
}

void
tray_icon_reload_prefs(TrayIcon *icon)
{
	/* Reload some configuration */
	tray_icon_prefs_free(icon->prefs);
	icon->prefs = tray_icon_prefs_new();

	/* Recreate the volume meter */
	vol_meter_free(icon->vol_meter);
	icon->vol_meter = vol_meter_new();

	/* Destroy the pixbufs array */
	pixbuf_array_free(icon->pixbufs);
	icon->pixbufs = NULL;

	/* Update */
	tray_icon_update(icon);
}

void
tray_icon_destroy(TrayIcon *icon)
{
	g_object_unref(icon->status_icon);
	pixbuf_array_free(icon->pixbufs);
	vol_meter_free(icon->vol_meter);
	tray_icon_prefs_free(icon->prefs);
	g_free(icon);
}

/**
 * Creates the tray icon and connects the signals 'scroll_event'
 * and 'size-changed'.
 *
 * @return the newly created tray icon
 */
TrayIcon *
tray_icon_create(void)
{
	TrayIcon *icon;

	DEBUG_PRINT("Creating tray icon");

	icon = g_new0(TrayIcon, 1);

	icon->prefs = tray_icon_prefs_new();

	icon->vol_meter = vol_meter_new();

	icon->status_icon = gtk_status_icon_new();
	/* Handle the left click */
	g_signal_connect(icon->status_icon, "activate",
			 G_CALLBACK(on_activate), icon);
	/* Handle the right click */
	g_signal_connect(icon->status_icon, "popup-menu",
			 G_CALLBACK(on_popup_menu), icon);
	/* Handle the middle click */
	g_signal_connect(icon->status_icon, "button-release-event",
			 G_CALLBACK(on_button_release_event), icon);
	/* Handle mouse scrolling on the icon */
	g_signal_connect(icon->status_icon, "scroll_event",
			 G_CALLBACK(on_scroll_event), icon);
	/* Handle change of size */
	g_signal_connect(icon->status_icon, "size-changed",
			 G_CALLBACK(on_size_changed), icon);

	gtk_status_icon_set_visible(icon->status_icon, TRUE);

	return icon;
}
