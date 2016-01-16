/* main.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file main.h
 * Header for main.c holding public functions and debug macros.
 * @brief header for main.c
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

gboolean enable_noti, hotkey_noti, popup_noti, external_noti;
gint noti_timeout;

void do_mute(gboolean notify);
void do_open_prefs(void);
void do_mixer(void);
void do_custom_command(void);
void do_alsa_reinit(void);
void do_toggle_popup_window(void);
void do_show_popup_menu(GtkStatusIcon *status_icon, guint button, guint activate_time);

void do_update_ui(void);


gint get_tray_icon_size(void);

void report_error(char *, ...);
void warn_sound_conn_lost(void);

#endif				// MAIN_H
