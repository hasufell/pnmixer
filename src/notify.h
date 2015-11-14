/* notify.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file notify.h
 * Header for notify.c.
 * @brief header for notify.c
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef NOTIFY_H_
#define NOTIFY_H_

#include <glib.h>

void do_notify_volume(gint level, gboolean muted);
void do_notify_text(const gchar *title, const gchar *text);

#endif				// NOTIFY_H_
