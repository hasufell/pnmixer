/* ui-prefs-window.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-prefs-window.c
 * This file holds the ui-related code for the preferences window.
 * @brief preferences window ui subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#ifndef WITH_GTK3
#include <gdk/gdkkeysyms.h>
#endif

#include "audio.h"
#include "prefs.h"
#include "hotkey.h"
#include "support.h"
#include "ui-prefs-window.h"
#include "ui-hotkey-dialog.h"

#include "main.h"

#ifdef WITH_GTK3
#define PREFS_UI_FILE "prefs-window-gtk3.glade"
#else
#define PREFS_UI_FILE "prefs-window-gtk2.glade"
#endif

/**
 * Defines the whole preferences entity.
 */
struct prefs_window {
	/* Top-level widgets */
	GtkWidget *prefs_window;
	GtkWidget *notebook;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;
	/* View panel */
	GtkWidget *vol_orientation_combo;
	GtkWidget *vol_text_check;
	GtkWidget *vol_pos_label;
	GtkWidget *vol_pos_combo;
	GtkWidget *vol_meter_draw_check;
	GtkWidget *vol_meter_pos_label;
	GtkWidget *vol_meter_pos_spin;
	GtkAdjustment *vol_meter_pos_adjustment;
	GtkWidget *vol_meter_color_label;
	GtkWidget *vol_meter_color_button;
	GtkWidget *system_theme;
	/* Device panel */
	GtkWidget *card_combo;
	GtkWidget *chan_combo;
	GtkWidget *normalize_vol_check;
	/* Behavior panel */
	GtkWidget *vol_control_entry;
	GtkWidget *scroll_step_spin;
	GtkWidget *fine_scroll_step_spin;
	GtkWidget *middle_click_combo;
	GtkWidget *custom_label;
	GtkWidget *custom_entry;
	/* Hotkeys panel */
	GtkWidget *hotkeys_enable_check;
	GtkWidget *hotkeys_vol_label;
	GtkWidget *hotkeys_vol_spin;
	GtkWidget *hotkeys_mute_eventbox;
	GtkWidget *hotkeys_mute_label;
	GtkWidget *hotkeys_up_eventbox;
	GtkWidget *hotkeys_up_label;
	GtkWidget *hotkeys_down_eventbox;
	GtkWidget *hotkeys_down_label;
	/* Notifications panel */
#ifdef HAVE_LIBN
	GtkWidget *noti_vbox_enabled;
	GtkWidget *noti_enable_check;
	GtkWidget *noti_timeout_label;
	GtkWidget *noti_timeout_spin;
	GtkWidget *noti_hotkey_check;
	GtkWidget *noti_mouse_check;
	GtkWidget *noti_popup_check;
	GtkWidget *noti_ext_check;
#else
	GtkWidget *noti_vbox_disabled;
#endif
};

typedef struct prefs_window PrefsWindow;

static PrefsWindow *instance;

/* Helpers */

/* Gets one of the hotkey code and mode in the Hotkeys settings
 * from the specified label (parsed as an accelerator name).
 */
static void
get_keycode_for_label(GtkLabel *label, gint *code, GdkModifierType *mods)
{
	const gchar *key_accel;

	key_accel = gtk_label_get_text(label);
	hotkey_accel_to_code(key_accel, code, mods);
}

/* Sets one of the hotkey labels in the Hotkeys settings
 * to the specified keycode (converted to a accelerator name).
 */
static void
set_label_for_keycode(GtkLabel *label, gint code, GdkModifierType mods)
{
	gchar *key_accel;

	if (code < 0)
		return;

	key_accel = hotkey_code_to_accel(code, mods);
	gtk_label_set_text(label, key_accel);
	g_free(key_accel);
}

/**
 * Fills the GtkComboBoxText 'combo' with the currently available
 * channels of the card.
 * 
 *
 * @param combo the GtkComboBoxText widget for the channels
 * @param channels list of available channels
 * @param selected the currently selected channel
 */

