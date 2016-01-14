/* ui-prefs.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-prefs.c
 * This file holds the ui-related code for the preferences window.
 * @brief preferences ui subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "ui-prefs.h"
#include "prefs.h"
#include "alsa.h"
#include "debug.h"

#include "main.h"
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <X11/XKBlib.h>

#ifdef WITH_GTK3
#define PREFS_UI_FILE "prefs-gtk3.glade"
#else
#define PREFS_UI_FILE "prefs-gtk2.glade"
#endif



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

/**
 * Handler for the signal 'changed' on the GtkComboBoxText widget
 * card_combo. This basically refills the channel list if the card
 * changes.
 *
 * @param box the box which received the signal
 * @param data user data set when the signal handler was connected
 */
void
on_card_changed(GtkComboBox *box, PrefsData *data)
{
	struct acard *card;
	gchar *card_name;

	card_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(box));
	card = find_card(card_name);
	g_free(card_name);

	if (card) {
		gchar *sel_chan = prefs_get_channel(card->name);
		fill_channel_combo(card->channels, data->chan_combo, sel_chan);
		g_free(sel_chan);
	}
}

/**
 * Handler for the signal 'toggled' on the GtkCheckButton vol_text_check.
 * Updates the preferences window.
 *
* @param button the button which received the signal
* @param data user data set when the signal handler was connected
 */
void
on_vol_text_toggle(GtkToggleButton *button, PrefsData *data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(data->vol_pos_label, active);
	gtk_widget_set_sensitive(data->vol_pos_combo, active);
}

/**
 * Handler for the signal 'toggled' on the GtkCheckButton draw_vol_check.
 * Updates the preferences window.
 *
 * @param button the button which received the signal
 * @param data user data set when the signal handler was connected
 */
void
on_draw_vol_toggle(GtkToggleButton *button, PrefsData *data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(data->vol_meter_pos_label, active);
	gtk_widget_set_sensitive(data->vol_meter_pos_spin, active);
	gtk_widget_set_sensitive(data->vol_meter_color_label, active);
	gtk_widget_set_sensitive(data->vol_meter_color_button, active);
}

/**
 * Handler for the signal 'changed' on the GtkComboBox middle_click_combo.
 * Updates the preferences window.
 *
 * @param box the combobox which received the signal
 * @param data user data set when the signal handler was connected
 */
void
on_middle_changed(GtkComboBox *box, PrefsData *data)
{
	gboolean cust = gtk_combo_box_get_active(GTK_COMBO_BOX(box)) == 3;
	gtk_widget_set_sensitive(data->custom_label, cust);
	gtk_widget_set_sensitive(data->custom_entry, cust);
}

/**
 * Handler for the signal 'toggled' on the GtkCheckButton enable_noti_check.
 * Updates the preferences window.
 *
 * @param button the button which received the signal
 * @param data user data set when the signal handler was connected
 */
#ifdef HAVE_LIBN
void
on_notification_toggle(GtkToggleButton *button, PrefsData *data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(data->noti_timeout_label, active);
	gtk_widget_set_sensitive(data->noti_timeout_spin, active);
	gtk_widget_set_sensitive(data->hotkey_noti_check, active);
	gtk_widget_set_sensitive(data->mouse_noti_check, active);
	gtk_widget_set_sensitive(data->popup_noti_check, active);
	gtk_widget_set_sensitive(data->external_noti_check, active);
}
#else
void
on_notification_toggle(G_GNUC_UNUSED GtkToggleButton *button,
		G_GNUC_UNUSED PrefsData *data)
{
}
#endif


/**
 * This is called from within the callback function
 * on_hotkey_button_click() in callbacks. which is triggered when
 * one of the hotkey boxes mute_eventbox, up_eventbox or
 * down_eventbox (GtkEventBox) in the preferences received
 * the button-press-event signal.
 *
 * Then this function grabs the keyboard, opens the hotkey_dialog
 * and updates the GtkLabel with the pressed hotkey.
 * The GtkLabel is later read by on_ok_button_clicked() in
 * callbacks.c which stores the result in the global keyFile.
 *
 * @param widget_name the name of the widget (mute_eventbox, up_eventbox
 * or down_eventbox)
 * @param data struct holding the GtkWidgets of the preferences windows
 */
