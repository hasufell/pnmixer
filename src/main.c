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
#include <glib.h>

#include "main.h"
#include "audio.h"
#include "notif.h"
#include "hotkeys.h"
#include "prefs.h"
#include "support-intl.h"
#include "support-log.h"
#include "ui-about-dialog.h"
#include "ui-prefs-dialog.h"
#include "ui-popup-menu.h"
#include "ui-popup-window.h"
#include "ui-tray-icon.h"

/* Life-long instances */
static Audio *audio;
static PopupMenu *popup_menu;
static PopupWindow *popup_window;
static TrayIcon *tray_icon;
static Hotkeys *hotkeys;
static Notif *notif;

/* Pointer toward the main window. We need it as a parent for dialogs. */
static GtkWindow *main_window;

/* Temporary instances */
static PrefsDialog *prefs_dialog;
static AboutDialog *about_dialog;

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
		run_error_dialog(_("Unable to run command: %s"), error->message);
		g_error_free(error);
		error = NULL;
	}
}

/**
 * Opens the specified mixer application which can be triggered either
 * by clicking the 'Volume Control' GtkImageMenuItem in the context
 * menu, the GtkButton 'Mixer' in the left-click popup_window or
 * by middle-click if the Middle Click Action in the preferences
 * is set to 'Volume Control'.
 */
void
run_mixer_command(void)
{
	gchar *cmd;

	cmd = prefs_get_string("VolumeControlCommand", NULL);

	if (cmd) {
		run_command(cmd);
		g_free(cmd);
	} else {
		run_error_dialog(_("No mixer application was found on your system. "
		                   "Please open preferences and set the command you want "
		                   "to run for volume control."));
	}
}

void
run_custom_command(void)
{
	gchar *cmd;

	cmd = prefs_get_string("CustomCommand", NULL);

	if (cmd) {
		run_command(cmd);
		g_free(cmd);
	} else {
		run_error_dialog(_("You have not specified a custom command to run, "
		                   "please specify one in preferences."));
	}
}

/**
 * Brings up the preferences window, either triggered by clicking
 * on the GtkImageMenuItem 'Preferences' in the context menu
 * or by middle-click if the Middle Click Action in the preferences
 * is set to 'Show Preferences'.
 */
void
run_prefs_dialog(void)
{
	gint resp;

	/* Ensure there's no dialog already running */
	if (prefs_dialog)
		return;

	/* Create the preferences dialog */
	prefs_dialog = prefs_dialog_create(main_window, audio, hotkeys);
	prefs_dialog_populate(prefs_dialog);

	/* Run it */
	resp = prefs_dialog_run(prefs_dialog);
	if (resp == GTK_RESPONSE_OK)
		prefs_dialog_retrieve(prefs_dialog);

	/* Destroy it */
	prefs_dialog_destroy(prefs_dialog);
	prefs_dialog = NULL;

	/* Now apply the new preferences.
	 * It's safer to do that after destroying the preference dialog,
	 * since it listens for some audio signals that will be emitted
	 * while new prefs are applied.
	 */
	if (resp == GTK_RESPONSE_OK) {
		/* Popup window, rebuild it from scratch. This is needed in case
		 * the slider orientation was modified.
		 */
		popup_window_destroy(popup_window);
		popup_window = popup_window_create(audio);
		main_window = popup_window_get_gtk_window(popup_window);

		/* Tray icon preferences */
		tray_icon_reload(tray_icon);

		/* Hotkeys preferences */
		hotkeys_reload(hotkeys);

		/* Notifications preferences */
		notif_reload(notif);

		/* Reload audio */
		audio_reload(audio);

		/* Save preferences to file */
		prefs_save();
	}
}

/**
 * Run the abut dialog.
 */
void
run_about_dialog(void)
{
	/* Ensure there's no dialog already running */
	if (about_dialog)
		return;

	/* Run the about dialog */
	about_dialog = about_dialog_create(main_window);
	about_dialog_run(about_dialog);
	about_dialog_destroy(about_dialog);
	about_dialog = NULL;
}

/**
 * Report an error, usually via a dialog window or on stderr.
 *
 * @param err the error
 * @param ... more string segments in the format of printf
 */
