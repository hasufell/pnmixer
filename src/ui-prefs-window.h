/* ui-prefs-window.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-prefs-window.h
 * Header for ui-prefs-window.c, holding public functions and globals.
 * @brief header for ui-prefs-window.c
 */

#ifndef _UI_PREFS_WINDOW_H_
#define _UI_PREFS_WINDOW_H_

#include "audio.h"

typedef struct prefs_window PrefsWindow;

PrefsWindow *prefs_dialog_create(GtkWindow *parent, Audio *audio);
void prefs_dialog_destroy(PrefsWindow *dialog);
gint prefs_dialog_run(PrefsWindow *dialog);

void prefs_dialog_populate(PrefsWindow *dialog);
void prefs_dialog_retrieve(PrefsWindow *dialog);

#endif				// _UI_PREFS_WINDOW_H_
