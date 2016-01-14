/* ui-prefs.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

#ifndef UI_PREFS_H_
#define UI_PREFS_H_

#include <gtk/gtk.h>

/**
 * Defines the whole preferences entity.
 */
typedef struct {
	GtkWidget *prefs_window;
	GtkWidget *card_combo;
	GtkWidget *chan_combo;
	GtkWidget *normalize_vol_check;
	GtkWidget *vol_pos_label;
	GtkWidget *vol_pos_combo;
	GtkWidget *vol_meter_pos_label;
	GtkWidget *vol_meter_pos_spin;
	GtkWidget *vol_meter_color_label;
	GtkWidget *vol_meter_color_button;
	GtkWidget *custom_label;
	GtkWidget *custom_entry;
	GtkWidget *slider_orientation_combo;
	GtkWidget *vol_text_check;
	GtkWidget *draw_vol_check;
	GtkWidget *system_theme;
	GtkWidget *vol_control_entry;
	GtkWidget *scroll_step_spin;
	GtkWidget *fine_scroll_step_spin;
	GtkWidget *middle_click_combo;
	GtkWidget *enable_hotkeys_check;
	GtkWidget *hotkey_vol_label;
	GtkWidget *hotkey_vol_spin;
	GtkWidget *hotkey_dialog;
	GtkWidget *hotkey_key_label;
	GtkWidget *mute_hotkey_label;
	GtkWidget *up_hotkey_label;
	GtkWidget *down_hotkey_label;
#ifdef HAVE_LIBN
	GtkWidget *enable_noti_check;
	GtkWidget *noti_timeout_label;
	GtkWidget *noti_timeout_spin;
	GtkWidget *hotkey_noti_check;
	GtkWidget *mouse_noti_check;
	GtkWidget *popup_noti_check;
	GtkWidget *external_noti_check;
#endif
} PrefsData;

/**
 * @file ui-prefs.h
 * Header for ui-prefs.c, holding public functions and globals.
 * @brief header for ui-prefs.c
 */

GtkWidget *ui_prefs_create_window(void);

#endif				// UI_PREFS_H_
