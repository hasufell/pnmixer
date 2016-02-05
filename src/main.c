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

#include "main.h"
#include "audio.h"
#include "notif.h"
#include "support.h"
#include "hotkeys.h"
#include "prefs.h"
#include "ui-prefs-window.h"
#include "ui-popup-menu.h"
#include "ui-popup-window.h"
#include "ui-tray-icon.h"

GtkWindow *main_window = NULL;

static Audio *audio;
static PopupMenu *popup_menu;
static PopupWindow *popup_window;
static TrayIcon *tray_icon;
static Hotkeys *hotkeys;
static Notif *notif;

/**
 * Reports an error, usually via a dialog window or on stderr.
 *
 * @param err the error
 * @param ... more string segments in the format of printf
 */
void
do_report_error(char *fmt, ...)
{
	va_list ap;
	char err_buf[512];

	va_start(ap, fmt);
	vsnprintf(err_buf, sizeof err_buf, fmt, ap);
	va_end(ap);

	ERROR("%s", err_buf);

	if (main_window) {
		GtkWidget *dialog = gtk_message_dialog_new(main_window,
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_CLOSE,
				    NULL);
		gtk_window_set_title(GTK_WINDOW(dialog), _("PNMixer Error"));
		g_object_set(dialog, "text", err_buf, NULL);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
}

/**
 * Emits a warning if the sound connection is lost, usually
 * via a dialog window (with option to reinitialize alsa) or stderr.
 * Also reload alsa.
 * TODO: move that alsa reloading outisde of here
 */
void
do_warn_sound_conn_lost(void)
{
	WARN("Warning: Connection to sound system failed, "
	     "you probably need to restart pnmixer");

	if (main_window) {
		gint resp;
		GtkWidget *dialog = gtk_message_dialog_new(main_window,
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
			do_reload_audio();
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

	if (g_spawn_command_line_async(cmd, &error) == FALSE) {
		do_report_error(_("Unable to run command: %s"), error->message);
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
	prefs_window_open(audio);
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
		do_report_error(_("No mixer application was found on your system. "
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
		do_report_error(_("You have not specified a custom command to run, "
		                  "please specify one in preferences."));
}

void
do_reload_audio(void)
{
	audio_unhook_soundcard(audio);
	audio_hook_soundcard(audio);
}


/**
 * Applies the preferences, usually triggered by on_ok_button_clicked()
 * in callbacks.c, but also initially called from main().
 *
 * @param alsa_change whether we want to trigger alsa-reinitalization
 */
void
apply_prefs(void)
{
	/* Audio */
	audio_reload_prefs(audio);

	/* Notifications preferences */
	notif_reload_prefs(notif);

	/* Hotkeys preferences */
	hotkeys_reload_prefs(hotkeys);

	/* Popup window, rebuild it from scratch. This is needed in case
	 * the slider orientation was modified.
	 */
	popup_window_destroy(popup_window);
	popup_window = popup_window_create(audio);

	/* Tray icon, reload */
	tray_icon_reload_prefs(tray_icon);

	/* Reload audio */
	do_reload_audio();
}

static void
on_audio_changed(G_GNUC_UNUSED Audio *audio, AudioEvent *event, G_GNUC_UNUSED gpointer data)
{
	switch (event->signal) {
	case AUDIO_CARD_DISCONNECTED:
		do_reload_audio();
		break;
	case AUDIO_CARD_ERROR:
		do_warn_sound_conn_lost();
		break;
	default:
		break;
	}

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

	/* Init the hotkeys control */
	hotkeys = hotkeys_new(audio);

	/* Init the notifications system */
	notif = notif_new(audio);
	if (!notif)
		do_report_error("Unable to initialize libnotify. "
		                "Notifications won't be sent.");

	// We make the main window public, it's needed as a transient parent
	main_window = popup_window_get_gtk_window(popup_window);

	/* Connect audio signals handlers */
	audio_signals_connect(audio, on_audio_changed, NULL);

	/* Hook the soundcard */
	audio_hook_soundcard(audio);

	/* Run */
	DEBUG("Running main loop...");
	gtk_main();

	/* Cleanup */

	main_window = NULL;

	audio_signals_disconnect(audio, on_audio_changed, NULL);

	notif_free(notif);
	hotkeys_free(hotkeys);
	tray_icon_destroy(tray_icon);
	popup_window_destroy(popup_window);
	popup_menu_destroy(popup_menu);
	audio_free(audio);

	return EXIT_SUCCESS;
}
