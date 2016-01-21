/* main.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file main.c
 * The main program entry point. Also handles creating and opening
 * the widgets, connecting signals, updating the tray icon and
 * error handling.
 * @brief gtk+ initialization
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <locale.h>

#include <glib.h>

#include "audio.h"
#include "main.h"
#include "notify.h"
#include "support.h"
#include "hotkeys.h"
#include "prefs.h"
#include "ui-prefs-window.h"
#include "ui-popup-menu.h"
#include "ui-popup-window.h"
#include "ui-tray-icon.h"

GtkWindow *main_window = NULL;
int scroll_step;

static PopupMenu *popup_menu;
static PopupWindow *popup_window;
static TrayIcon *tray_icon;


/**
 * Applies the preferences, usually triggered by on_ok_button_clicked()
 * in callbacks.c, but also initially called from main().
 *
 * @param alsa_change whether we want to trigger alsa-reinitalization
 */
void
apply_prefs(gint alsa_change)
{
	/* Scroll step */
	scroll_step = prefs_get_integer("ScrollStep", 5);

	/* Hotkeys preferences */
	if (prefs_get_boolean("EnableHotKeys", FALSE)) {
		gint mk, uk, dk, mm, um, dm, hstep;
		mk = prefs_get_integer("VolMuteKey", -1);
		uk = prefs_get_integer("VolUpKey", -1);
		dk = prefs_get_integer("VolDownKey", -1);
		mm = prefs_get_integer("VolMuteMods", 0);
		um = prefs_get_integer("VolUpMods", 0);
		dm = prefs_get_integer("VolDownMods", 0);
		hstep = prefs_get_integer("HotkeyVolumeStep", 1);
		hotkeys_grab(mk, uk, dk, mm, um, dm, hstep);
	} else
		// will actually just ungrab everything
		hotkeys_grab(-1, -1, -1, 0, 0, 0, 1);

	/* Notifications preferences */
	enable_noti = prefs_get_boolean("EnableNotifications", FALSE);
	hotkey_noti = prefs_get_boolean("HotkeyNotifications", TRUE);
	mouse_noti = prefs_get_boolean("MouseNotifications", TRUE);
	popup_noti = prefs_get_boolean("PopupNotifications", FALSE);
	external_noti = prefs_get_boolean("ExternalNotifications", FALSE);
	noti_timeout = prefs_get_integer("NotificationTimeout", 1500);

	/* Popup window, rebuild it from scratch. This is needed in case
	 * the slider orientation was modified.
	 */
	popup_window_destroy(popup_window);
	popup_window = popup_window_create();

	/* Tray icon, reload */
	tray_icon_reload_prefs(tray_icon);

	/* Update the whole ui */
	do_update_ui();

	/* Reload alsa */
	if (alsa_change)
		audio_reinit();
}

/**
 * Runs a given command via g_spawn_command_line_async().
 *
 * @param cmd the command to run
 */
static void
run_command(const gchar *cmd)
{
	GError *error = NULL;

	g_assert(cmd != NULL);

	if (g_spawn_command_line_async(cmd, &error) == FALSE) {
		report_error(_("Unable to run command: %s"), error->message);
		g_error_free(error);
		error = NULL;
	}
}

/**
 * Brings up the preferences window, either triggered by clicking
 * on the GtkImageMenuItem 'Preferences' in the context menu
 * or by middle-click if the Middle Click Action in the preferences
 * is set to 'Show Preferences'.
 */
void
do_open_prefs(void)
{
	prefs_window_open();
}

void
do_toggle_popup_window(void)
{
	popup_window_toggle(popup_window);
}

void
do_show_popup_menu(GtkMenuPositionFunc func, gpointer data, guint button, guint activate_time)
{
	popup_window_hide(popup_window);
	popup_menu_show(popup_menu, func, data, button, activate_time);
}

/**
 * Opens the specified mixer application which can be triggered either
 * by clicking the 'Volume Control' GtkImageMenuItem in the context
 * menu, the GtkButton 'Mixer' in the left-click popup_window or
 * by middle-click if the Middle Click Action in the preferences
 * is set to 'Volume Control'.
 */
void
do_mixer(void)
{
	gchar *cmd;

	cmd = prefs_get_vol_command();

	if (cmd) {
		run_command(cmd);
		g_free(cmd);
	} else
		report_error(_
			     ("No mixer application was found on your system. "
			      "Please open preferences and set the command you want "
			      "to run for volume control."));
}

void
do_custom_command(void)
{
	gchar *cmd;

	cmd = prefs_get_string("CustomCommand", NULL);

	if (cmd) {
		run_command(cmd);
		g_free(cmd);
	} else
		report_error(_("You have not specified a custom command to run, "
			       "please specify one in preferences."));
}

/**
 * Updates the states that always needs to be updated on volume changes.
 * This is currently the tray icon and the mute checkboxes.
 */
void
do_update_ui(void)
{
	tray_icon_update(tray_icon);
	popup_window_update(popup_window);
	popup_menu_update(popup_menu);
}

static gboolean version = FALSE;
static GOptionEntry args[] = {
	{
		"version", 0, 0, G_OPTION_ARG_NONE, &version, "Show version and exit",
		NULL
	},
	{
		"debug", 'd', 0, G_OPTION_ARG_NONE, &want_debug, "Run in debug mode",
		NULL
	},
	{NULL, 0, 0, 0, NULL, NULL, NULL}
};

/**
 * Program entry point. Initializes gtk+, calls the widget creating
 * functions and starts the main loop. Also connects 'popup-menu',
 * 'activate' and 'button-release-event' to the tray_icon.
 *
 * @param argc count of arguments
 * @param argv string array of arguments
 * @return 0 for success, otherwise error code
 */
int
main(int argc, char *argv[])
{
	GError *error = NULL;
	GOptionContext *context;
	want_debug = FALSE;

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	setlocale(LC_ALL, "");

	/* Parse options */
	context = g_option_context_new(_("- A mixer for the system tray."));
	g_option_context_add_main_entries(context, args, GETTEXT_PACKAGE);
	g_option_context_add_group(context, gtk_get_option_group(TRUE));
	g_option_context_parse(context, &argc, &argv, &error);
	g_option_context_free(context);

	/* Print version and exit */
	if (version) {
		printf(_("%s version: %s\n"), PACKAGE, VERSION);
		exit(EXIT_SUCCESS);
	}

	/* Init Gtk+ */
	gtk_init(&argc, &argv);

	add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");
	add_pixmap_directory("./data/pixmaps");

	/* Load preferences */
	prefs_ensure_save_dir();
	prefs_load();


	/* Init everything */
	audio_init();
	init_libnotify();
	hotkeys_add_filter();

	popup_menu = popup_menu_create();
	popup_window = popup_window_create();
	tray_icon = tray_icon_create();

	// We make the main window public, it's needed as a transient parent
	main_window = popup_window_get_gtk_window(popup_window);

	/* Apply preferences */
	DEBUG("Applying prefs...");
	apply_prefs(0);

	/* Run */
	DEBUG("Running main loop...");
	gtk_main();

	/* Cleanup */

	main_window = NULL;

	tray_icon_destroy(tray_icon);
	popup_window_destroy(popup_window);
	popup_menu_destroy(popup_menu);

	uninit_libnotify();
	audio_cleanup();

	return EXIT_SUCCESS;
}
