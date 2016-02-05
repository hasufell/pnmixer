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

/*
 * Printing, reporting, debugging.
 */

extern gboolean want_debug;

/**
 * Macro to print verbose debug info in case we want debugging.
 */

#define VT_ESC    "\033"
#define VT_RESET  "[0m"
#define VT_RED    "[0;31m"
#define VT_GREY   "[0;37m"
#define VT_YELLOW "[1;33m"

#define ERROR(fmt, ...)	  \
	fprintf(stderr, \
	        VT_ESC VT_RED "error" VT_ESC VT_RESET ": %s: " fmt "\n", \
	        __FILE__, ##__VA_ARGS__)

#define WARN(fmt, ...)	  \
	fprintf(stderr, \
	        VT_ESC VT_YELLOW "warning" VT_ESC VT_RESET ": %s: " fmt "\n",	\
	        __FILE__, ##__VA_ARGS__)

#define _DEBUG(fmt, ...)	  \
	fprintf(stderr, \
	        VT_ESC VT_GREY "debug" VT_ESC VT_RESET ": %s: " fmt "\n", \
	        __FILE__, ##__VA_ARGS__)

#define DEBUG(fmt, ...)	  \
	do { \
		if (want_debug == TRUE) \
			_DEBUG(fmt, ##__VA_ARGS__); \
	} while (0) \

/*
 * GtkBuilder helpers
 */

#define gtk_builder_get_widget(builder, name)	  \
	GTK_WIDGET(gtk_builder_get_object(builder, name))

#define gtk_builder_get_adjustment(builder, name)	  \
	GTK_ADJUSTMENT(gtk_builder_get_object(builder, name))

/*
 * 'assign' functions are used to retrieve a widget from a GtkBuilder
 * and to assign it into a structure. This is used a lot when we build
 * a window, and need to keep a pointer toward some widgets for later use.
 * This macro is more clever that it seems:
 * - it ensures that the name used in the struct is the same as the name used
 *   in the ui file, therefore enforcing a consistent naming across the code.
 * - it ensures that the widget was found amidst the GtkBuilder objects,
 *   therefore detecting errors that can happen when reworking the ui files.
 */

#define assign_gtk_widget(builder, container, name)	  \
	do { \
		container->name = gtk_builder_get_widget(builder, #name); \
		g_assert(GTK_IS_WIDGET(container->name)); \
	} while (0)

#define assign_gtk_adjustment(builder, container, name)	  \
	do { \
		container->name = gtk_builder_get_adjustment(builder, #name); \
		g_assert(GTK_IS_ADJUSTMENT(container->name)); \
	} while (0)


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

/* This is used to create the pixbufs used in the interface. */
GdkPixbuf *create_pixbuf(const gchar *filename);

GdkPixbuf *get_stock_pixbuf(const char *filename, gint size);

gchar *get_ui_file(const char *filename);

#endif				// SUPPORT_H_