/**
 * Fills the GtkComboBoxText 'chan_combo' with the currently available channels
 * for a given card.
 * The active channel in the combo box is set to the SELECTED channel found in
 * preferences for this card.
 *
 * @param combo the GtkComboBoxText widget for the channels.
 * @param card_name the card to use to get the channels list.
 */
static void
fill_chan_combo(GtkComboBoxText *combo, const gchar *card_name)
{
	int idx, sidx;
	gchar *selected_channel;
	GSList *channel_list, *item;

	DEBUG("Filling channels ComboBox for card '%s'", card_name);
 
	selected_channel = prefs_get_channel(card_name);
	channel_list = audio_get_channel_list(card_name);

	/* Empty the combo box */
	gtk_combo_box_text_remove_all(combo);

	/* Fill the combo box with the channels, save the selected channel index */
	for (sidx = idx = 0, item = channel_list; item; idx++, item = item->next) {
		const char *channel_name = item->data;
		gtk_combo_box_text_append_text(combo, channel_name);

		if (!g_strcmp0(channel_name, selected_channel))
			sidx = idx;
	}

	/* Set the combo box active item */
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), sidx);

	/* Cleanup */
	g_slist_free_full(channel_list, g_free);
	g_free(selected_channel);
}

/**
 * Fills the GtkComboBoxText 'card_combo' with the currently available cards.
 * The active card in the combo box is set to the currently ACTIVE card,
 * which may be different from the SELECTED card found in preferences.
 *
 * @param combo the GtkComboBoxText widget for the cards.
 */
static void
fill_card_combo(GtkComboBoxText *combo)
{
	int idx, sidx;
	const gchar *active_card;
	GSList *card_list, *item;

	DEBUG("Filling cards ComboBox");

	active_card = audio_get_card();
	card_list = audio_get_card_list();

	/* Empty the combo box */
	gtk_combo_box_text_remove_all(combo);

	/* Fill the combo box with the cards, save the active card index */
	for (sidx = idx = 0, item = card_list; item; idx++, item = item->next) {
		const char *card_name = item->data;
		gtk_combo_box_text_append_text(combo, card_name);

		if (!g_strcmp0(card_name, active_card))
			sidx = idx;
	}

	/* Set the combo box active item */
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), sidx);

	/* Cleanup */
	g_slist_free_full(card_list, g_free);
}

/* Signals handlers */

/**
 * Handles the 'toggled' signal on the GtkCheckButton 'vol_text_check'.
 * Updates the preferences window.
 *
 * @param button the button which received the signal.
 * @param window user data set when the signal handler was connected.
 */
void
on_vol_text_check_toggled(GtkToggleButton *button, PrefsWindow *window)
{
	gboolean active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(window->vol_pos_label, active);
	gtk_widget_set_sensitive(window->vol_pos_combo, active);
}

/**
 * Handles the 'toggled' signal on the GtkCheckButton 'vol_meter_draw_check'.
 * Updates the preferences window.
 *
 * @param button the button which received the signal.
 * @param window user data set when the signal handler was connected.
 */
void
on_vol_meter_draw_check_toggled(GtkToggleButton *button, PrefsWindow *window)
{
	gboolean active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(window->vol_meter_pos_label, active);
	gtk_widget_set_sensitive(window->vol_meter_pos_spin, active);
	gtk_widget_set_sensitive(window->vol_meter_color_label, active);
	gtk_widget_set_sensitive(window->vol_meter_color_button, active);
}

/**
 * Handles the 'changed' signal on the GtkComboBoxText 'card_combo'.
 * This basically refills the channel list if the card changes.
 *
 * @param box the box which received the signal.
 * @param window user data set when the signal handler was connected.
 */
