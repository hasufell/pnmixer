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
#include <X11/XKBlib.h>

#include "ui-prefs-window.h"
#include "prefs.h"
#include "alsa.h"
#include "debug.h"

#include "main.h"

#ifdef WITH_GTK3
#define PREFS_UI_FILE "prefs-gtk3.glade"
#else
#define PREFS_UI_FILE "prefs-gtk2.glade"
#endif

/**
 * Defines the whole preferences entity.
 */
struct prefs_window {
	/* Top-level widgets */
	GtkWidget *window;
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
	GObject   *vol_meter_pos_adjustment;
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
	GtkWidget *hotkeys_dialog;
	GtkWidget *hotkeys_dialog_key_label;
	GtkWidget *hotkeys_mute_label;
	GtkWidget *hotkeys_up_label;
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

/*
 * Helpers
 */

/**
 * Fills the GtkComboBoxText chan_combo with the currently available
 * channels of the card.
 *
 * @param channels list of available channels
 * @param combo the GtkComboBoxText widget for the channels
 * @param selected the currently selected channel
 */
static void
fill_channel_combo(GSList *channels, GtkWidget *combo, gchar *selected)
{
	int idx = 0, sidx = 0;
	GtkTreeIter iter;
	GtkListStore *store =
		GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combo)));
	gtk_list_store_clear(store);
	while (channels) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, channels->data, -1);
		if (selected && !strcmp(channels->data, selected))
			sidx = idx;
		idx++;
		channels = channels->next;
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), sidx);
}

/**
 * Fills the GtkComboBoxText card_combo with the currently available
 * cards and calls fill_channel_combo() as well.
 *
 * @param combo the GtkComboBoxText widget for the alsa cards
 * @param channels_combo the GtkComboBoxText widget for the card channels
 */
static void
fill_card_combo(GtkWidget *combo, GtkWidget *channels_combo)
{
	struct acard *c;
	GSList *cur_card;
	struct acard *active_card;
	int idx, sidx = 0;

	GtkTreeIter iter;
	GtkListStore *store =
		GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combo)));

	cur_card = cards;
	active_card = alsa_get_active_card();
	idx = 0;
	while (cur_card) {
		c = cur_card->data;
		if (!c->channels) {
			cur_card = cur_card->next;
			continue;
		}
		if (active_card && !strcmp(c->name, active_card->name)) {
			gchar *sel_chan = prefs_get_channel(c->name);
			sidx = idx;
			fill_channel_combo(c->channels, channels_combo, sel_chan);
			if (sel_chan)
				g_free(sel_chan);
		}
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, c->name, -1);
		cur_card = cur_card->next;
		idx++;
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), sidx);
}

/*
 * Widgets signals handlers.
 * Link between signal name and handler is declared in the ui file.
 * These functions must not be static.
 */

/**
 * Handler for the signal 'toggled' on the GtkCheckButton vol_text_check.
 * Updates the preferences window.
 *
 * @param button the button which received the signal
 * @param window user data set when the signal handler was connected
 */
void
on_vol_text_toggled(GtkToggleButton *button, PrefsWindow *window)
{
	gboolean active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(window->vol_pos_label, active);
	gtk_widget_set_sensitive(window->vol_pos_combo, active);
}

/**
 * Handler for the signal 'toggled' on the GtkCheckButton vol_meter_draw_check.
 * Updates the preferences window.
 *
 * @param button the button which received the signal
 * @param window user data set when the signal handler was connected
 */
void
on_vol_meter_draw_toggled(GtkToggleButton *button, PrefsWindow *window)
{
	gboolean active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(window->vol_meter_pos_label, active);
	gtk_widget_set_sensitive(window->vol_meter_pos_spin, active);
	gtk_widget_set_sensitive(window->vol_meter_color_label, active);
	gtk_widget_set_sensitive(window->vol_meter_color_button, active);
}

/**
 * Handler for the signal 'changed' on the GtkComboBoxText card_combo.
 * This basically refills the channel list if the card changes.
 *
 * @param box the box which received the signal
 * @param window user data set when the signal handler was connected
 */
