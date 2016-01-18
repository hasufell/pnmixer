/* about-dialog.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file about-dialog.c
 * This file holds the ui-related code for the about dialog
 * @brief About dialog subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#include "debug.h"
#include "support.h"
#include "ui-about-dialog.h"

#ifdef WITH_GTK3
#define ABOUT_UI_FILE      "about-gtk3.glade"
#else
#define ABOUT_UI_FILE      "about-gtk2.glade"
#endif

struct about_dialog {
	GtkWidget *dialog;
};

/* Public functions */

void
about_dialog_run(AboutDialog *dialog)
{
	gtk_dialog_run(GTK_DIALOG(dialog->dialog));
}

void
about_dialog_destroy(AboutDialog *dialog)
{
	gtk_widget_destroy(dialog->dialog);
	g_free(dialog);
}

AboutDialog *
about_dialog_create(void)
{
	gchar *uifile;
	GtkBuilder *builder;
	AboutDialog *dialog;

	uifile = get_ui_file(ABOUT_UI_FILE);
	g_assert(uifile);

	DEBUG_PRINT("Building about dialog from ui file '%s'", uifile);
	builder = gtk_builder_new_from_file(uifile);

	dialog = g_new(AboutDialog, 1);
	assign_gtk_widget(builder, dialog, dialog);

	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog->dialog), VERSION);

	gtk_builder_connect_signals(builder, NULL);

	g_object_unref(builder);
	g_free(uifile);

	return dialog;
}
