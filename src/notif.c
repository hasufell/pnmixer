/* notif.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file notif.c
 * This file handles the notification subsystem
 * via libnotify and mostly reacts to volume changes.
 * @brief libnotify subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <libnotify/notify.h>

#include "prefs.h"
#include "support.h"
#include "notif.h"

gboolean enabled;
guint timeout;
gboolean popup;
gboolean tray;
gboolean hotkey;
gboolean external;

struct notif {
	enum notif_source source;
	gint64 time;
};

struct notif *pending_notif;

// TODO: handle compil without libnotify

void
notif_inform(enum notif_source source)
{
	if (pending_notif) {
		WARN("Discarding pending notif (%d, %lld)",
		     pending_notif->source, pending_notif->time);
		g_free(pending_notif);
		pending_notif = NULL;
	}

	pending_notif = g_new0(struct notif, 1);
	pending_notif->source = source;
	pending_notif->time = g_get_real_time();
}

/**
 * Reload notif preferences.
 * This has to be called each time the preferences are modified.
 */
void
notif_reload_prefs(void)
{
	enabled = prefs_get_boolean("EnableNotifications", FALSE);
	timeout = prefs_get_integer("NotificationTimeout", 1500);
	popup = prefs_get_boolean("PopupNotifications", FALSE);
	tray = prefs_get_boolean("MouseNotifications", TRUE);
	hotkey = prefs_get_boolean("HotkeyNotifications", TRUE);
	external = prefs_get_boolean("ExternalNotifications", FALSE);
}

/**
 * Uninitializes libnotify. This should be called only once at cleanup.
 */
void
notif_cleanup(void)
{
	g_assert(notify_is_initted() == TRUE);

	notify_uninit();
}

/**
 * Initializes libnotify. This should be called only once at startup.
 */
void
notif_init(void)
{
	g_assert(notify_is_initted() == FALSE);

	if (!notify_init(PACKAGE))
		report_error("Unable to initialize libnotify. "
		             "Notifications will not be sent.");

	notif_reload_prefs();
}