void
on_card_changed(GtkComboBoxText *box, PrefsWindow *window)
{
	struct acard *card;
	gchar *card_name;

	card_name = gtk_combo_box_text_get_active_text(box);
	card = find_card(card_name);
	g_free(card_name);

	if (card) {
		gchar *sel_chan = prefs_get_channel(card->name);
		fill_channel_combo(card->channels, window->chan_combo, sel_chan);
		g_free(sel_chan);
	}
}

/**
 * Handler for the signal 'changed' on the GtkComboBox middle_click_combo.
 * Updates the preferences window.
 *
 * @param box the combobox which received the signal
 * @param window user data set when the signal handler was connected
 */
void
on_middle_click_changed(GtkComboBox *box, PrefsWindow *window)
{
	gboolean cust = gtk_combo_box_get_active(GTK_COMBO_BOX(box)) == 3;
	gtk_widget_set_sensitive(window->custom_label, cust);
	gtk_widget_set_sensitive(window->custom_entry, cust);
}

/**
 * Handler for the signal 'toggled' on the GtkCheckButton hotkeys_enable_check.
 * Updates the preferences window.
 *
 * @param button the button which received the signal
 * @param window user data set when the signal handler was connected
 */
void
on_hotkeys_enabled_toggled(GtkToggleButton *button, PrefsWindow *window)
{
	gboolean active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(window->hotkeys_vol_label, active);
	gtk_widget_set_sensitive(window->hotkeys_vol_spin, active);
}

/**
 * Handler for the signal 'key-press-event' on the GtkDialog hotkeys_dialog
 * which was opened by acquire_hotkey().
 *
 * @param dialog the dialog window which received the signal
 * @param ev the GdkEventKey which triggered the signal
 * @param window struct holding the GtkWidgets of the preferences windows
 * @return TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
gboolean
on_hotkey_pressed(G_GNUC_UNUSED GtkWidget *dialog,
                  GdkEventKey *ev,
                  PrefsWindow *window)
{
	gchar *key_text;
	guint keyval;
	GdkModifierType state, consumed;
	GtkLabel *key_label = GTK_LABEL(window->hotkeys_dialog_key_label);

	state = ev->state;
	gdk_keymap_translate_keyboard_state(gdk_keymap_get_default(),
					    ev->hardware_keycode,
					    state, ev->group, &keyval, NULL, NULL, &consumed);

	state &= ~consumed;
	state &= gtk_accelerator_get_default_mod_mask();

	key_text = gtk_accelerator_name(keyval, state);
	gtk_label_set_text(key_label, key_text);
	g_free(key_text);

	return FALSE;
}

/**
 * Handler for the signal 'key-release-event' on the GtkDialog hotkeys_dialog
 * which was opened by acquire_hotkey().
 *
 * @param dialog the dialog window which received the signal
 * @param ev the GdkEventKey which triggered the signal
 * @param window struct holding the GtkWidgets of the preferences windows
 * @return TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
gboolean
on_hotkey_released(GtkWidget *dialog,
                   G_GNUC_UNUSED GdkEventKey *ev,
                   G_GNUC_UNUSED PrefsWindow *window)
{
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	return FALSE;
}

/**
 * This is called from within the callback function
 * on_hotkey_event_box_button_pressed() in callbacks. which is triggered when
 * one of the hotkey boxes hotkeys_mute_eventbox, hotkeys_up_eventbox or
 * hotkeys_down_eventbox (GtkEventBox) in the preferences received
 * the button-press-event signal.
 *
 * Then this function grabs the keyboard, opens the hotkeys_dialog
 * and updates the GtkLabel with the pressed hotkey.
 * The GtkLabel is later read by on_ok_button_clicked() in
 * callbacks.c which stores the result in the global keyFile.
 *
 * @param widget_name the name of the widget (hotkeys_mute_eventbox, hotkeys_up_eventbox
 * or hotkeys_down_eventbox)
 * @param window struct holding the GtkWidgets of the preferences windows
 */
