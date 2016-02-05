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

#include "audio.h"

typedef struct popup_window PopupWindow;

PopupWindow *popup_window_create(Audio *audio);
void popup_window_destroy(PopupWindow *window);
void popup_window_update(PopupWindow *window);
void popup_window_show(PopupWindow *window);
void popup_window_hide(PopupWindow *window);
void popup_window_toggle(PopupWindow *window);

#include <gtk/gtk.h>
GtkWindow *popup_window_get_gtk_window(PopupWindow *window);

#endif				// UI_POPUP_WINDOW_H_