void
on_card_combo_changed(GtkComboBoxText *box, PrefsWindow *window)
{
	gchar *card_name;

	card_name = gtk_combo_box_text_get_active_text(box);
	fill_chan_combo(GTK_COMBO_BOX_TEXT(window->chan_combo), card_name);
	g_free(card_name);
}

/**
 * Handles the 'changed' signal on the GtkComboBoxText 'middle_click_combo'.
 * Updates the preferences window.
 *
 * @param box the combobox which received the signal.
 * @param window user data set when the signal handler was connected.
 */
void
on_middle_click_combo_changed(GtkComboBoxText *box, PrefsWindow *window)
{
	gboolean cust = gtk_combo_box_get_active(GTK_COMBO_BOX(box)) == 3;
	gtk_widget_set_sensitive(window->custom_label, cust);
	gtk_widget_set_sensitive(window->custom_entry, cust);
}

/**
 * Handles the 'toggled' signal on the GtkCheckButton 'hotkeys_enable_check'.
 * Updates the preferences window.
 *
 * @param button the button which received the signal.
 * @param window user data set when the signal handler was connected.
 */
void
on_hotkeys_enable_check_toggled(GtkToggleButton *button, PrefsWindow *window)
{
	gboolean active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(window->hotkeys_vol_label, active);
	gtk_widget_set_sensitive(window->hotkeys_vol_spin, active);
}

/**
 * Handles 'button-press-event' signal on one of the GtkEventBoxes used to
 * define a hotkey: 'hotkeys_mute/up/down_eventbox'.
 * Runs a dialog window where user can define a new hotkey.
 * User should double-click on the event box to define a new hotkey.
 *
 * @param widget the object which received the signal.
 * @param event the GdkEventButton which triggered this signal.
 * @param window user data set when the signal handler was connected.
 * @return TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
gboolean
on_hotkey_event_box_button_press_event(GtkWidget *widget, GdkEventButton *event,
				       PrefsWindow *window)
{
	const gchar *hotkey;
	GtkLabel *hotkey_label;

	gchar *resp;

	/* We want a left-click */
	if (event->button != 1)
		return FALSE;

	/* We want it to be double-click */
	if (event->type == GDK_2BUTTON_PRESS)
		return FALSE;

	/* Let's check which eventbox was clicked */
	hotkey = NULL;
	if (widget == window->hotkeys_mute_eventbox) {
		hotkey_label = GTK_LABEL(window->hotkeys_mute_label);
		hotkey = _("Mute/Unmute");
	} else if (widget == window->hotkeys_up_eventbox) {
		hotkey_label = GTK_LABEL(window->hotkeys_up_label);
		hotkey = _("Volume Up");
	} else if (widget == window->hotkeys_down_eventbox) {
		hotkey_label = GTK_LABEL(window->hotkeys_down_label);
		hotkey = _("Volume Down");
	}
	g_assert(hotkey);

	/* Run the hotkey dialog */
	resp = hotkey_dialog_do(GTK_WINDOW(window->prefs_window), hotkey);

	/* Handle the response */
	if (resp == NULL)
		return FALSE;

	/* <Primary>c is used to disable the hotkey */
	if (!g_ascii_strcasecmp(resp, "<Primary>c")) {
		g_free(resp);
		resp = g_strdup_printf("(%s)", _("None"));
	}

	/* Set */
	gtk_label_set_text(hotkey_label, resp);
	g_free(resp);

	return FALSE;
}

/**
 * Handles the 'toggled' signal on the GtkCheckButton 'noti_enable_check'.
 * Updates the preferences window.
 *
 * @param button the button which received the signal.
 * @param window user data set when the signal handler was connected.
 */