static void
acquire_hotkey(const char *widget_name, PrefsWindow *window)
{
	gint resp, action;
	GtkWidget *diag = window->hotkeys_dialog;
	GtkLabel *dialog_key_label = GTK_LABEL(window->hotkeys_dialog_key_label);
	GdkGrabStatus grab_status;

	action =
		(!strcmp(widget_name, "hotkeys_mute_eventbox")) ? 0 :
		(!strcmp(widget_name, "hotkeys_up_eventbox")) ? 1 :
		(!strcmp(widget_name, "hotkeys_down_eventbox")) ? 2 : -1;

	g_assert(action >= 0);

	/* Set the right key label for the dialog window */
	switch (action) {
	case 0:
		gtk_label_set_text(dialog_key_label, _("Mute/Unmute"));
		break;
	case 1:
		gtk_label_set_text(dialog_key_label, _("Volume Up"));
		break;
	case 2:
		gtk_label_set_text(dialog_key_label, _("Volume Down"));
		break;
	default:
		break;
	}

	/* Grab keyboard */
	grab_status =
#ifdef WITH_GTK3
		gdk_device_grab(gtk_get_current_event_device(),
		                gdk_screen_get_root_window(gdk_screen_get_default()),
		                GDK_OWNERSHIP_APPLICATION,
		                TRUE, GDK_ALL_EVENTS_MASK, NULL, GDK_CURRENT_TIME);
#else
		gdk_keyboard_grab(gtk_widget_get_root_window(GTK_WIDGET(diag)),
		                  TRUE, GDK_CURRENT_TIME);
#endif

	if (grab_status != GDK_GRAB_SUCCESS) {
		report_error(_("Could not grab the keyboard."));
		return;
	}

	/* Run the dialog window */
	resp = gtk_dialog_run(GTK_DIALOG(diag));

	/* Ungrab keyboard */
#ifdef WITH_GTK3
	gdk_device_ungrab(gtk_get_current_event_device(), GDK_CURRENT_TIME);
#else
	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
#endif

	/* Handle the response from the dialog window */
	if (resp == GTK_RESPONSE_OK) {
		const gchar *key_name;
		GtkLabel *key_label;

		key_name = gtk_label_get_text(dialog_key_label);

		if (!strcasecmp(key_name, "<Primary>c"))
			key_name = "(None)";
		
		switch (action) {
		case 0:
			key_label = GTK_LABEL(window->hotkeys_mute_label);
			break;
		case 1:
			key_label = GTK_LABEL(window->hotkeys_up_label);
			break;
		case 2:
			key_label = GTK_LABEL(window->hotkeys_down_label);
			break;
		default:
			break;
		}

		gtk_label_set_text(key_label, key_name);
	}

	/* Hide window at last */
	gtk_widget_hide(diag);
}

/**
 * Callback function when one of the hotkey event boxes hotkeys_mute_eventbox,
 * hotkeys_up_eventbox or hotkeys_down_eventbox (GtkEventBox) in the preferences
 * received the button-press-event signal.
 *
 * @param widget the object which received the signal
 * @param event the GdkEventButton which triggered this signal
 * @param window struct holding the GtkWidgets of the preferences windows
 * @return TRUE to stop other handlers from being invoked for the event.
 * False to propagate the event further
 */
gboolean
on_hotkey_event_box_button_pressed(GtkWidget *widget, GdkEventButton *event,
                                   PrefsWindow *window)
{
	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
		acquire_hotkey(gtk_buildable_get_name(GTK_BUILDABLE(widget)), window);
	return TRUE;
}

/**
 * Handler for the signal 'toggled' on the GtkCheckButton noti_enable_check.
 * Updates the preferences window.
 *
 * @param button the button which received the signal
 * @param window user data set when the signal handler was connected
 */
#ifdef HAVE_LIBN
void
on_noti_enable_toggled(GtkToggleButton *button, PrefsWindow *window)
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
on_noti_enable_toggled(G_GNUC_UNUSED GtkToggleButton *button,
                       G_GNUC_UNUSED PrefsWindow *window)
{
}
#endif

/**
 * Gets one of the hotkey code and mode in the Hotkeys settings
 * from the specified label (parsed as an accelerator name).
 *
 * @param label the label to parse
 * @param code the resulting keycode
 * @param mods the resulting pressed keymode
 */
static void
get_keycode_for_label(GtkLabel *label, gint *code, GdkModifierType *mods)
{
	guint keysym;
	const gchar *key_text;

	key_text = gtk_label_get_text(label);
	gtk_accelerator_parse(key_text, &keysym, mods);
	if (keysym != 0)
		*code = XKeysymToKeycode(gdk_x11_get_default_xdisplay(), keysym);
	else
		*code = -1;
}

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
	prefs_set_vol_meter_colors(colors, 3);

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

