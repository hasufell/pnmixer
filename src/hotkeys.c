/* hotkeys.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file hotkeys.c
 * This file handles the hotkey subsystem, including
 * communcation with Xlib and intercepting key presses
 * before they can be interpreted by gdk/gtk.
 * @brief hotkey subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gdk/gdkx.h>
#include <X11/XKBlib.h>

#include "audio.h"
#include "main.h"
#include "hotkey.h"
#include "support.h"
#include "prefs.h"

static Hotkey *mute_hotkey;
static Hotkey *up_hotkey;
static Hotkey *down_hotkey;

/* Helpers */

/**
 * This function is called before gdk/gtk can respond
 * to any(!) window event and handles pressed hotkeys.
 *
 * @param gdk_xevent the native event to filter
 * @param event the GDK event to which the X event will be translated
 * @param data user data set when the filter was installed
 * @return a GdkFilterReturn value, should be GDK_FILTER_CONTINUE only
 */
static GdkFilterReturn
key_filter(GdkXEvent *gdk_xevent,
	   G_GNUC_UNUSED GdkEvent *event,
	   G_GNUC_UNUSED gpointer data)
{
	gint type;
	guint key, state;
	XKeyEvent *xevent;

	xevent = (XKeyEvent *) gdk_xevent;
	type = xevent->type;
	key = xevent->keycode;
	state = xevent->state;

	if (type == KeyPress) {
		if (hotkey_matches(mute_hotkey, key, state))
			audio_mute(hotkey_noti);
		else if (hotkey_matches(up_hotkey, key, state))
			audio_raise_volume(hotkey_noti);
		else if (hotkey_matches(down_hotkey, key, state))
			audio_lower_volume(hotkey_noti);
		// just ignore unknown hotkeys
	}

	return GDK_FILTER_CONTINUE;
}

/* Removes the previously attached key_filter() function from
 * the root window.
 */
static void
hotkeys_remove_filter(void)
{
	GdkWindow *window;

	window = gdk_x11_window_foreign_new_for_display(
		gdk_display_get_default(), GDK_ROOT_WINDOW());

	gdk_window_remove_filter(window, key_filter, NULL);
}

/* Ataches the key_filter() function as a filter
 * to the root window, so it will intercept window events.
 */
static void
hotkeys_add_filter(void)
{
	GdkWindow *window;

	window = gdk_x11_window_foreign_new_for_display(
		gdk_display_get_default(), GDK_ROOT_WINDOW());

	gdk_window_add_filter(window, key_filter, NULL);
}

/* Public functions */

/**
 * Reload hotkey preferences.
 * This has to be called each time the preferences are modified.
 */
void
hotkeys_reload_prefs(void)
{
	gboolean enabled;
	gint key, mods;
	gboolean mute_err, up_err, down_err;

	/* Free any hotkey that may be currently assigned */
	hotkey_free(mute_hotkey);
	mute_hotkey = NULL;

	hotkey_free(up_hotkey);
	up_hotkey = NULL;

	hotkey_free(down_hotkey);
	down_hotkey = NULL;

	/* Return if hotkeys are disabled */
	enabled = prefs_get_boolean("EnableHotKeys", FALSE);
	if (enabled == FALSE)
		return;

	/* Setup mute hotkey */
	mute_err = FALSE;
	key = prefs_get_integer("VolMuteKey", -1);
	mods = prefs_get_integer("VolMuteMods", 0);
	if (key != -1) {
		mute_hotkey = hotkey_new(key, mods);
		if (mute_hotkey == NULL)
			mute_err = TRUE;
	}

	/* Setup volume up hotkey */
	up_err = FALSE;
	key = prefs_get_integer("VolUpKey", -1);
	mods = prefs_get_integer("VolUpMods", 0);
	if (key != -1) {
		up_hotkey = hotkey_new(key, mods);
		if (up_hotkey == NULL)
			up_err = TRUE;
	}

	/* Setup volume down hotkey */
	down_err = FALSE;
	key = prefs_get_integer("VolDownKey", -1);
	mods = prefs_get_integer("VolDownMods", 0);
	if (key != -1) {
		down_hotkey = hotkey_new(key, mods);
		if (down_hotkey == NULL)
			down_err = TRUE;
	}

	/* Display error message if needed */
	if (mute_err || up_err || down_err) {
		// TODO: check if idle report is needed
		report_error("%s:\n%s%s%s%s%s%s",
		             _("Could not bind the following hotkeys"),
		             mute_err ? _("Mute/Unmute") : "",
		             mute_err ? "\n" : "",
		             up_err ? _("Volume Up") : "",
		             up_err ? "\n" : "",
		             down_err ? _("Volume Down") : "",
		             down_err ? "\n" : ""
			);
	}
}

/**
 * Cleanup the hotkey subsystem. This should be called only once on exit.
 */
void
hotkeys_cleanup(void)
{
	hotkeys_remove_filter();
	hotkey_free(mute_hotkey);
	hotkey_free(up_hotkey);
	hotkey_free(down_hotkey);
}

/**
 * Initialize the hotkey subsystem. This should be called only once on startup.
 */
void
hotkeys_init(void)
{
	hotkeys_reload_prefs();
	hotkeys_add_filter();
}