static void
acquire_hotkey(const char *widget_name, PrefsData *data)
{
	gint resp, action;
	GtkWidget *diag = data->hotkey_dialog;

	action =
		(!strcmp(widget_name, "mute_eventbox")) ? 0 :
		(!strcmp(widget_name, "up_eventbox")) ? 1 :
		(!strcmp(widget_name, "down_eventbox")) ? 2 : -1;

	if (action < 0) {
		report_error(_("Invalid widget passed to acquire_hotkey: %s"),
			     widget_name);
		return;
	}

	switch (action) {
	case 0:
		gtk_label_set_text(GTK_LABEL(data->hotkey_key_label),
				   _("Mute/Unmute"));
		break;
	case 1:
		gtk_label_set_text(GTK_LABEL(data->hotkey_key_label),
				   _("Volume Up"));
		break;
	case 2:
		gtk_label_set_text(GTK_LABEL(data->hotkey_key_label),
				   _("Volume Down"));
		break;
	default:
		break;
	}

	// grab keyboard
	if (G_LIKELY(
#ifdef WITH_GTK3
		    gdk_device_grab(gtk_get_current_event_device(),
				    gdk_screen_get_root_window(gdk_screen_get_default()),
				    GDK_OWNERSHIP_APPLICATION,
				    TRUE, GDK_ALL_EVENTS_MASK, NULL, GDK_CURRENT_TIME)
#else
		    gdk_keyboard_grab(gtk_widget_get_root_window(
					      GTK_WIDGET(diag)), TRUE, GDK_CURRENT_TIME)
#endif
		    == GDK_GRAB_SUCCESS)) {
		resp = gtk_dialog_run(GTK_DIALOG(diag));
#ifdef WITH_GTK3
		gdk_device_ungrab(gtk_get_current_event_device(), GDK_CURRENT_TIME);
#else
		gdk_keyboard_ungrab(GDK_CURRENT_TIME);
#endif
		if (resp == GTK_RESPONSE_OK) {
			const gchar *key_name =
				gtk_label_get_text(GTK_LABEL(data->hotkey_key_label));
			if (!strcasecmp(key_name, "<Primary>c")) {
				key_name = "(None)";
			}
			switch (action) {
			case 0:
				gtk_label_set_text(GTK_LABEL(data->mute_hotkey_label),
						   key_name);
				break;
			case 1:
				gtk_label_set_text(GTK_LABEL(data->up_hotkey_label),
						   key_name);
				break;
			case 2:
				gtk_label_set_text(GTK_LABEL(data->down_hotkey_label),
						   key_name);
				break;
			default:
				break;
			}
		}
	} else
		report_error(_("Could not grab the keyboard."));
	gtk_widget_hide(diag);
}

/**
 * Callback function when one of the hotkey boxes mute_eventbox,
 * up_eventbox or down_eventbox (GtkEventBox) in the
 * preferences received the button-press-event signal.
 *
 * @param widget the object which received the signal
 * @param event the GdkEventButton which triggered this signal
 * @param data struct holding the GtkWidgets of the preferences windows
 * @return TRUE to stop other handlers from being invoked for the event.
 * False to propagate the event further
 */
static gboolean
on_hotkey_button_click(GtkWidget *widget, GdkEventButton *event,
		       PrefsData *data)
{

	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
		acquire_hotkey(gtk_buildable_get_name(GTK_BUILDABLE(widget)), data);
	return TRUE;
}

/**
 * Handler for the signal 'key-press-event' on the GtkDialog hotkey_dialog
 * which was opened by acquire_hotkey().
 *
 * @param dialog the dialog window which received the signal
 * @param ev the GdkEventKey which triggered the signal
 * @param data struct holding the GtkWidgets of the preferences windows
 * @return TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
hotkey_pressed(G_GNUC_UNUSED GtkWidget *dialog,
		GdkEventKey *ev,
		PrefsData *data)
{
	gchar *key_text;
	guint keyval;
	GdkModifierType state, consumed;

	state = ev->state;
	gdk_keymap_translate_keyboard_state(gdk_keymap_get_default(),
					    ev->hardware_keycode,
					    state, ev->group, &keyval, NULL, NULL, &consumed);

	state &= ~consumed;
	state &= gtk_accelerator_get_default_mod_mask();

	key_text = gtk_accelerator_name(keyval, state);
	gtk_label_set_text(GTK_LABEL(data->hotkey_key_label), key_text);
	g_free(key_text);
	return FALSE;
}

/**
 * Handler for the signal 'key-release-event' on the GtkDialog
 * hotkey_dialog which was opened by acquire_hotkey().
 *
 * @param dialog the dialog window which received the signal
 * @param ev the GdkEventKey which triggered the signal
 * @param data struct holding the GtkWidgets of the preferences windows
 * @return TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
static gboolean
hotkey_released(GtkWidget *dialog,
		G_GNUC_UNUSED GdkEventKey *ev,
		G_GNUC_UNUSED PrefsData *data)
{
#ifdef WITH_GTK3
	gdk_device_ungrab(gtk_get_current_event_device(), GDK_CURRENT_TIME);
#else
	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
#endif
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
	return FALSE;
}

/**
 * Handler for the signal 'toggled' on the GtkCheckButton
 * enable_hotkeys_check.
 * Updates the preferences window.
 *
 * @param button the button which received the signal
 * @param data user data set when the signal handler was connected
 */
