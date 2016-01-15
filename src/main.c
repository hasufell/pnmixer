/* main.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file main.c
 * The main program entry point. Also handles creating and opening
 * the widgets, connecting signals, updating the tray icon and
 * error handling.
 * @brief gtk+ initialization
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <locale.h>
#include "alsa.h"
#include "debug.h"
#include "callbacks.h"
#include "main.h"
#include "notify.h"
#include "support.h"
#include "hotkeys.h"
#include "prefs.h"
#include "ui-prefs.h"
#include "ui-popup-menu.h"
#include "ui-popup-window.h"

enum {
	VOLUME_MUTED,
	VOLUME_OFF,
	VOLUME_LOW,
	VOLUME_MEDIUM,
	VOLUME_HIGH,
	N_VOLUME_ICONS
};

static GtkStatusIcon *tray_icon = NULL;
static GdkPixbuf *status_icons[N_VOLUME_ICONS] = { NULL };

static char err_buf[512];

static PopupMenu *popup_menu;
static PopupWindow *popup_window;

/**
 * Reports an error, usually via a dialog window or
 * on stderr.
 *
 * @param err the error
 * @param ... more string segments in the format of printf
 */
void
report_error(char *err, ...)
{
	va_list ap;
	va_start(ap, err);
	if (popup_window) {
		vsnprintf(err_buf, 512, err, ap);
		GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(popup_window->window),
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_CLOSE,
				    NULL);
		gtk_window_set_title(GTK_WINDOW(dialog), _("PNMixer Error"));
		g_object_set(dialog, "text", err_buf, NULL);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	} else {
		vfprintf(stderr, err, ap);
		fprintf(stderr, "\n");
	}
	va_end(ap);
}

/**
 * Emits a warning if the sound connection is lost, usually
 * via a dialog window (with option to reinitialize alsa) or stderr.
 */
void
warn_sound_conn_lost(void)
{
	if (popup_window) {
		gint resp;
		GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(popup_window->window),
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_YES_NO,
				    _("Warning: Connection to sound system failed."));
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
				_("Do you want to re-initialize the connection to alsa?\n\n"
				  "If you do not, you will either need to restart PNMixer "
				  "or select the 'Reload Alsa' option in the right click "
				  "menu in order for PNMixer to function."));
		gtk_window_set_title(GTK_WINDOW(dialog), _("PNMixer Error"));
		resp = gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
		if (resp == GTK_RESPONSE_YES)
			do_alsa_reinit();
	} else
		fprintf(stderr,
			_("Warning: Connection to sound system failed, "
			  "you probably need to restart pnmixer\n"));
}

/**
 * Runs a given command via g_spawn_command_line_async().
 *
 * @param cmd the command to run
 */
static void
run_command(const gchar *cmd)
{
	GError *error = NULL;

	g_assert(cmd != NULL);

	gtk_widget_hide(popup_window->window);

	if (g_spawn_command_line_async(cmd, &error) == FALSE) {
		report_error(_("Unable to run command: %s"), error->message);
		g_error_free(error);
		error = NULL;
	}
}

/* FIXME: return type should be gboolean */
/**
 * Handles button-release-event' signal on the tray_icon, currently
 * only used for middle-click.
 *
 * @param status_icon the object which received the signal
 * @param event the GdkEventButton which triggered this signal
 * @param user_data user data set when the signal handler was
 * connected
 */
void
tray_icon_button(G_GNUC_UNUSED GtkStatusIcon *status_icon,
		 GdkEventButton *event, G_GNUC_UNUSED gpointer user_data)
{
	gint action;

	if (event->button != 2)
		return;
	
	action = prefs_get_integer("MiddleClickAction", 0);

	switch (action) {
	case 0:	// mute/unmute
		setmute(mouse_noti);
		on_volume_has_changed();
		break;
	case 1:
		do_prefs();
		break;
	case 2:
		do_mixer();
		break;
	case 3: {
		gchar *cmd;

		cmd = prefs_get_string("CustomCommand", NULL);

		if (cmd) {
			run_command(cmd);
			g_free(cmd);
		} else
			report_error(_("You have not specified a custom command to run, "
			               "please specify one in preferences."));
		break;
	}
	default: {
	}	// nothing
	}
}

