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

#ifndef PREFS_H_
#define PREFS_H_

#include <glib.h>
#include <gtk/gtk.h>

#include "support.h"

gint scroll_step, fine_scroll_step;
GtkIconTheme *icon_theme;

gboolean prefs_get_boolean(gchar *key, gboolean def);
gint     prefs_get_integer(gchar *key, gint def);
gdouble  prefs_get_double(gchar *key, gdouble def);
gchar   *prefs_get_string(gchar *key, const gchar *def);
gchar   *prefs_get_channel(const gchar *card);
gchar   *prefs_get_vol_command(void);
gdouble *prefs_get_vol_meter_colors(void);

void prefs_set_boolean(const gchar *key, gboolean value);
void prefs_set_integer(const gchar *key, gint value);
void prefs_set_double(const gchar *key, gdouble value);
void prefs_set_string(const gchar *key, const gchar *value);
void prefs_set_channel(const gchar *card, const gchar *channel);
void prefs_set_vol_meter_colors(gdouble *colors, gsize n);

void prefs_load(void);
void prefs_save(void);
void prefs_ensure_save_dir(void);

#endif				// PREFS_H_