#ifdef HAVE_LIBN
void
on_noti_enable_check_toggled(GtkToggleButton *button, PrefsWindow *window)
{
	gboolean active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(window->noti_timeout_label, active);
	gtk_widget_set_sensitive(window->noti_timeout_spin, active);
	gtk_widget_set_sensitive(window->noti_hotkey_check, active);
	gtk_widget_set_sensitive(window->noti_mouse_check, active);
	gtk_widget_set_sensitive(window->noti_popup_check, active);
	gtk_widget_set_sensitive(window->noti_ext_check, active);
}
#else
void
on_noti_enable_check_toggled(G_GNUC_UNUSED GtkToggleButton *button,
			     G_GNUC_UNUSED PrefsWindow *window)
{
}
#endif


/**
 * Callback function when the ok_button (GtkButton) of the
 * preferences window received the clicked signal.
 *
 * @param button the object that received the signal
 * @param window struct holding the GtkWidgets of the preferences windows
 */
static void
retrieve_window_values(PrefsWindow *window)
{
	DEBUG("Retrieving prefs window values");

	// volume slider orientation
	GtkWidget *soc = window->vol_orientation_combo;
	const gchar *orientation;
#ifdef WITH_GTK3
	orientation = gtk_combo_box_get_active_id(GTK_COMBO_BOX(soc));
#else
	/* Gtk2 ComboBoxes don't have item ids */
	orientation = "vertical";
	if (gtk_combo_box_get_active(GTK_COMBO_BOX(soc)) == 1)
		orientation = "horizontal";
#endif
	prefs_set_string("SliderOrientation", orientation);

	// volume text display
	GtkWidget *vtc = window->vol_text_check;
	gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vtc));
	prefs_set_boolean("DisplayTextVolume", active);

	// volume text position
	GtkWidget *vpc = window->vol_pos_combo;
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(vpc));
	prefs_set_integer("TextVolumePosition", idx);

	// volume meter display
	GtkWidget *dvc = window->vol_meter_draw_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dvc));
	prefs_set_boolean("DrawVolMeter", active);

	// volume meter positon
	GtkWidget *vmps = window->vol_meter_pos_spin;
	gint vmpos = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vmps));
	prefs_set_integer("VolMeterPos", vmpos);

	// volume meter colors
	GtkWidget *vcb = window->vol_meter_color_button;
	gdouble colors[3];
#ifdef WITH_GTK3
	GdkRGBA color;
	gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(vcb), &color);
	colors[0] = color.red;
	colors[1] = color.green;
	colors[2] = color.blue;
#else
	GdkColor color;
	gtk_color_button_get_color(GTK_COLOR_BUTTON(vcb), &color);
	colors[0] = (gdouble) color.red / 65536;
	colors[1] = (gdouble) color.green / 65536;
	colors[2] = (gdouble) color.blue / 65536;
