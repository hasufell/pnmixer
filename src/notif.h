/* notif.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file notif.h
 * Header for notif.c.
 * @brief header for notif.c
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef NOTIF_H_
#define NOTIF_H_

enum notif_source {
	NOTIF_UNKNOWN,
	NOTIF_POPUP,
	NOTIF_TRAY,
	NOTIF_HOTKEY,
};

void notif_init(void);
void notif_cleanup(void);
void notif_reload_prefs(void);
void notif_inform(enum notif_source source);

#endif				// NOTIF_H_
