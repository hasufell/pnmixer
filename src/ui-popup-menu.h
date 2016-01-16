/* ui-popup-menu.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-popup-menu.h
 * Header for ui-popup-menu.c, holding public functions and globals.
 * @brief header for ui-popup-menu.c
 */

#ifndef UI_POPUP_MENU_H_
#define UI_POPUP_MENU_H_

#include <gtk/gtk.h>

struct popup_menu {
	GtkMenu *menu;
	GtkWidget *mute_check;
};

typedef struct popup_menu PopupMenu;

PopupMenu *popup_menu_create(void);
void       popup_menu_destroy(PopupMenu *menu);
void       popup_menu_update(PopupMenu *menu);
void       popup_menu_show(PopupMenu *menu, GtkStatusIcon *status_icon,
                           guint button, guint activate_time);

#endif				// UI_POPUP_MENU_H_
