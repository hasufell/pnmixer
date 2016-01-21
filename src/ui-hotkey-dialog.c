/* ui-hotkey-dialog.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file ui-hotkey-dialog.c
 * This file holds the ui-related code for the hotkey dialog,
 * usually run from the preferences window.
 * @brief Hotkey dialog subsystem
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>

#include "support.h"
#include "ui-hotkey-dialog.h"

#ifdef WITH_GTK3
#define HOTKEY_DIALOG_UI_FILE      "hotkey-dialog-gtk3.glade"
#else
#define HOTKEY_DIALOG_UI_FILE      "hotkey-dialog-gtk2.glade"
#endif

struct hotkey_dialog {
	GtkWidget *hotkey_dialog;
	GtkWidget *key_label;
};

typedef struct hotkey_dialog HotkeyDialog;

/* Signal handlers */

/**
 * Handles the 'key-press-event' signal on the GtkDialog 'hotkey_dialog'.
 * Update the text displayed in the dialog.
 *
 * @param widget the object which received the signal.
 * @param event the GdkEventKey which triggered the signal.
 * @param dialog user data set when the signal handler was connected.
 * @return TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
gboolean
on_hotkey_dialog_key_press_event(G_GNUC_UNUSED GtkWidget *widget,
                                 GdkEventKey *event, HotkeyDialog *dialog)
{
	GdkModifierType state, consumed;
	GtkLabel *key_label;
	gchar *key_text;
	guint key_val;

	key_label = GTK_LABEL(dialog->key_label);

	state = event->state;
	gdk_keymap_translate_keyboard_state(gdk_keymap_get_default(),
					    event->hardware_keycode,
					    state, event->group, &key_val,
	                                    NULL, NULL, &consumed);

	state &= ~consumed;
	state &= gtk_accelerator_get_default_mod_mask();

	key_text = gtk_accelerator_name(key_val, state);
	gtk_label_set_text(key_label, key_text);
	g_free(key_text);

	return FALSE;
}

/**
 * Handles the signal 'key-release-event' on the GtkDialog 'hotkey_dialog'.
 * Closes the dialog.
 *
 * @param widget the object which received the signal.
 * @param event the GdkEventKey which triggered the signal.
 * @param dialog user data set when the signal handler was connected.
 * @return TRUE to stop other handlers from being invoked for the event.
 * FALSE to propagate the event further.
 */
gboolean
on_hotkey_dialog_key_release_event(GtkWidget *widget,
                                   G_GNUC_UNUSED GdkEventKey *event,
                                   G_GNUC_UNUSED HotkeyDialog *dialog)
{
	gtk_dialog_response(GTK_DIALOG(widget), GTK_RESPONSE_OK);

	return FALSE;
}

/* Helpers */

static const gchar *
hotkey_dialog_run(HotkeyDialog *dialog)
{
	gint resp;
	GdkGrabStatus grab_status;
	GtkDialog *hotkey_dialog = GTK_DIALOG(dialog->hotkey_dialog);

	/* Grab keyboard */
	grab_status =
#ifdef WITH_GTK3
		gdk_device_grab(gtk_get_current_event_device(),
		                gdk_screen_get_root_window(gdk_screen_get_default()),
		                GDK_OWNERSHIP_APPLICATION,
		                TRUE, GDK_ALL_EVENTS_MASK, NULL, GDK_CURRENT_TIME);
#else
		gdk_keyboard_grab(gtk_widget_get_root_window(GTK_WIDGET(hotkey_dialog)),
		                  TRUE, GDK_CURRENT_TIME);
#endif

	if (grab_status != GDK_GRAB_SUCCESS) {
		report_error(_("Could not grab the keyboard."));
		return NULL;
	}

	/* Run the dialog */
	resp = gtk_dialog_run(hotkey_dialog);

	/* Ungrab keyboard */
#ifdef WITH_GTK3
	gdk_device_ungrab(gtk_get_current_event_device(), GDK_CURRENT_TIME);
#else
	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
#endif

	/* Handle response */
	if (resp != GTK_RESPONSE_OK)
		return NULL;

	/* Return the key entered */
	return gtk_label_get_text(GTK_LABEL(dialog->key_label));
}

static void
hotkey_dialog_destroy(HotkeyDialog *dialog)
{
	gtk_widget_destroy(dialog->hotkey_dialog);
	g_free(dialog);
}

static HotkeyDialog *
hotkey_dialog_create(GtkWindow *parent, const gchar *hotkey_desc)
{
	gchar *uifile;
	GtkBuilder *builder;
	HotkeyDialog *dialog;

	dialog = g_new0(HotkeyDialog, 1);

	/* Build UI file */
	uifile = get_ui_file(HOTKEY_DIALOG_UI_FILE);
	g_assert(uifile);

	DEBUG("Building hotkey dialog from ui file '%s'", uifile);
	builder = gtk_builder_new_from_file(uifile);

	/* Save some widgets for later use */
	assign_gtk_widget(builder, dialog, hotkey_dialog);
	assign_gtk_widget(builder, dialog, key_label);

	/* Configure some widgets */
	gtk_label_set_text(GTK_LABEL(dialog->key_label), hotkey_desc);

	/* Set transient parent */
	gtk_window_set_transient_for(GTK_WINDOW(dialog->hotkey_dialog), parent);

	/* Connect signal handlers */
	gtk_builder_connect_signals(builder, dialog);

	/* Cleanup */
	g_object_unref(builder);
	g_free(uifile);

	return dialog;
}

/* Public function */

gchar *
hotkey_dialog_do(GtkWindow *parent, const gchar *hotkey_desc)
{
	HotkeyDialog *dialog;
	const gchar *key_entered;
	gchar *ret;

	dialog = hotkey_dialog_create(parent, hotkey_desc);
	key_entered = hotkey_dialog_run(dialog);
	ret = g_strdup(key_entered);
	hotkey_dialog_destroy(dialog);

	return ret;
}
