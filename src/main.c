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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <locale.h>
#include "alsa.h"
#include "debug.h"
#include "main.h"
#include "notify.h"
#include "support.h"
#include "hotkeys.h"
#include "prefs.h"
#include "ui-prefs.h"
#include "ui-popup-menu.h"
#include "ui-popup-window.h"
#include "ui-tray-icon.h"

static char err_buf[512];

static PopupMenu *popup_menu;
static PopupWindow *popup_window;

/**
 * Reports an error, usually via a dialog window or
 * on stderr.
 *
 * @param err the error
 * @param ... more string segments in the format of printf
 */
void
report_error(char *err, ...)
{
	va_list ap;
	va_start(ap, err);
	if (popup_window) {
		vsnprintf(err_buf, 512, err, ap);
		GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(popup_window->window),
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_CLOSE,
				    NULL);
		gtk_window_set_title(GTK_WINDOW(dialog), _("PNMixer Error"));
		g_object_set(dialog, "text", err_buf, NULL);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	} else {
		vfprintf(stderr, err, ap);
		fprintf(stderr, "\n");
	}
	va_end(ap);
}

/**
 * Emits a warning if the sound connection is lost, usually
 * via a dialog window (with option to reinitialize alsa) or stderr.
 */
void
warn_sound_conn_lost(void)
{
	if (popup_window) {
		gint resp;
		GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(popup_window->window),
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_YES_NO,
				    _("Warning: Connection to sound system failed."));
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
				_("Do you want to re-initialize the connection to alsa?\n\n"
				  "If you do not, you will either need to restart PNMixer "
				  "or select the 'Reload Alsa' option in the right click "
				  "menu in order for PNMixer to function."));
		gtk_window_set_title(GTK_WINDOW(dialog), _("PNMixer Error"));
		resp = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		if (resp == GTK_RESPONSE_YES)
			do_alsa_reinit();
	} else
		fprintf(stderr,
			_("Warning: Connection to sound system failed, "
			  "you probably need to restart pnmixer\n"));
}

/**
 * Sets the global options enable_noti, hotkey_noti, popup_noti,
 * noti_timeout and external_noti from the user settings.
 */
static void
set_notification_options(void)
{
	enable_noti = prefs_get_boolean("EnableNotifications", FALSE);
	hotkey_noti = prefs_get_boolean("HotkeyNotifications", TRUE);
	popup_noti = prefs_get_boolean("PopupNotifications", FALSE);
	external_noti = prefs_get_boolean("ExternalNotifications", FALSE);
	noti_timeout = prefs_get_integer("NotificationTimeout", 1500);
}

/**
 * Applies the preferences, usually triggered by on_ok_button_clicked()
 * in callbacks.c, but also initially called from main().
 *
 * @param alsa_change whether we want to trigger alsa-reinitalization
 */
void
apply_prefs(gint alsa_change)
{
	GtkAdjustment *vol_adj = popup_window->vol_adj;

	scroll_step = prefs_get_integer("ScrollStep", 5);
	gtk_adjustment_set_page_increment(vol_adj, scroll_step);

	fine_scroll_step = prefs_get_integer("FineScrollStep", 1);
	gtk_adjustment_set_step_increment(vol_adj, fine_scroll_step);

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

	set_notification_options();

	tray_icon_reload_prefs();

	popup_window_update(popup_window);

	if (alsa_change)
		do_alsa_reinit();
}

static gboolean
on_prefs_ok_clicked(G_GNUC_UNUSED GtkButton *button, UiPrefsData *data)
{
	/* Save values to preferences */
	ui_prefs_retrieve_values(data);
	ui_prefs_destroy_window(data);

	/* Save preferences to file */
	prefs_save();

	/* Make it effective */
	apply_prefs(1);

	return TRUE;
}

static gboolean
on_prefs_cancel_clicked(G_GNUC_UNUSED GtkButton *button, UiPrefsData *data)
{
	ui_prefs_destroy_window(data);

	return TRUE;
}