/**
 * Sets one of the hotkey labels in the Hotkeys settings
 * to the specified keycode (converted to a accelerator name).
 *
 * @param label the label to set
 * @param code the keycode to convert to accelerator name
 * @param mods the pressed keymod
 */
static void
set_label_for_keycode(GtkLabel *label, gint code, GdkModifierType mods)
{
	int keysym;
	gchar *key_text;

	if (code < 0)
		return;

	keysym = XkbKeycodeToKeysym(gdk_x11_get_default_xdisplay(), code, 0, 0);
	key_text = gtk_accelerator_name(keysym, mods);
	gtk_label_set_text(GTK_LABEL(label), key_text);
	g_free(key_text);
}

static void
populate_window(PrefsWindow *window)
{
	gdouble *vol_meter_clrs;
	gchar *slider_orientation, *vol_cmd, *custcmd;

	DEBUG_PRINT("Populating prefs window");

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

	on_vol_text_toggled(GTK_TOGGLE_BUTTON(window->vol_text_check), window);

	// volume text position
	gtk_combo_box_set_active
	(GTK_COMBO_BOX(window->vol_pos_combo),
	 prefs_get_integer("TextVolumePosition", 0));

	// volume meter display
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->vol_meter_draw_check),
	 prefs_get_boolean("DrawVolMeter", FALSE));

	on_vol_meter_draw_toggled(GTK_TOGGLE_BUTTON(window->vol_meter_draw_check),
	                          window);

	// volume meter position
	gtk_adjustment_set_upper
	(GTK_ADJUSTMENT(window->vol_meter_pos_adjustment),
	 get_tray_icon_size() - 10);

	gtk_spin_button_set_value
	(GTK_SPIN_BUTTON(window->vol_meter_pos_spin),
	 prefs_get_integer("VolMeterPos", 0));

	// volume meter colors
	vol_meter_clrs = prefs_get_vol_meter_colors();
#ifdef WITH_GTK3
	GdkRGBA vol_meter_color_button_color;

	vol_meter_color_button_color.red = vol_meter_clrs[0];
	vol_meter_color_button_color.green = vol_meter_clrs[1];
	vol_meter_color_button_color.blue = vol_meter_clrs[2];
	vol_meter_color_button_color.alpha = 1.0;

	gtk_color_chooser_set_rgba(
	GTK_COLOR_CHOOSER(window->vol_meter_color_button),
	&vol_meter_color_button_color);
#else
	GdkColor vol_meter_color_button_color;

	vol_meter_color_button_color.red = (guint32) (vol_meter_clrs[0] * 65536);
	vol_meter_color_button_color.green = (guint32) (vol_meter_clrs[1] * 65536);
	vol_meter_color_button_color.blue = (guint32) (vol_meter_clrs[2] * 65536);

	gtk_color_button_set_color(
	GTK_COLOR_BUTTON(window->vol_meter_color_button),
	&vol_meter_color_button_color);
#endif
	g_free(vol_meter_clrs);

	// icon theme
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(window->system_theme),
	 prefs_get_boolean("SystemTheme", FALSE));

	// fill in card/channel combo boxes
	fill_card_combo(window->card_combo, window->chan_combo);

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

	on_middle_click_changed(GTK_COMBO_BOX(window->middle_click_combo), window);

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

	on_hotkeys_enabled_toggled(GTK_TOGGLE_BUTTON(window->hotkeys_enable_check),
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

	on_noti_enable_toggled(GTK_TOGGLE_BUTTON(window->noti_enable_check), window);
#endif
}