static void
on_hotkey_toggle(GtkToggleButton *button, PrefsData *data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(data->hotkey_vol_label, active);
	gtk_widget_set_sensitive(data->hotkey_vol_spin, active);
}

#ifndef WITH_GTK3
/**
 * Gtk2 cludge
 * Gtk2 ComboBoxes don't have ids. We workaround that by
 * mapping the id with a ComboBox index. We're fine as long
 * as nobody changes the content of the ComboBox.
 *
 * @param combo_box a GtkComboBox
 * @return the ID of the active row
 */
static const gchar*
gtk_combo_box_get_active_id(GtkComboBox *combo_box)
{
	gint index = gtk_combo_box_get_active(combo_box);

	if (index == 1)
		return "horizontal";

	return "vertical";
}
#endif

/**
 * Callback function when the cancel_button (GtkButton) of the
 * preferences window received the clicked signal.
 *
 * @param button the object that received the signal
 * @param data struct holding the GtkWidgets of the preferences windows
 */
static void
on_cancel_button_clicked(G_GNUC_UNUSED GtkButton *button, PrefsData *data)
{
	gtk_widget_destroy(data->prefs_window);
	g_slice_free(PrefsData, data);
}

/**
 * Callback function when the ok_button (GtkButton) of the
 * preferences window received the clicked signal.
 *
 * @param button the object that received the signal
 * @param data struct holding the GtkWidgets of the preferences windows
 */
