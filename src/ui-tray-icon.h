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

#include "audio.h"

typedef struct tray_icon TrayIcon;

TrayIcon *tray_icon_create(Audio *audio);
void tray_icon_destroy(TrayIcon *tray_icon);
void tray_icon_reload_prefs(TrayIcon *tray_icon);
void tray_icon_update(TrayIcon *tray_icon);

#endif				// UI_TRAY_ICON_H_