#endif
	prefs_set_double_list("VolMeterColor", colors, 3);

	// icon theme
	GtkWidget *system_theme = window->system_theme;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(system_theme));
	prefs_set_boolean("SystemTheme", active);

	// alsa card
	GtkWidget *acc = window->card_combo;
	gchar *card = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(acc));
	prefs_set_string("AlsaCard", card);

	// alsa channel
	GtkWidget *ccc = window->chan_combo;
	gchar *chan = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ccc));
	prefs_set_channel(card, chan);
	g_free(card);
	g_free(chan);

	// normalize volume
	GtkWidget *vnorm = window->normalize_vol_check;
	gboolean is_pressed;
	is_pressed = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vnorm));
	prefs_set_boolean("NormalizeVolume", is_pressed);

	// volume control command
	GtkWidget *ve = window->vol_control_entry;
	const gchar *vc = gtk_entry_get_text(GTK_ENTRY(ve));
	prefs_set_string("VolumeControlCommand", vc);

	// volume scroll steps
	GtkWidget *sss = window->scroll_step_spin;
	gdouble step = gtk_spin_button_get_value(GTK_SPIN_BUTTON(sss));
	prefs_set_double("ScrollStep", step);

	GtkWidget *fsss = window->fine_scroll_step_spin;
	gdouble fine_step = gtk_spin_button_get_value(GTK_SPIN_BUTTON(fsss));
	prefs_set_double("FineScrollStep", fine_step);

	// middle click
	GtkWidget *mcc = window->middle_click_combo;
	idx = gtk_combo_box_get_active(GTK_COMBO_BOX(mcc));
	prefs_set_integer("MiddleClickAction", idx);

	// custom command
	GtkWidget *ce = window->custom_entry;
	const gchar *cc = gtk_entry_get_text(GTK_ENTRY(ce));
	prefs_set_string("CustomCommand", cc);

	// hotkeys enabled
	GtkWidget *hkc = window->hotkeys_enable_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hkc));
	prefs_set_boolean("EnableHotKeys", active);

	// hotkeys scroll step
	GtkWidget *hs = window->hotkeys_vol_spin;
	gint hotstep = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(hs));
	prefs_set_integer("HotkeyVolumeStep", hotstep);

	// hotkeys
	GtkWidget *kl;
	gint keycode;
	GdkModifierType mods;

	kl = window->hotkeys_mute_label;
	get_keycode_for_label(GTK_LABEL(kl), &keycode, &mods);
	prefs_set_integer("VolMuteKey", keycode);
	prefs_set_integer("VolMuteMods", mods);

	kl = window->hotkeys_up_label;
	get_keycode_for_label(GTK_LABEL(kl), &keycode, &mods);
	prefs_set_integer("VolUpKey", keycode);
	prefs_set_integer("VolUpMods", mods);

	kl = window->hotkeys_down_label;
	get_keycode_for_label(GTK_LABEL(kl), &keycode, &mods);
	prefs_set_integer("VolDownKey", keycode);
	prefs_set_integer("VolDownMods", mods);

	// notifications
#ifdef HAVE_LIBN
	GtkWidget *nc = window->noti_enable_check;
	gint noti_spin;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(nc));
	prefs_set_boolean("EnableNotifications", active);

	nc = window->noti_hotkey_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(nc));
	prefs_set_boolean("HotkeyNotifications", active);

	nc = window->noti_mouse_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(nc));
	prefs_set_boolean("MouseNotifications", active);

	nc = window->noti_popup_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(nc));
	prefs_set_boolean("PopupNotifications", active);

	nc = window->noti_ext_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(nc));
	prefs_set_boolean("ExternalNotifications", active);

	nc = window->noti_timeout_spin;
	noti_spin = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(nc));
	prefs_set_integer("NotificationTimeout", noti_spin);
#endif
}

static void
populate_window_values(PrefsWindow *window)
{
	gdouble *vol_meter_clrs;
	gchar *slider_orientation, *vol_cmd, *custcmd;

	DEBUG("Populating prefs window values");

	// volume slider orientation
	slider_orientation = prefs_get_string("SliderOrientation", NULL);
	if (slider_orientation) {
		GtkComboBox *combo_box = GTK_COMBO_BOX
					 (window->vol_orientation_combo);
#ifndef WITH_GTK3
		/* Gtk2 ComboBoxes don't have item ids */
		if (!strcmp(slider_orientation, "horizontal"))
			gtk_combo_box_set_active(combo_box, 1);
		else
			gtk_combo_box_set_active(combo_box, 0);
#else
		gtk_combo_box_set_active_id(combo_box, slider_orientation);
#endif
		g_free(slider_orientation);
	}

	// volume text display
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->vol_text_check),
	 prefs_get_boolean("DisplayTextVolume", FALSE));

	on_vol_text_check_toggled(GTK_TOGGLE_BUTTON(window->vol_text_check), window);

	// volume text position
	gtk_combo_box_set_active
	(GTK_COMBO_BOX(window->vol_pos_combo),
	 prefs_get_integer("TextVolumePosition", 0));

	// volume meter display
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->vol_meter_draw_check),
	 prefs_get_boolean("DrawVolMeter", FALSE));

	on_vol_meter_draw_check_toggled(GTK_TOGGLE_BUTTON(window->vol_meter_draw_check),
					window);

	// volume meter position
	gtk_spin_button_set_value
	(GTK_SPIN_BUTTON(window->vol_meter_pos_spin),
	 prefs_get_integer("VolMeterPos", 0));

	// volume meter colors
	vol_meter_clrs = prefs_get_double_list("VolMeterColor", NULL);