static void
on_ok_button_clicked(G_GNUC_UNUSED GtkButton *button, PrefsData *data)
{
	gint alsa_change = 0;

	// pull out various prefs

	// slider orientation
	GtkWidget *soc = data->slider_orientation_combo;
	const gchar *orientation = gtk_combo_box_get_active_id(GTK_COMBO_BOX(soc));
	prefs_set_string("SliderOrientation", orientation);
	
	// show vol text
	GtkWidget *vtc = data->vol_text_check;
	gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vtc));
	prefs_set_boolean("DisplayTextVolume", active);

	// vol pos
	GtkWidget *vpc = data->vol_pos_combo;
	gint idx = gtk_combo_box_get_active(GTK_COMBO_BOX(vpc));
	prefs_set_integer("TextVolumePosition", idx);

	// show vol meter
	GtkWidget *dvc = data->draw_vol_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dvc));
	prefs_set_boolean("DrawVolMeter", active);

	// vol meter pos
	GtkWidget *vmps = data->vol_meter_pos_spin;
	gint vmpos = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vmps));
	prefs_set_integer("VolMeterPos", vmpos);

	GtkWidget *vcb = data->vol_meter_color_button;
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

	// alsa card
	GtkWidget *acc = data->card_combo;
	gchar *old_card = prefs_get_string("AlsaCard", NULL);
	gchar *card = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(acc));
	if (old_card && strcmp(old_card, card))
		alsa_change = 1;
	prefs_set_string("AlsaCard", card);

	// channel
	GtkWidget *ccc = data->chan_combo;
	gchar *old_channel = NULL;
	if (old_card) {
		old_channel = prefs_get_channel(old_card);
		g_free(old_card);
	}
	gchar *chan = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(ccc));
	if (old_channel) {
		if (strcmp(old_channel, chan))
			alsa_change = 1;
		g_free(old_channel);
	}
	prefs_set_channel(card, chan);
	g_free(card);
	g_free(chan);

	// icon theme
	GtkWidget *system_theme = data->system_theme;
	active = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(system_theme));
	prefs_set_boolean("SystemTheme", active);

	// volume control command
	GtkWidget *ve = data->vol_control_entry;
	const gchar *vc = gtk_entry_get_text(GTK_ENTRY(ve));
	prefs_set_string("VolumeControlCommand", vc);

	// volume scroll steps
	GtkWidget *sss = data->scroll_step_spin;
	gdouble step = gtk_spin_button_get_value(GTK_SPIN_BUTTON(sss));
	prefs_set_double("ScrollStep", step);

	GtkWidget *fsss = data->fine_scroll_step_spin;
	gdouble fine_step = gtk_spin_button_get_value(GTK_SPIN_BUTTON(fsss));
	prefs_set_double("FineScrollStep", fine_step);
	
	// middle click
	GtkWidget *mcc = data->middle_click_combo;
	idx = gtk_combo_box_get_active(GTK_COMBO_BOX(mcc));
	prefs_set_integer("MiddleClickAction", idx);

	// custom command
	GtkWidget *ce = data->custom_entry;
	const gchar *cc = gtk_entry_get_text(GTK_ENTRY(ce));
	prefs_set_string("CustomCommand", cc);

	// normalize volume
	GtkWidget *vnorm = data->normalize_vol_check;
	gboolean is_pressed;
	is_pressed = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vnorm));
	prefs_set_boolean("NormalizeVolume", is_pressed);

	// hotkey enable
	GtkWidget *hkc = data->enable_hotkeys_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(hkc));
	prefs_set_boolean("EnableHotKeys", active);

	// scroll step
	GtkWidget *hs = data->hotkey_vol_spin;
	gint hotstep = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(hs));
	prefs_set_integer("HotkeyVolumeStep", hotstep);
	
	// hotkeys
	guint keysym;
	gint keycode;
	GdkModifierType mods;
	GtkWidget *kl = data->mute_hotkey_label;
	gtk_accelerator_parse(gtk_label_get_text(GTK_LABEL(kl)), &keysym, &mods);
	if (keysym != 0)
		keycode = XKeysymToKeycode(gdk_x11_get_default_xdisplay(), keysym);
	else
		keycode = -1;
	prefs_set_integer("VolMuteKey", keycode);
	prefs_set_integer("VolMuteMods", mods);

	kl = data->up_hotkey_label;
	gtk_accelerator_parse(gtk_label_get_text(GTK_LABEL(kl)), &keysym, &mods);
	if (keysym != 0)
		keycode = XKeysymToKeycode(gdk_x11_get_default_xdisplay(), keysym);
	else
		keycode = -1;
	prefs_set_integer("VolUpKey", keycode);
	prefs_set_integer("VolUpMods", mods);

	kl = data->down_hotkey_label;
	gtk_accelerator_parse(gtk_label_get_text(GTK_LABEL(kl)), &keysym, &mods);
	if (keysym != 0)
		keycode = XKeysymToKeycode(gdk_x11_get_default_xdisplay(), keysym);
	else
		keycode = -1;
	prefs_set_integer("VolDownKey", keycode);
	prefs_set_integer("VolDownMods", mods);

#ifdef HAVE_LIBN
	// notification prefs
	GtkWidget *nc = data->enable_noti_check;
	gint noti_spin;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(nc));
	prefs_set_boolean("EnableNotifications", active);

	nc = data->hotkey_noti_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(nc));
	prefs_set_boolean("HotkeyNotifications", active);

	nc = data->mouse_noti_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(nc));
	prefs_set_boolean("MouseNotifications", active);

	nc = data->popup_noti_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(nc));
	prefs_set_boolean("PopupNotifications", active);

	nc = data->external_noti_check;
	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(nc));
	prefs_set_boolean("ExternalNotifications", active);

	nc = data->noti_timeout_spin;
	noti_spin = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(nc));
	prefs_set_integer("NotificationTimeout", noti_spin);
#endif

	prefs_save();
	apply_prefs(alsa_change);

	gtk_widget_destroy(data->prefs_window);
	g_slice_free(PrefsData, data);
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
set_label_for_keycode(GtkWidget *label, gint code, GdkModifierType mods)
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

/**
 * Creates the whole preferences window by reading the corresponding
 * ui file, and returns it as a GtkWidget pointer.
 *
 * @return the newly created preferences window, NULL on failure
 */
