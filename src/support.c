/* support.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived 
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute 
 * it and/or modify it under the terms of the GNU General 
 * Public License v3. source code is available at 
 * <http://github.com/nicklan/pnmixer>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gtk/gtk.h>

#include "main.h"
#include "support.h"
#include "prefs.h"

static GList *pixmaps_directories = NULL;

/* Use this function to set the directory containing installed pixmaps. */
void add_pixmap_directory(const gchar *directory) {
  pixmaps_directories = g_list_prepend (pixmaps_directories,
                                        g_strdup (directory));
}

/* This is an internally used function to find pixmap files. */
static gchar* find_pixmap_file(const gchar *filename) {
  GList *elem;

  /* We step through each of the pixmaps directory to find it. */
  elem = pixmaps_directories;
  while (elem)
    {
      gchar *pathname = g_strdup_printf ("%s%s%s", (gchar*)elem->data,
                                         G_DIR_SEPARATOR_S, filename);
      if (g_file_test (pathname, G_FILE_TEST_EXISTS))
        return pathname;
      g_free (pathname);
      elem = elem->next;
    }
  return NULL;
}

/* This is an internally used function to create pixmaps. */
GtkWidget* create_pixmap(GtkWidget   *widget,
			 const gchar *filename) {
  gchar *pathname = NULL;
  GtkWidget *pixmap;

  if (!filename || !filename[0])
    return gtk_image_new ();

  pathname = find_pixmap_file (filename);

  if (!pathname)
    {
      g_warning (_("Couldn't find pixmap file: %s"), filename);
      return gtk_image_new ();
    }

  pixmap = gtk_image_new_from_file (pathname);
  g_free (pathname);
  return pixmap;
}

/* This is an internally used function to create pixmaps. */
GdkPixbuf* create_pixbuf(const gchar *filename) {
  gchar *pathname = NULL;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  if (!filename || !filename[0])
    return NULL;

  pathname = find_pixmap_file (filename);

  if (!pathname)
    {
      g_warning (_("Couldn't find pixmap file: %s"), filename);
      return NULL;
    }

  pixbuf = gdk_pixbuf_new_from_file (pathname, &error);
  if (!pixbuf)
    {
      report_error(_("Failed to load pixbuf file: %s: %s\n"),
		   pathname, error->message);
      g_error_free (error);
    }
  g_free (pathname);
  return pixbuf;
}


/* lookup icons based on theme */
GdkPixbuf* get_stock_pixbuf(const char* filename, gint size) {
  GError *err = NULL;
  GdkPixbuf *return_buf = NULL;
  if (icon_theme == NULL) 
    get_icon_theme();
  return_buf = gtk_icon_theme_load_icon(icon_theme,filename,size,0,&err);
  if (err != NULL) {
    report_error(_("Unable to load icon %s: %s\n"), filename, err->message);
    g_error_free (err);
  }
  return return_buf;
}

/* Return path to ui file, or NULL if file can't be found.
   Looks first in PACKAGE_DATA_DIR/pnmixer/ui/[file] and then in ./data/[file] */
gchar* get_ui_file(const char* filename) {
  gchar* path;
  path = g_build_filename(PACKAGE_DATA_DIR,"pnmixer","ui",filename,NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    return path;
  g_free(path);
  path = g_build_filename(".","data",filename,NULL);
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    return path;
  g_free(path);
  return NULL;
}