#ifdef WITH_GTK3
	GdkRGBA vol_meter_color_button_color;
	vol_meter_color_button_color.red = vol_meter_clrs[0];
	vol_meter_color_button_color.green = vol_meter_clrs[1];
	vol_meter_color_button_color.blue = vol_meter_clrs[2];
	vol_meter_color_button_color.alpha = 1.0;
	gtk_color_chooser_set_rgba
	(GTK_COLOR_CHOOSER(window->vol_meter_color_button),
	 &vol_meter_color_button_color);
#else
	GdkColor vol_meter_color_button_color;
	vol_meter_color_button_color.red = (guint32) (vol_meter_clrs[0] * 65536);
	vol_meter_color_button_color.green = (guint32) (vol_meter_clrs[1] * 65536);
	vol_meter_color_button_color.blue = (guint32) (vol_meter_clrs[2] * 65536);
	gtk_color_button_set_color
	(GTK_COLOR_BUTTON(window->vol_meter_color_button),
	 &vol_meter_color_button_color);
#endif
	g_free(vol_meter_clrs);

	// icon theme
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->system_theme),
	 prefs_get_boolean("SystemTheme", FALSE));

	// fill in card & channel combo boxes
	fill_card_combo(GTK_COMBO_BOX_TEXT(window->card_combo));
	fill_chan_combo(GTK_COMBO_BOX_TEXT(window->chan_combo), audio_get_card());

	// normalize volume
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->normalize_vol_check),
	 prefs_get_boolean("NormalizeVolume", FALSE));

	// volume control command
	vol_cmd = prefs_get_vol_command();
	if (vol_cmd) {
		gtk_entry_set_text(GTK_ENTRY(window->vol_control_entry), vol_cmd);
		g_free(vol_cmd);
	}

	// volume scroll steps
	gtk_spin_button_set_value
	(GTK_SPIN_BUTTON(window->scroll_step_spin),
	 prefs_get_double("ScrollStep", 5));

	gtk_spin_button_set_value
	(GTK_SPIN_BUTTON(window->fine_scroll_step_spin),
	 prefs_get_double("FineScrollStep", 1));

	//  middle click
	gtk_combo_box_set_active
	(GTK_COMBO_BOX(window->middle_click_combo),
	 prefs_get_integer("MiddleClickAction", 0));

	on_middle_click_combo_changed(GTK_COMBO_BOX_TEXT(window->middle_click_combo), window);

	// custom command
	gtk_entry_set_invisible_char(GTK_ENTRY(window->custom_entry), 8226);

	custcmd = prefs_get_string("CustomCommand", NULL);
	if (custcmd) {
		gtk_entry_set_text(GTK_ENTRY(window->custom_entry), custcmd);
		g_free(custcmd);
	}

	// hotkeys enabled
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->hotkeys_enable_check),
	 prefs_get_boolean("EnableHotKeys", FALSE));

	// hotkeys scroll step
	gtk_spin_button_set_value
	(GTK_SPIN_BUTTON(window->hotkeys_vol_spin),
	 prefs_get_integer("HotkeyVolumeStep", 1));

	// hotkeys
	set_label_for_keycode(GTK_LABEL(window->hotkeys_mute_label),
			      prefs_get_integer("VolMuteKey", -1),
			      prefs_get_integer("VolMuteMods", 0));

	set_label_for_keycode(GTK_LABEL(window->hotkeys_up_label),
			      prefs_get_integer("VolUpKey", -1),
			      prefs_get_integer("VolUpMods", 0));

	set_label_for_keycode(GTK_LABEL(window->hotkeys_down_label),
			      prefs_get_integer("VolDownKey", -1),
			      prefs_get_integer("VolDownMods", 0));

	on_hotkeys_enable_check_toggled(GTK_TOGGLE_BUTTON(window->hotkeys_enable_check),
					window);

	// notifications