GtkWidget *
ui_prefs_create_window(void)
{
	GtkBuilder *builder;
	GError *error = NULL;

	gdouble *vol_meter_clrs;
	gchar *uifile, *slider_orientation, *vol_cmd, *custcmd;

	PrefsData *prefs_data;

	uifile = get_ui_file(PREFS_UI_FILE);
	if (!uifile) {
		report_error(_
			     ("Can't find preferences user interface file. "
			      "Please ensure PNMixer is installed correctly."));
		return NULL;
	}

	DEBUG_PRINT("Loading prefs ui from '%s'", uifile);
	builder = gtk_builder_new();
	if (!gtk_builder_add_from_file(builder, uifile, &error)) {
		g_warning("%s", error->message);
		report_error(error->message);
		g_error_free(error);
		g_free(uifile);
		g_object_unref(G_OBJECT(builder));
		return NULL;
	}

	g_free(uifile);

	prefs_data = g_slice_new(PrefsData);
#define GO(name) prefs_data->name = GTK_WIDGET(gtk_builder_get_object(builder, #name))
	GO(prefs_window);
	GO(card_combo);
	GO(chan_combo);
	GO(normalize_vol_check);
	GO(vol_pos_label);
	GO(vol_pos_combo);
	GO(vol_meter_pos_label);
	GO(vol_meter_pos_spin);
	GO(vol_meter_color_label);
	GO(vol_meter_color_button);
	GO(custom_label);
	GO(custom_entry);
	GO(slider_orientation_combo);
	GO(vol_text_check);
	GO(draw_vol_check);
	GO(system_theme);
	GO(vol_control_entry);
	GO(scroll_step_spin);
	GO(fine_scroll_step_spin);
	GO(middle_click_combo);
	GO(enable_hotkeys_check);
	GO(hotkey_vol_label);
	GO(hotkey_vol_spin);
	GO(hotkey_dialog);
	GO(hotkey_key_label);
	GO(mute_hotkey_label);
	GO(up_hotkey_label);
	GO(down_hotkey_label);
#ifdef HAVE_LIBN
	GO(enable_noti_check);
	GO(noti_timeout_spin);
	GO(noti_timeout_label);
	GO(hotkey_noti_check);
	GO(mouse_noti_check);
	GO(popup_noti_check);
	GO(external_noti_check);
#endif
#undef GO

	// slider orientation
	slider_orientation = prefs_get_string("SliderOrientation", NULL);
	if (slider_orientation) {
		GtkComboBox *combo_box = GTK_COMBO_BOX
			(prefs_data->slider_orientation_combo);
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
	
	// vol text display
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(prefs_data->vol_text_check),
	 prefs_get_boolean("DisplayTextVolume", FALSE));

	gtk_combo_box_set_active
	(GTK_COMBO_BOX(prefs_data->vol_pos_combo),
	 prefs_get_integer("TextVolumePosition", 0));

	// volume meter
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(prefs_data->draw_vol_check),
	 prefs_get_boolean("DrawVolMeter", FALSE));

	gtk_adjustment_set_upper
	(GTK_ADJUSTMENT(gtk_builder_get_object(builder,
	                                       "vol_meter_pos_adjustment")),
	 tray_icon_size() - 10);

	gtk_spin_button_set_value
	(GTK_SPIN_BUTTON(prefs_data->vol_meter_pos_spin),
	 prefs_get_integer("VolMeterPos", 0));

	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(prefs_data->system_theme),
	 prefs_get_boolean("SystemTheme", FALSE));

	// set color button color
	vol_meter_clrs = prefs_get_vol_meter_colors();
#ifdef WITH_GTK3
	GdkRGBA vol_meter_color_button_color;

	vol_meter_color_button_color.red = vol_meter_clrs[0];
	vol_meter_color_button_color.green = vol_meter_clrs[1];
	vol_meter_color_button_color.blue = vol_meter_clrs[2];
	vol_meter_color_button_color.alpha = 1.0;

	gtk_color_chooser_set_rgba(
	GTK_COLOR_CHOOSER(prefs_data->vol_meter_color_button),
	&vol_meter_color_button_color);
#else
	GdkColor vol_meter_color_button_color;

	vol_meter_color_button_color.red = (guint32) (vol_meter_clrs[0] * 65536);
	vol_meter_color_button_color.green = (guint32) (vol_meter_clrs[1] * 65536);
	vol_meter_color_button_color.blue = (guint32) (vol_meter_clrs[2] * 65536);

	gtk_color_button_set_color(
	GTK_COLOR_BUTTON(prefs_data->vol_meter_color_button),
	&vol_meter_color_button_color);
