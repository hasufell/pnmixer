/* ui-tray-icon.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-tray-icon.h
 * Header for ui-tray-icon.c, holding public functions and globals.
 * @brief header for ui-tray-icon.c
 */

#ifndef UI_TRAY_ICON_H_
#define UI_TRAY_ICON_H_

void tray_icon_create(void);
void tray_icon_destroy(void);
void tray_icon_update(void);
void tray_icon_reload_prefs(void);
gint tray_icon_get_size(void);

#endif				// UI_TRAY_ICON_H_