#ifdef HAVE_LIBN
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->noti_enable_check),
	 prefs_get_boolean("EnableNotifications", FALSE));

	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->noti_hotkey_check),
	 prefs_get_boolean("HotkeyNotifications", TRUE));

	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->noti_mouse_check),
	 prefs_get_boolean("MouseNotifications", TRUE));

	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->noti_popup_check),
	 prefs_get_boolean("PopupNotifications", FALSE));

	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->noti_ext_check),
	 prefs_get_boolean("ExternalNotifications", FALSE));

	gtk_spin_button_set_value
	(GTK_SPIN_BUTTON(window->noti_timeout_spin),
	 prefs_get_integer("NotificationTimeout", 1500));

	on_noti_enable_check_toggled(GTK_TOGGLE_BUTTON(window->noti_enable_check), window);
#endif
}

/* Private functions */

static void
prefs_window_show(PrefsWindow *window)
{
	gtk_widget_show(window->prefs_window);
}

static void
prefs_window_destroy(PrefsWindow *window)
{
	gtk_widget_destroy(window->prefs_window);
	g_free(window);
}

static PrefsWindow *
prefs_window_create(void)
{
	gchar *uifile = NULL;
	GtkBuilder *builder = NULL;
	PrefsWindow *window;

	window = g_new0(PrefsWindow, 1);

	/* Build UI file */
	uifile = get_ui_file(PREFS_UI_FILE);
	g_assert(uifile);

	DEBUG("Building prefs window from ui file '%s'", uifile);
	builder = gtk_builder_new_from_file(uifile);

	/* Append the notification page.
	 * This has to be done manually here, in the C code,
	 * because notifications support is optional at build time.
	 */
	gtk_notebook_append_page
	(GTK_NOTEBOOK(gtk_builder_get_object(builder, "notebook")),
#ifdef HAVE_LIBN
	 GTK_WIDGET(gtk_builder_get_object(builder, "noti_vbox_enabled")),
#else
	 GTK_WIDGET(gtk_builder_get_object(builder, "noti_vbox_disabled")),
#endif
	 gtk_label_new(_("Notifications")));

	/* Save some widgets for later use */
	// Top level widgets
	assign_gtk_widget(builder, window, prefs_window);
	assign_gtk_widget(builder, window, notebook);
	assign_gtk_widget(builder, window, ok_button);
	assign_gtk_widget(builder, window, cancel_button);
	// View panel
	assign_gtk_widget(builder, window, vol_orientation_combo);
	assign_gtk_widget(builder, window, vol_text_check);
	assign_gtk_widget(builder, window, vol_pos_label);
	assign_gtk_widget(builder, window, vol_pos_combo);
	assign_gtk_widget(builder, window, vol_meter_draw_check);
	assign_gtk_widget(builder, window, vol_meter_pos_label);
	assign_gtk_widget(builder, window, vol_meter_pos_spin);
	assign_gtk_adjustment(builder, window, vol_meter_pos_adjustment);
	assign_gtk_widget(builder, window, vol_meter_color_label);
	assign_gtk_widget(builder, window, vol_meter_color_button);
	assign_gtk_widget(builder, window, system_theme);
	// Device panel
	assign_gtk_widget(builder, window, card_combo);
	assign_gtk_widget(builder, window, chan_combo);
	assign_gtk_widget(builder, window, normalize_vol_check);
	// Behavior panel
	assign_gtk_widget(builder, window, vol_control_entry);
	assign_gtk_widget(builder, window, scroll_step_spin);
	assign_gtk_widget(builder, window, fine_scroll_step_spin);
	assign_gtk_widget(builder, window, middle_click_combo);
	assign_gtk_widget(builder, window, custom_label);
	assign_gtk_widget(builder, window, custom_entry);
	// Hotkeys panel
	assign_gtk_widget(builder, window, hotkeys_enable_check);
	assign_gtk_widget(builder, window, hotkeys_vol_label);
	assign_gtk_widget(builder, window, hotkeys_vol_spin);
	assign_gtk_widget(builder, window, hotkeys_mute_eventbox);
	assign_gtk_widget(builder, window, hotkeys_mute_label);
	assign_gtk_widget(builder, window, hotkeys_up_eventbox);
	assign_gtk_widget(builder, window, hotkeys_up_label);
	assign_gtk_widget(builder, window, hotkeys_down_eventbox);
	assign_gtk_widget(builder, window, hotkeys_down_label);
	// Notifications panel
#ifdef HAVE_LIBN
	assign_gtk_widget(builder, window, noti_vbox_enabled);
	assign_gtk_widget(builder, window, noti_enable_check);
	assign_gtk_widget(builder, window, noti_timeout_spin);
	assign_gtk_widget(builder, window, noti_timeout_label);
	assign_gtk_widget(builder, window, noti_hotkey_check);
	assign_gtk_widget(builder, window, noti_mouse_check);
	assign_gtk_widget(builder, window, noti_popup_check);
	assign_gtk_widget(builder, window, noti_ext_check);
#else
	assign_gtk_widget(builder, window, noti_vbox_disabled);
#endif

	/* Configure some widgets */
	populate_window_values(window);

	/* Connect signals */
	gtk_builder_connect_signals(builder, window);

	/* Cleanup */
	g_object_unref(G_OBJECT(builder));
	g_free(uifile);

	return window;
}