#endif
	g_free(vol_meter_clrs);

	// fill in card/channel combo boxes
	fill_card_combo(prefs_data->card_combo, prefs_data->chan_combo);

	// volume normalization (ALSA mapped)
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(prefs_data->normalize_vol_check),
	 prefs_get_boolean("NormalizeVolume", FALSE));

	// volume command
	vol_cmd = prefs_get_vol_command();
	if (vol_cmd) {
		gtk_entry_set_text(GTK_ENTRY(prefs_data->vol_control_entry), vol_cmd);
		g_free(vol_cmd);
	}
	                           
	// volume scroll steps
	gtk_spin_button_set_value
	(GTK_SPIN_BUTTON(prefs_data->scroll_step_spin),
	 prefs_get_double("ScrollStep", 5));

	gtk_spin_button_set_value
	(GTK_SPIN_BUTTON(prefs_data->fine_scroll_step_spin),
	 prefs_get_double("FineScrollStep", 1));

	//  middle click
	gtk_combo_box_set_active
	(GTK_COMBO_BOX(prefs_data->middle_click_combo),
	 prefs_get_integer("MiddleClickAction", 0));

	// custom command
	gtk_entry_set_invisible_char(GTK_ENTRY(prefs_data->custom_entry), 8226);

	custcmd = prefs_get_string("CustomCommand", NULL);
	if (custcmd) {
		gtk_entry_set_text(GTK_ENTRY(prefs_data->custom_entry), custcmd);
		g_free(custcmd);
	}

	on_vol_text_toggle(GTK_TOGGLE_BUTTON(prefs_data->vol_text_check),
			   prefs_data);
	on_draw_vol_toggle(GTK_TOGGLE_BUTTON(prefs_data->draw_vol_check),
			   prefs_data);
	on_middle_changed(GTK_COMBO_BOX(prefs_data->middle_click_combo),
			  prefs_data);

	// hotkeys
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(prefs_data->enable_hotkeys_check),
	 prefs_get_boolean("EnableHotKeys", FALSE));

	// hotkey step
	gtk_spin_button_set_value
	(GTK_SPIN_BUTTON(prefs_data->hotkey_vol_spin),
	 prefs_get_integer("HotkeyVolumeStep", 1));

	set_label_for_keycode(prefs_data->mute_hotkey_label,
	                      prefs_get_integer("VolMuteKey", -1),
	                      prefs_get_integer("VolMuteMods", 0));

	set_label_for_keycode(prefs_data->up_hotkey_label,
	                      prefs_get_integer("VolUpKey", -1),
	                      prefs_get_integer("VolUpMods", 0));

	set_label_for_keycode(prefs_data->down_hotkey_label,
	                      prefs_get_integer("VolDownKey", -1),
	                      prefs_get_integer("VolDownMods", 0));

	on_hotkey_toggle(GTK_TOGGLE_BUTTON(prefs_data->enable_hotkeys_check),
			 prefs_data);

	gtk_notebook_append_page(GTK_NOTEBOOK(gtk_builder_get_object(builder,
					      "notebook1")),
#ifdef HAVE_LIBN
				 GTK_WIDGET(gtk_builder_get_object(builder,
						 "notification_vbox")),
#else
				 GTK_WIDGET(gtk_builder_get_object
					    (builder, "no_notification_label")),
#endif
				 gtk_label_new(_("Notifications")));

#ifdef HAVE_LIBN
	// notification checkboxes
	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(prefs_data->enable_noti_check),
	 prefs_get_boolean("EnableNotifications", FALSE));

	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(prefs_data->hotkey_noti_check),
	 prefs_get_boolean("HotkeyNotifications", TRUE));

	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(prefs_data->mouse_noti_check),
	 prefs_get_boolean("MouseNotifications", TRUE));

	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(prefs_data->popup_noti_check),
	 prefs_get_boolean("PopupNotifications", FALSE));

	gtk_toggle_button_set_active
	(GTK_TOGGLE_BUTTON(prefs_data->external_noti_check),
	 prefs_get_boolean("ExternalNotifications", FALSE));

	on_notification_toggle
	(GTK_TOGGLE_BUTTON(prefs_data->enable_noti_check),
	 prefs_data);

	gtk_spin_button_set_value
	(GTK_SPIN_BUTTON(prefs_data->noti_timeout_spin),
	 prefs_get_integer("NotificationTimeout", 1500));
#endif

	gtk_builder_connect_signals(builder, prefs_data);
	g_object_unref(G_OBJECT(builder));

	return prefs_data->prefs_window;
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
static gboolean
on_key_press(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event, PrefsData *data)
{

	switch (event->keyval) {
	case GDK_KEY_Escape:
		on_cancel_button_clicked(NULL, data);
		break;
	case GDK_KEY_Return:
		on_ok_button_clicked(NULL, data);
		break;
	default:
		return FALSE;
	}
	return TRUE;
}


