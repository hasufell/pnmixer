/* ui-prefs.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-prefs.h
 * Header for ui-prefs.c, holding public functions and globals.
 * @brief header for ui-prefs.c
 */

#ifndef UI_PREFS_H_
#define UI_PREFS_H_

#include <gtk/gtk.h>

struct UiPrefsData;
typedef struct UiPrefsData UiPrefsData;

gboolean ui_prefs_create_window(GCallback on_ok_clicked, GCallback on_cancel_cliked,
                                GCallback on_key_pressed);
void     ui_prefs_destroy_window(UiPrefsData *data);
void     ui_prefs_retrieve_values(UiPrefsData *window);

#endif				// UI_PREFS_H_
