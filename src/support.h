/* support.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file support.h
 * Header for support.c, holds public functions.
 * @brief header for support.c
 */

#ifndef SUPPORT_H_
#define SUPPORT_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

/*
 * Standard gettext macros.
 */
#ifdef ENABLE_NLS
#include <libintl.h>
#undef _
#define _(String) dgettext (PACKAGE, String)
#define Q_(String) g_strip_context ((String), gettext (String))
#ifdef gettext_noop
#define N_(String) gettext_noop (String)
#else
#define N_(String) (String)
#endif
#else
#define textdomain(String) (String)
#define gettext(String) (String)
#define dgettext(Domain, Message) (Message)
#define dcgettext(Domain, Message, Type) (Message)
#define bindtextdomain(Domain, Directory) (Domain)
#define _(String) (String)
#define Q_(String) g_strip_context ((String), (String))
#define N_(String) (String)
#endif

#define gtk_builder_get_widget(builder, name)	  \
	GTK_WIDGET(gtk_builder_get_object(builder, name))

#ifndef WITH_GTK3
GtkBuilder *gtk_builder_new_from_file (const gchar *filename);
#endif

/*
 * Public Functions.
 */

/* Use this function to set the directory containing installed pixmaps. */
void add_pixmap_directory(const gchar *directory);

/*
 * Private Functions.
 */

/* This is used to create the pixmaps used in the interface. */
GtkWidget *create_pixmap(GtkWidget *widget, const gchar *filename);

/* This is used to create the pixbufs used in the interface. */
GdkPixbuf *create_pixbuf(const gchar *filename);

GdkPixbuf *get_stock_pixbuf(const char *filename, gint size);

gchar *get_ui_file(const char *filename);

#endif				// SUPPORT_H_