/**
 * Handles the 'activate' signal on the tray_icon,
 * usually opening the popup_window and grabbing pointer and keyboard.
 *
 * @param status_icon the object which received the signal
 * @param user_data user data set when the signal handler was connected
 */
void
tray_icon_on_click(G_GNUC_UNUSED GtkStatusIcon *status_icon,
		G_GNUC_UNUSED gpointer user_data)
{
	get_current_levels();
	if (!gtk_widget_get_visible(GTK_WIDGET(popup_window->window))) {
		gtk_widget_show_now(popup_window->window);
		gtk_widget_grab_focus(popup_window->vol_scale);
#ifdef WITH_GTK3
		GdkDevice *pointer_dev = gtk_get_current_event_device();
		if (pointer_dev != NULL) {
			GdkDevice *keyboard_dev =
				gdk_device_get_associated_device(pointer_dev);
			if (gdk_device_grab(pointer_dev,
					    gtk_widget_get_window(GTK_WIDGET(popup_window->window)),
					    GDK_OWNERSHIP_NONE,
					    TRUE,
					    GDK_BUTTON_PRESS_MASK,
					    NULL, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
				g_warning("Could not grab %s\n",
					  gdk_device_get_name(pointer_dev));
			if (keyboard_dev != NULL) {
				if (gdk_device_grab(keyboard_dev,
						    gtk_widget_get_window(GTK_WIDGET(popup_window->window)),
						    GDK_OWNERSHIP_NONE,
						    TRUE,
						    GDK_KEY_PRESS_MASK,
						    NULL, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS)
					g_warning("Could not grab %s\n",
						  gdk_device_get_name(keyboard_dev));
			}
		}
#else
		gdk_keyboard_grab(gtk_widget_get_window(popup_window->window),
				  TRUE, GDK_CURRENT_TIME);
		gdk_pointer_grab(gtk_widget_get_window(popup_window->window), TRUE,
				 GDK_BUTTON_PRESS_MASK, NULL, NULL, GDK_CURRENT_TIME);
#endif
	} else {
		gtk_widget_hide(popup_window->window);
	}
}

/**
 * Returns the size of the tray icon.
 *
 * @return size of the tray icon or 48 if there is none
 */
gint
tray_icon_size(void)
{
	// gtk_status_icon_is_embedded returns false until the prefs
	// window is opened on gtk3
	if (tray_icon && GTK_IS_STATUS_ICON(tray_icon))
		return gtk_status_icon_get_size(tray_icon);
	return 48;
}

/**
 * Handles the 'size-changed' signal on the tray_icon by
 * calling update_status_icons().
 *
 * @param status_icon the object which received the signal
 * @param size the new size
 * @param user_data set when the signal handler was connected
 * @return FALSE, so Gtk+ scales the icon as necessary
 */
static gboolean
tray_icon_resized(G_GNUC_UNUSED GtkStatusIcon *status_icon,
		G_GNUC_UNUSED gint size, G_GNUC_UNUSED gpointer user_data)
{
	update_status_icons();
	return FALSE;
}

/**
 * Creates the tray icon and connects the signals 'scroll_event'
 * and 'size-changed'.
 *
 * @return the newly created tray icon
 */
GtkStatusIcon *
create_tray_icon(void)
{
	DEBUG_PRINT("Creating tray icon");
	
	tray_icon = gtk_status_icon_new();

	/* catch scroll-wheel events */
	g_signal_connect((gpointer) tray_icon, "scroll_event",
			 G_CALLBACK(on_scroll), NULL);
	g_signal_connect((gpointer) tray_icon, "size-changed",
			 G_CALLBACK(tray_icon_resized), NULL);

	gtk_status_icon_set_visible(tray_icon, TRUE);
	return tray_icon;
}

/**
 * Handles the 'popup-menu' signal on the tray_icon, which brings
 * up the context menu, usually activated by right-click.
 *
 * @param status_icon the object which received the signal
 * @param button the button that was pressed, or 0 if the signal
 * is not emitted in response to a button press event
 * @param activate_time the timestamp of the event that triggered
 * the signal emission
 * @param menu user data set when the signal handler was connected
 */
static void
popup_callback(GtkStatusIcon *status_icon, guint button,
	       guint activate_time, GtkMenu *menu)
{
	gtk_widget_hide(popup_window->window);
	gtk_menu_popup(menu, NULL, NULL,
		       gtk_status_icon_position_menu, status_icon,
		       button, activate_time);
}

/**
 * Sets the global options enable_noti, hotkey_noti, mouse_noti, popup_noti,
 * noti_timeout and external_noti from the user settings.
 */
static void
set_notification_options(void)
{
	enable_noti = prefs_get_boolean("EnableNotifications", FALSE);
	hotkey_noti = prefs_get_boolean("HotkeyNotifications", TRUE);
	mouse_noti = prefs_get_boolean("MouseNotifications", TRUE);
	popup_noti = prefs_get_boolean("PopupNotifications", FALSE);
	external_noti = prefs_get_boolean("ExternalNotifications", FALSE);
	noti_timeout = prefs_get_integer("NotificationTimeout", 1500);
}

/**
 * Applies the preferences, usually triggered by on_ok_button_clicked()
 * in callbacks.c, but also initially called from main().
 *
 * @param alsa_change whether we want to trigger alsa-reinitalization
 */
void
apply_prefs(gint alsa_change)
{
	gdouble *vol_meter_clrs;
	GtkAdjustment *vol_adj = popup_window->vol_adj;

	scroll_step = prefs_get_integer("ScrollStep", 5);
	gtk_adjustment_set_page_increment(vol_adj, scroll_step);

	fine_scroll_step = prefs_get_integer("FineScrollStep", 1);
	gtk_adjustment_set_step_increment(vol_adj, fine_scroll_step);

	if (prefs_get_boolean("EnableHotKeys", FALSE)) {
		gint mk, uk, dk, mm, um, dm, hstep;
		mk = prefs_get_integer("VolMuteKey", -1);
		uk = prefs_get_integer("VolUpKey", -1);
		dk = prefs_get_integer("VolDownKey", -1);
		mm = prefs_get_integer("VolMuteMods", 0);
		um = prefs_get_integer("VolUpMods", 0);
		dm = prefs_get_integer("VolDownMods", 0);
		hstep = prefs_get_integer("HotkeyVolumeStep", 1);
		hotkeys_grab(mk, uk, dk, mm, um, dm, hstep);
	} else
		// will actually just ungrab everything
		hotkeys_grab(-1, -1, -1, 0, 0, 0, 1);

	set_notification_options();

	vol_meter_clrs = prefs_get_vol_meter_colors();
	set_vol_meter_color(vol_meter_clrs[0], vol_meter_clrs[1],
			    vol_meter_clrs[2]);
	g_free(vol_meter_clrs);

	update_status_icons();
	popup_window_update(popup_window);

	if (alsa_change)
		do_alsa_reinit();
}

static gboolean
on_prefs_ok_clicked(G_GNUC_UNUSED GtkButton *button, UiPrefsData *data)
{
	/* Save values to preferences */
	ui_prefs_retrieve_values(data);
	ui_prefs_destroy_window(data);

	/* Save preferences to file */
	prefs_save();

	/* Make it effective */
	apply_prefs(1);

	return TRUE;
}

static gboolean
on_prefs_cancel_clicked(G_GNUC_UNUSED GtkButton *button, UiPrefsData *data)
{
	ui_prefs_destroy_window(data);

	return TRUE;
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
on_key_pressed(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event, UiPrefsData *data)
{
	switch (event->keyval) {
	case GDK_KEY_Escape:
		return on_prefs_cancel_clicked(NULL, data);
		break;
	case GDK_KEY_Return:
		return on_prefs_ok_clicked(NULL, data);
		break;
	default:
		return FALSE;
	}
}

void
do_mute(gboolean notify)
{
        setmute(notify);
        on_volume_has_changed();
}

/**
 * Opens the specified mixer application which can be triggered either
 * by clicking the 'Volume Control' GtkImageMenuItem in the context
 * menu, the GtkButton 'Mixer' in the left-click popup_window or
 * by middle-click if the Middle Click Action in the preferences
 * is set to 'Volume Control'.
 */
void
do_mixer(void)
{
	gchar *cmd;

	cmd = prefs_get_vol_command();

	if (cmd) {
		run_command(cmd);
		g_free(cmd);
	} else
		report_error(_
			     ("No mixer application was found on your system. "
			      "Please open preferences and set the command you want "
			      "to run for volume control."));
}

/**
 * Brings up the preferences window, either triggered by clicking
 * on the GtkImageMenuItem 'Preferences' in the context menu
 * or by middle-click if the Middle Click Action in the preferences
 * is set to 'Show Preferences'.
 */
void
do_prefs(void)
{
	ui_prefs_create_window(G_CALLBACK(on_prefs_ok_clicked),
	                       G_CALLBACK(on_prefs_cancel_clicked),
	                       G_CALLBACK(on_key_pressed));
}

/**
 * Reinitializes alsa and updates the various states.
 */
void
do_alsa_reinit(void)
{
	alsa_init();
	update_status_icons();
	popup_window_update(popup_window);
	on_volume_has_changed();
}

/**
 * Gets the current volume level and adjusts the GtkAdjustment
 * vol_scale_adjustment widget which is used by GtkHScale/GtkScale.
 */
void
get_current_levels(void)
{
	int tmpvol = getvol();
	GtkAdjustment *vol_adj = popup_window->vol_adj;

	gtk_adjustment_set_value(vol_adj, (double) tmpvol);
}

static float vol_div_factor;
static int vol_meter_width;
static guchar *vol_meter_row = NULL;

/**
 * Draws the volume meter on top of the icon.
 *
 * @param pixbuf the GdkPixbuf icon to draw the volume meter on
 * @param x offset
 * @param y offset
 * @param h height of the volume meter
 */
static void
draw_vol_meter(GdkPixbuf *pixbuf, int x, int y, int h)
{
	int width, height, rowstride, n_channels, i;
	guchar *pixels, *p;

	n_channels = gdk_pixbuf_get_n_channels(pixbuf);

	g_assert(gdk_pixbuf_get_colorspace(pixbuf) == GDK_COLORSPACE_RGB);
	g_assert(gdk_pixbuf_get_bits_per_sample(pixbuf) == 8);
	g_assert(gdk_pixbuf_get_has_alpha(pixbuf));
	g_assert(n_channels == 4);

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);

	g_assert(x >= 0 && x < width);
	g_assert(y + h >= 0 && y < height);

	rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	pixels = gdk_pixbuf_get_pixels(pixbuf);

	y = height - y;
	for (i = 0; i < h; i++) {
		p = pixels + (y - i) * rowstride + x * n_channels;
		memcpy(p, vol_meter_row, vol_meter_width);
	}
}

static int draw_offset = 0;
static GdkPixbuf *icon_copy = NULL;

/**
 * Updates the tray icon. Usually called after volume has been muted
 * or changed.
 */
void
update_tray_icon(void)
{
	int muted;
	int tmpvol = getvol();
	char tooltip[60];
	gchar *active_card_name = (alsa_get_active_card())->name;
	const char *active_channel = alsa_get_active_channel();

	muted = ismuted();

	if (muted == 1) {
		GdkPixbuf *icon;

		if (tmpvol == 0)
			icon = status_icons[VOLUME_OFF];
		else if (tmpvol < 33)
			icon = status_icons[VOLUME_LOW];
		else if (tmpvol < 66)
			icon = status_icons[VOLUME_MEDIUM];
		else
			icon = status_icons[VOLUME_HIGH];
		sprintf(tooltip, _("%s (%s)\nVolume: %d %%"), active_card_name,
			active_channel,
			tmpvol);

		if (vol_meter_row) {
			GdkPixbuf *old_icon = icon_copy;
			icon_copy = gdk_pixbuf_copy(icon);
			draw_vol_meter(icon_copy, draw_offset, 5,
				       (tmpvol * vol_div_factor));
			if (old_icon)
				g_object_unref(old_icon);
			gtk_status_icon_set_from_pixbuf(tray_icon, icon_copy);
		} else
			gtk_status_icon_set_from_pixbuf(tray_icon, icon);
	} else {
		gtk_status_icon_set_from_pixbuf(tray_icon, status_icons[VOLUME_MUTED]);
		sprintf(tooltip, _("%s (%s)\nVolume: %d %%\nMuted"), active_card_name,
			active_channel, tmpvol);
	}
	gtk_status_icon_set_tooltip_text(tray_icon, tooltip);
}

/**
 * Updates the states that always needs to be updated on volume changes.
 * This is currently the tray icon and the mute checkboxes.
 */
void
on_volume_has_changed(void)
{
	update_tray_icon();
	popup_window_update(popup_window);
	popup_menu_update(popup_menu);
}

static guchar vol_meter_red, vol_meter_green, vol_meter_blue;

/**
 * Sets the color of the volume meter which is drawn on top
 * of the tray_icon.
 *
 * @param nr red color strength, from 0 - 1.0
 * @param ng green color strength, from 0 - 1.0
 * @param nb blue color strength, from 0 - 1.0
 */
void
set_vol_meter_color(gdouble nr, gdouble ng, gdouble nb)
{
	vol_meter_red = nr * 255;
	vol_meter_green = ng * 255;
	vol_meter_blue = nb * 255;
}

/**
 * Updates all status icons for the different volume states like
 * muted, low, medium, high as well as the volume meter. This
 * is triggered either by apply_prefs() in the preferences subsystem,
 * do_alsa_reinit() or tray_icon_resized().
 */
void
update_status_icons(void)
{
	int i, icon_width;
	GdkPixbuf *old_icons[N_VOLUME_ICONS];
	int size = tray_icon_size();

	for (i = 0; i < N_VOLUME_ICONS; i++)
		old_icons[i] = status_icons[i];

	/* Handle icons depending on the theme */
	if (prefs_get_boolean("SystemTheme", FALSE)) {
		status_icons[VOLUME_MUTED] = get_stock_pixbuf("audio-volume-muted",
					     size);
		status_icons[VOLUME_OFF] = get_stock_pixbuf("audio-volume-off", size);
		status_icons[VOLUME_LOW] = get_stock_pixbuf("audio-volume-low", size);
		status_icons[VOLUME_MEDIUM] = get_stock_pixbuf("audio-volume-medium",
					      size);
		status_icons[VOLUME_HIGH] = get_stock_pixbuf("audio-volume-high",
					    size);
		/* 'audio-volume-off' is not available in every icon set. More info at:
		 * http://standards.freedesktop.org/icon-naming-spec/icon-naming-spec-latest.html
		 */
		if (status_icons[VOLUME_OFF] == NULL)
			status_icons[VOLUME_OFF] = get_stock_pixbuf("audio-volume-low",
						   size);
	} else {
		status_icons[VOLUME_MUTED] = create_pixbuf("pnmixer-muted.png");
		status_icons[VOLUME_OFF] = create_pixbuf("pnmixer-off.png");
		status_icons[VOLUME_LOW] = create_pixbuf("pnmixer-low.png");
		status_icons[VOLUME_MEDIUM] = create_pixbuf("pnmixer-medium.png");
		status_icons[VOLUME_HIGH] = create_pixbuf("pnmixer-high.png");
	}

	/* Handle volume meter */
	icon_width = gdk_pixbuf_get_height(status_icons[0]);
	vol_div_factor = ((gdk_pixbuf_get_height(status_icons[0]) - 10) / 100.0);
	vol_meter_width = 1.25 * icon_width;
	if (vol_meter_width % 4 != 0)
		vol_meter_width -= (vol_meter_width % 4);

	if (prefs_get_boolean("DrawVolMeter", FALSE)) {
		int lim;

		if (vol_meter_row)
			g_free(vol_meter_row);
		vol_meter_row = g_malloc(vol_meter_width * sizeof(guchar));

		lim = vol_meter_width / 4;
		for (i = 0; i < lim; i++) {
			vol_meter_row[i * 4] = vol_meter_red;
			vol_meter_row[i * 4 + 1] = vol_meter_green;
			vol_meter_row[i * 4 + 2] = vol_meter_blue;
			vol_meter_row[i * 4 + 3] = 255;
		}
	} else {
		if (vol_meter_row)
			g_free(vol_meter_row);
		vol_meter_row = NULL;
		if (icon_copy)
			g_object_unref(icon_copy);
		icon_copy = NULL;
	}

	draw_offset = prefs_get_integer("VolMeterPos", 0);

	if (tray_icon)
		on_volume_has_changed();

	for (i = 0; i < N_VOLUME_ICONS; i++)
		if (old_icons[i])
			g_object_unref(old_icons[i]);
}

static gboolean version = FALSE;
static GOptionEntry args[] = {
	{
		"version", 0, 0, G_OPTION_ARG_NONE, &version, "Show version and exit",
		NULL
	},
	{
		"debug", 'd', 0, G_OPTION_ARG_NONE, &want_debug, "Run in debug mode",
		NULL
	},
	{NULL, 0, 0, 0, NULL, NULL, NULL}
};

/**
 * Program entry point. Initializes gtk+, calls the widget creating
 * functions and starts the main loop. Also connects 'popup-menu',
 * 'activate' and 'button-release-event' to the tray_icon.
 *
 * @param argc count of arguments
 * @param argv string array of arguments
 * @return 0 for success, otherwise error code
 */
int
main(int argc, char *argv[])
{
	GError *error = NULL;
	GOptionContext *context;
	want_debug = FALSE;

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif

	DEBUG_PRINT("[Debugging Mode Build]\n");

	setlocale(LC_ALL, "");
	context = g_option_context_new(_("- A mixer for the system tray."));
	g_option_context_add_main_entries(context, args, GETTEXT_PACKAGE);
	g_option_context_add_group(context, gtk_get_option_group(TRUE));
	g_option_context_parse(context, &argc, &argv, &error);
	gtk_init(&argc, &argv);

	g_option_context_free(context);

	if (version) {
		printf(_("%s version: %s\n"), PACKAGE, VERSION);
		exit(0);
	}

	popup_window = NULL;

	add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");
	add_pixmap_directory("./data/pixmaps");

	prefs_ensure_save_dir();
	prefs_load();
	cards = NULL;		// so we don't try and free on first run
	alsa_init();
	init_libnotify();

	// Popups
	popup_menu = popup_menu_create();
	popup_window = popup_window_create();


	hotkeys_add_filter();

	// Tray icon
	tray_icon = create_tray_icon();

	g_signal_connect(G_OBJECT(tray_icon), "popup-menu",
			 G_CALLBACK(popup_callback), popup_menu->menu);
	g_signal_connect(G_OBJECT(tray_icon), "activate",
			 G_CALLBACK(tray_icon_on_click), NULL);
	g_signal_connect(G_OBJECT(tray_icon), "button-release-event",
			 G_CALLBACK(tray_icon_button), NULL);

	apply_prefs(0);


	// Update levels
	get_current_levels();


	gtk_main();

	popup_window_destroy(popup_window);
	popup_menu_destroy(popup_menu);

	uninit_libnotify();
	alsa_close();
	return 0;
}
