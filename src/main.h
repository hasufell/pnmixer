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

gboolean enable_noti, hotkey_noti, mouse_noti, popup_noti, external_noti;
gint noti_timeout;

void create_popups(void);
void create_about(void);
void do_mute(gboolean notify);
void do_mixer(void);
void do_prefs(void);
void do_alsa_reinit(void);

void report_error(char *, ...);
void warn_sound_conn_lost(void);
void get_current_levels(void);
void update_tray_icon(void);
void update_mute_checkboxes(void);
void on_volume_has_changed(void);
gint tray_icon_size(void);
void set_vol_meter_color(gdouble nr, gdouble ng, gdouble nb);
void update_status_icons(void);

#endif				// MAIN_H
