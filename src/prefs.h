/* prefs.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file prefs.h
 * Header for prefs.c, holding public functions and globals.
 * @brief header for prefs.c
 */

#ifndef _PREFS_H_
#define _PREFS_H_

#include <glib.h>
#include <gtk/gtk.h>

gboolean prefs_get_boolean(const gchar *key, gboolean def);
gint     prefs_get_integer(const gchar *key, gint def);
gdouble  prefs_get_double(const gchar *key, gdouble def);
gchar   *prefs_get_string(const gchar *key, const gchar *def);
gdouble *prefs_get_double_list(const gchar *key, gsize *n);

gchar   *prefs_get_channel(const gchar *card);
gchar   *prefs_get_vol_command(void);

void prefs_set_boolean(const gchar *key, gboolean value);
void prefs_set_integer(const gchar *key, gint value);
void prefs_set_double(const gchar *key, gdouble value);
void prefs_set_string(const gchar *key, const gchar *value);
void prefs_set_double_list(const gchar *key, gdouble *list, gsize n);

void prefs_set_channel(const gchar *card, const gchar *channel);

void prefs_load(void);
void prefs_save(void);
void prefs_ensure_save_dir(void);

#endif				// _PREFS_H_
