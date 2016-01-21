/* ui-hotkey-dialog.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-hotkey-dialog.h
 * Header for ui-hotkey-dialog.c, holding public functions and globals.
 * @brief header for ui-hotkey-dialog.c
 */

#ifndef UI_HOTKEY_DIALOG_H_
#define UI_HOTKEY_DIALOG_H_

#include <gtk/gtk.h>

gchar *hotkey_dialog_do(GtkWindow *parent, const gchar *hotkey);

#endif				// UI_HOTKEY_DIALOG_H_