void
run_error_dialog(char *fmt, ...)
{
	GtkWidget *dialog;
	char err_buf[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(err_buf, sizeof err_buf, fmt, ap);
	va_end(ap);

	ERROR("%s", err_buf);

	if (!main_window)
		return;

	dialog = gtk_message_dialog_new(main_window,
	                                GTK_DIALOG_DESTROY_WITH_PARENT,
	                                GTK_MESSAGE_ERROR,
	                                GTK_BUTTONS_CLOSE,
	                                NULL);

	g_object_set(dialog, "text", err_buf, NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), _("PNMixer Error"));
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

/**
 * Emits a warning if the sound connection is lost, usually
 * via a dialog window (with option to reinitialize sound) or stderr.
 * Asks for alsa reload, check the return value to know.
 */
gint
run_audio_error_dialog(void)
{
	GtkWidget *dialog;
	gint resp;

	ERROR("Connection with audio failed, "
	      "you probably need to restart pnmixer");

	if (!main_window)
		return GTK_RESPONSE_NO;

	dialog = gtk_message_dialog_new
	         (main_window,
	          GTK_DIALOG_DESTROY_WITH_PARENT,
	          GTK_MESSAGE_ERROR,
	          GTK_BUTTONS_YES_NO,
	          _("Warning: Connection to sound system failed."));

	gtk_message_dialog_format_secondary_text
	(GTK_MESSAGE_DIALOG(dialog),
	 _("Do you want to re-initialize the audio connection ?\n\n"
	   "If you do not, you will either need to restart PNMixer "
	   "or select the 'Reload Audio' option in the right-click "
	   "menu in order for PNMixer to function."));

	gtk_window_set_title(GTK_WINDOW(dialog), _("PNMixer Error"));
	resp = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	return resp;
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

static void
on_audio_changed(G_GNUC_UNUSED Audio *audio, AudioEvent *event, G_GNUC_UNUSED gpointer data)
{
	switch (event->signal) {
	case AUDIO_CARD_DISCONNECTED:
		audio_reload(audio);
		break;
	case AUDIO_CARD_ERROR:
		if (run_audio_error_dialog() == GTK_RESPONSE_YES)
			audio_reload(audio);
		break;
	default:
		break;
	}

}

static gboolean version = FALSE;
static GOptionEntry args[] = {
	{ "version", 0, 0, G_OPTION_ARG_NONE, &version, "Show version and exit", NULL },
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &want_debug, "Run in debug mode", NULL },
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
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
	GOptionContext *context;

	/* Init internationalization stuff */
	intl_init();

	/* Parse options */
	context = g_option_context_new(_("- A mixer for the system tray."));
	g_option_context_add_main_entries(context, args, GETTEXT_PACKAGE);
	g_option_context_add_group(context, gtk_get_option_group(TRUE));
	g_option_context_parse(context, &argc, &argv, NULL);
	g_option_context_free(context);

	/* Print version and exit */
	if (version) {
		printf(_("%s version: %s\n"), PACKAGE, VERSION);
		exit(EXIT_SUCCESS);
	}

	/* Init Gtk+ */
	gtk_init(&argc, &argv);

	/* Load preferences.
	 * This must be done at first, all the following init code rely on it.
	 */
	prefs_ensure_save_dir();
	prefs_load();

	/* Init the low-level (aka the audio system) at first */
	audio = audio_new();

	/* Init the high-level (aka the ui) */
	popup_menu = popup_menu_create(audio);
	popup_window = popup_window_create(audio);
	tray_icon = tray_icon_create(audio);

	/* Get a pointer toward the main window (needed to run dialogs) */
	main_window = popup_window_get_gtk_window(popup_window);

	/* Init what's left */
	hotkeys = hotkeys_new(audio);
	notif = notif_new(audio);

	/* Get the audio system ready */
	audio_signals_connect(audio, on_audio_changed, NULL);
	audio_reload(audio);

	/* Run */
	DEBUG("---- Running main loop ----");
	gtk_main();
	DEBUG("---- Exiting main loop ----");

	/* Cleanup */
	audio_signals_disconnect(audio, on_audio_changed, NULL);
	notif_free(notif);
	hotkeys_free(hotkeys);
	tray_icon_destroy(tray_icon);
	popup_window_destroy(popup_window);
	popup_menu_destroy(popup_menu);
	audio_free(audio);

	return EXIT_SUCCESS;
}
