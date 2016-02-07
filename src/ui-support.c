/* ui-support.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-support.c
 * High-level dialogs that are tied to the main window.
 * @brief Various ui-related functions.
 */

#include <glib.h>
#include <gtk/gtk.h>

#include "support-log.h"
#include "support-intl.h"
#include "support-log.h"
#include "ui-about-dialog.h"

#include "main.h"

static AboutDialog *about_dialog;


#ifndef WITH_GTK3
GtkBuilder *
gtk_builder_new_from_file (const gchar *filename)
{
	GError *error = NULL;
	GtkBuilder *builder;

	builder = gtk_builder_new ();
	if (!gtk_builder_add_from_file (builder, filename, &error))
		g_error ("failed to add UI: %s", error->message);

	return builder;
}
#endif

/**
 * Gets the path to an ui file.
 * Looks first in ./data/ui/[file], and then in
 * PACKAGE_DATA_DIR/pnmixer/ui/[file].
 *
 * @param filename the name of the ui file
 * @return path to the ui file or NULL on failure
 */
gchar *
ui_get_builder_file(const char *filename)
{
	gchar *path;
	path = g_build_filename(".", "data", "ui", filename, NULL);
	if (g_file_test(path, G_FILE_TEST_EXISTS))
		return path;
	g_free(path);
	path = g_build_filename(PACKAGE_DATA_DIR, "pnmixer", "ui",
				filename, NULL);
	if (g_file_test(path, G_FILE_TEST_EXISTS))
		return path;
	g_free(path);
	return NULL;
}

/**
 * Reports an error, usually via a dialog window or on stderr.
 *
 * @param err the error
 * @param ... more string segments in the format of printf
 */
void
ui_report_error(char *fmt, ...)
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
 */
gint
ui_run_audio_error_dialog(void)
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
ui_run_about_dialog(void)
{
	/* Ensure there's no dialog already running */
	if (about_dialog)
		return;

	/* Run the abut dialog */
	about_dialog = about_dialog_create(main_window);
	about_dialog_run(about_dialog);
	about_dialog_destroy(about_dialog);
	about_dialog = NULL;
}

