/* ui-popup-window.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-popup-window.h
 * Header for ui-popup-window.c, holding public functions and globals.
 * @brief header for ui-popup-window.c
 */

#ifndef UI_POPUP_WINDOW_H_
#define UI_POPUP_WINDOW_H_

#include <gtk/gtk.h>

struct popup_window {
	GtkWidget *window;
	GtkWidget *vol_scale;
	GtkAdjustment *vol_adj;
	GtkWidget *mute_check;
};

typedef struct popup_window PopupWindow;

PopupWindow *popup_window_create(void);
void         popup_window_destroy(PopupWindow *window);
void         popup_window_update(PopupWindow *window);

#endif				// UI_POPUP_WINDOW_H_
