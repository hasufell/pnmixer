/* ui-about-dialog.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-about-dialog.c
 * This file holds the ui-related code for the about dialog
 * @brief About dialog subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#include "support.h"
#include "ui-about-dialog.h"

#include "main.h"

#ifdef WITH_GTK3
#define ABOUT_UI_FILE      "about-gtk3.glade"
#else
#define ABOUT_UI_FILE      "about-gtk2.glade"
#endif

static GtkAboutDialog *instance;

/* Private functions */

static GtkAboutDialog *
about_dialog_create(void)
{
	gchar *uifile;
	GtkBuilder *builder;
	GtkAboutDialog *dialog;

	/* Build UI file */
	uifile = get_ui_file(ABOUT_UI_FILE);
	g_assert(uifile);

	DEBUG("Building about dialog from ui file '%s'", uifile);
	builder = gtk_builder_new_from_file(uifile);

	/* Get the GtkAboutDialog widget from builder */
	dialog = GTK_ABOUT_DIALOG(gtk_builder_get_object(builder, "dialog"));

	/* Configure it */
	gtk_about_dialog_set_version(dialog, VERSION);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), main_window);

	/* Cleanup */
	g_object_unref(builder);
	g_free(uifile);

	return dialog;
}

/* Public functions */

/**
 * Creates the about dialog, run it and destroy it.
 * Only one instance at a time is allowed.
 */
void
about_dialog_run(void)
{
	if (instance)
		return;

	instance = about_dialog_create();
	gtk_dialog_run(GTK_DIALOG(instance));
	gtk_widget_destroy(GTK_WIDGET(instance));
	instance = NULL;
}