/**
 * Callback function when a key is hit in prefs_window. Currently handles
 * Esc key (calls on_cancel_button_clicked())
 * and
 * Return key (calls on_ok_button_clicked()).
 *
 * @param widget the widget that received the signal
 * @param event the key event that was triggered
 * @param data struct holding the GtkWidgets of the preferences windows
 * @return TRUE to stop other handlers from being invoked for the event.
 * False to propagate the event further
 */
static gboolean
on_key_pressed(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event, UiPrefsData *data)
{
	switch (event->keyval) {
	case GDK_KEY_Escape:
		return on_prefs_cancel_clicked(NULL, data);
		break;
	case GDK_KEY_Return:
		return on_prefs_ok_clicked(NULL, data);
		break;
	default:
		return FALSE;
	}
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

	gtk_widget_hide(popup_window->window);

	if (g_spawn_command_line_async(cmd, &error) == FALSE) {
		report_error(_("Unable to run command: %s"), error->message);
		g_error_free(error);
		error = NULL;
	}
}

void
do_mute(gboolean notify)
{
        setmute(notify);
        on_volume_has_changed();
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
	ui_prefs_create_window(G_CALLBACK(on_prefs_ok_clicked),
	                       G_CALLBACK(on_prefs_cancel_clicked),
	                       G_CALLBACK(on_key_pressed));
}

void
do_toggle_popup_window(void)
{
	popup_window_toggle(popup_window);
}

void
do_show_popup_menu(GtkStatusIcon *status_icon, guint button, guint activate_time)
{
	popup_window_hide(popup_window);
	popup_menu_show(popup_menu, status_icon, button, activate_time);
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
 * Reinitializes alsa and updates the various states.
 */
void
do_alsa_reinit(void)
{
	alsa_init();
	tray_icon_update();
	popup_window_update(popup_window);
	on_volume_has_changed();
}

gint
get_tray_icon_size(void)
{
	return tray_icon_get_size();
}

/**
 * Gets the current volume level and adjusts the GtkAdjustment
 * vol_scale_adjustment widget which is used by GtkHScale/GtkScale.
 */
void
get_current_levels(void)
{
	int tmpvol = getvol();
	GtkAdjustment *vol_adj = popup_window->vol_adj;

	gtk_adjustment_set_value(vol_adj, (double) tmpvol);
}


/**
 * Updates the states that always needs to be updated on volume changes.
 * This is currently the tray icon and the mute checkboxes.
 * TODO: rename !
 */
void
on_volume_has_changed(void)
{
	tray_icon_update();
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

	DEBUG_PRINT("[Debugging Mode Build]\n");

	setlocale(LC_ALL, "");
	context = g_option_context_new(_("- A mixer for the system tray."));
	g_option_context_add_main_entries(context, args, GETTEXT_PACKAGE);
	g_option_context_add_group(context, gtk_get_option_group(TRUE));
	g_option_context_parse(context, &argc, &argv, &error);
	gtk_init(&argc, &argv);

	g_option_context_free(context);

	if (version) {
		printf(_("%s version: %s\n"), PACKAGE, VERSION);
		exit(0);
	}

	popup_window = NULL;

	add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");
	add_pixmap_directory("./data/pixmaps");

	prefs_ensure_save_dir();
	prefs_load();
	cards = NULL;		// so we don't try and free on first run
	alsa_init();
	init_libnotify();

	hotkeys_add_filter();

	// Popups
	popup_menu = popup_menu_create();
	popup_window = popup_window_create();

	// Tray icon
	tray_icon_create();

	// Set preferences
	apply_prefs(0);

	// Update levels
	get_current_levels();


	gtk_main();

	tray_icon_destroy();
	popup_window_destroy(popup_window);
	popup_menu_destroy(popup_menu);

	uninit_libnotify();
	alsa_close();

	return EXIT_SUCCESS;
}