static PrefsWindow *
build_window(void)
{
	gchar *uifile = NULL;
	GtkBuilder *builder = NULL;
	PrefsWindow *window = NULL;

	/* Get the path to the ui file */
	uifile = get_ui_file(PREFS_UI_FILE);
	g_assert(uifile);

	/* Build the preferences window from ui file */
	DEBUG_PRINT("Building prefs window from ui file '%s'", uifile);
	builder = gtk_builder_new_from_file(uifile);

	/* Append the notification page */
	gtk_notebook_append_page
	(GTK_NOTEBOOK(gtk_builder_get_object(builder, "notebook")),
#ifdef HAVE_LIBN
	 GTK_WIDGET(gtk_builder_get_object(builder, "noti_vbox_enabled")),
#else
	 GTK_WIDGET(gtk_builder_get_object(builder, "noti_vbox_disabled")),
#endif
	 gtk_label_new(_("Notifications")));

	/* Save pointers toward all the useful elements from the UI.
	 * Notice that gtk_builder_get_object() doesn't increment the
	 * reference count of the returned object, neither do we.
	 * So all those pointers are invalid after window destruction.
	 */
	window = g_slice_new(PrefsWindow);

#define GO(name) window->name = G_OBJECT(gtk_builder_get_object(builder, #name))
#define GW(name) window->name = GTK_WIDGET(gtk_builder_get_object(builder, #name))
	/* Top-level widgets */
	GW(window);
	GW(notebook);
	GW(ok_button);
	GW(cancel_button);
	/* View panel */
	GW(vol_orientation_combo);
	GW(vol_text_check);
	GW(vol_pos_label);
	GW(vol_pos_combo);
	GW(vol_meter_draw_check);
	GW(vol_meter_pos_label);
	GW(vol_meter_pos_spin);
	GO(vol_meter_pos_adjustment);
	GW(vol_meter_color_label);
	GW(vol_meter_color_button);
	GW(system_theme);
	/* Device panel */
	GW(card_combo);
	GW(chan_combo);
	GW(normalize_vol_check);
	/* Behavior panel */
	GW(vol_control_entry);
	GW(scroll_step_spin);
	GW(fine_scroll_step_spin);
	GW(middle_click_combo);
	GW(custom_label);
	GW(custom_entry);
	/* Hotkeys panel */
	GW(hotkeys_enable_check);
	GW(hotkeys_vol_label);
	GW(hotkeys_vol_spin);
	GW(hotkeys_dialog);
	GW(hotkeys_dialog_key_label);
	GW(hotkeys_mute_label);
	GW(hotkeys_up_label);
	GW(hotkeys_down_label);
	/* Notifications panel */
#ifdef HAVE_LIBN
	GW(noti_vbox_enabled);
	GW(noti_enable_check);
	GW(noti_timeout_spin);
	GW(noti_timeout_label);
	GW(noti_hotkey_check);
	GW(noti_mouse_check);
	GW(noti_popup_check);
	GW(noti_ext_check);
#else
	GW(noti_vbox_disabled);
#endif
#undef GO
#undef GW

	/* Connect signals */
	gtk_builder_connect_signals(builder, window);

	/* Cleanup */

	/* Notice that the builder drops the references of the objects
	 * it created when it is finalized. The finalization can cause
	 * unused objects to be destroyed.
	 */
	g_object_unref(G_OBJECT(builder));
	g_free(uifile);

	return window;
}

static void
destroy_window(PrefsWindow *window)
{
	g_slice_free(PrefsWindow, window);
}

/* Public functions */

void
prefs_window_create(void)
{
	PrefsWindow *window;

	window = build_window();
	populate_window(window);

	gtk_widget_show(window->window);
}

void
prefs_window_destroy(PrefsWindow *window)
{
	gtk_widget_destroy(window->window);
	destroy_window(window);
}

/* Ok & Cancel signal handlers */

void
on_ok_button_clicked(G_GNUC_UNUSED GtkButton *button, PrefsWindow *data)
{
	/* Save values to preferences */
	retrieve_window_values(data);
	prefs_window_destroy(data);

	/* Save preferences to file */
	prefs_save();

	/* Make it effective */
	apply_prefs(1);
}

void
on_cancel_button_clicked(G_GNUC_UNUSED GtkButton *button, PrefsWindow *data)
{
	prefs_window_destroy(data);
}

/**
 * Callback function when a key is hit in prefs_window. Currently handles
 * Esc key (calls on_cancel_button_clicked())
 * and
 * Return key (calls on_ok_button_clicked()).
 *
 * @param widget the widget that received the signal
 * @param event the key event that was triggered
 * @param data struct holding the GtkWidgets of the preferences windows
 * @return TRUE to stop other handlers from being invoked for the event.
 * False to propagate the event further
 */
gboolean
on_key_press_event(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event, PrefsWindow *data)
{
	switch (event->keyval) {
	case GDK_KEY_Escape:
		on_cancel_button_clicked(NULL, data);
		break;
	case GDK_KEY_Return:
		on_ok_button_clicked(NULL, data);
		break;
	default:
		break;
	}

	return FALSE;
}
