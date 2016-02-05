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
 * Header for main.c holding high-level public functions.
 * @brief header for main.c
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <gtk/gtk.h>

extern GtkWindow *main_window;

void do_open_prefs(void);
void do_mixer(void);
void do_custom_command(void);
void do_toggle_popup_window(void);
void do_show_popup_menu(GtkMenuPositionFunc func, gpointer data,
			guint button, guint activate_time);
void do_reload_audio(void);


void do_report_error(char *, ...);
void do_warn_sound_conn_lost(void);

void apply_prefs(void);

#endif				// MAIN_H
