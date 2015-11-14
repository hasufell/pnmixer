/* notify.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file notify.c
 * This file handles the notification subsystem
 * via libnotify and mostly reacts to volume changes.
 * @brief libnotify subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "alsa.h"
#include "main.h"
#include "notify.h"
#include "prefs.h"
#include "support.h"


/**
 * Send a volume notification. This is mainly called
 * from the alsa subsystem whenever we have volume
 * changes.
 *
 * @param level the playback volume level
 * @param muted whether the playback is muted
 */
void
do_notify_volume(gint level, gboolean muted)
{
	GNotification *notification;
	gchar *title, *body, *icon_file, *active_card_name;
	const char *active_channel;
	GIcon *icon;

	active_card_name = (alsa_get_active_card())->name;
	active_channel = alsa_get_active_channel();

	title = g_strdup_printf("%s (%s)", active_card_name, active_channel);

	notification = g_notification_new(title);

	if (level < 0)
		level = 0;
	if (level > 100)
		level = 100;

	if (muted)
		body = g_strdup("Volume muted");
	else
		body = g_strdup_printf("Volume: %d%%\n", level);

	g_notification_set_body(notification, body);

	if (muted)
		icon_file = "audio-volume-muted";
	else if (level == 0)
		icon_file = "audio-volume-off";
	else if (level < 33)
		icon_file = "audio-volume-low";
	else if (level < 66)
		icon_file = "audio-volume-medium";
	else
		icon_file = "audio-volume-high";

	icon = g_themed_icon_new(icon_file);
	g_notification_set_icon(notification, icon);

	g_application_send_notification(G_APPLICATION(gtkapp),
			"volume-change", notification);

	g_free(title);
	g_free(body);
	g_object_unref(icon);
}

/**
 * Send a text notification.
 *
 * @param title the notification title
 * @param body the notification body
 */
void
do_notify_text(const gchar *title, const gchar *body)
{
	GNotification *notification;

	notification = g_notification_new(title);
	g_notification_set_body(notification, body);

	g_application_send_notification(G_APPLICATION(gtkapp),
			"text-notify", notification);
}