/* Ok & Cancel signal handlers */

/**
 * Handles the 'clicked' signal on the GtkButton 'ok_button',
 * therefore closing the preferences window, applying and saving changes.
 *
 * @param button the GtkButton that received the signal.
 * @param window user data set when the signal handler was connected.
 */
void
on_ok_button_clicked(G_GNUC_UNUSED GtkButton *button, PrefsWindow *window)
{
	g_assert(window == instance);

	/* Save values to preferences */
	retrieve_window_values(window);

	/* Save preferences to file */
	prefs_save();

	/* Make it effective */
	apply_prefs();

	/* Destroy the window */
	prefs_window_destroy(instance);
	instance = NULL;
}

/**
 * Handles the 'clicked' signal on the GtkButton 'cancel_button',
 * therefore closing the preferences window and discarding any changes.
 *
 * @param button the GtkButton that received the signal.
 * @param window user data set when the signal handler was connected.
 */
void
on_cancel_button_clicked(G_GNUC_UNUSED GtkButton *button, PrefsWindow *window)
{
	g_assert(window == instance);

	prefs_window_destroy(window);
	instance = NULL;
}

/**
 * Handles the 'key-press-event' signal on the GtkWindow 'prefs_window',
 * currently handles Esc key (aka Cancel) and Return key (aka OK).
 *
 * @param widget the widget that received the signal.
 * @param event the key event that was triggered.
 * @param window user data set when the signal handler was connected.
 * @return TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
gboolean
on_prefs_window_key_press_event(G_GNUC_UNUSED GtkWidget *widget,
				GdkEventKey *event, PrefsWindow *window)
{
	switch (event->keyval) {
	case GDK_KEY_Escape:
		gtk_button_clicked(GTK_BUTTON(window->cancel_button));
		break;
	case GDK_KEY_Return:
		gtk_button_clicked(GTK_BUTTON(window->ok_button));
		break;
	default:
		break;
	}

	return FALSE;
}

/* Public functions */

/**
 * Creates the preferences window and display it.
 */
void
prefs_window_open(void)
{
	/* Only one instance at a time */
	if (instance)
		return;

	instance = prefs_window_create();
	prefs_window_show(instance);
}

