/* audio.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file audio.h
 * Header for audio.c holding public functions.
 * @brief header for audio.c
 */

#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <glib.h>

/* High-level audio functions, no need to have
 * a soundcard initialized to call that.
 */

GSList *audio_get_card_list(void);
GSList *audio_get_channel_list(const char *card);

typedef struct audio Audio;

/* Soundcard management.
 */

Audio *audio_new(void);
void audio_free(Audio *audio);
void audio_reload_prefs(Audio *audio);
void audio_unhook_soundcard(Audio *audio);
void audio_hook_soundcard(Audio *audio);
const char *audio_get_card(Audio *audio);
const char *audio_get_channel(Audio *audio);

/* Mute and volume handling.
 * Everyone who changes the volume must declare who he is.
 */

enum audio_user {
	AUDIO_USER_UNKNOWN,
	AUDIO_USER_POPUP,
	AUDIO_USER_TRAY_ICON,
	AUDIO_USER_HOTKEYS,
};

typedef enum audio_user AudioUser;

gboolean audio_is_muted(Audio *audio);
void audio_toggle_mute(Audio *audio, AudioUser user);
gdouble audio_get_volume(Audio *audio);
void audio_set_volume(Audio *audio, AudioUser user, gdouble volume);
void audio_lower_volume(Audio *audio, AudioUser user);
void audio_raise_volume(Audio *audio, AudioUser user);

/* Signals handling.
 * The audio system sends signals out there when something happens.
 */

enum audio_signal {
	AUDIO_NO_CARD,
	AUDIO_CARD_INITIALIZED,
	AUDIO_CARD_CLEANED_UP,
	AUDIO_CARD_DISCONNECTED,
	AUDIO_CARD_ERROR,
	AUDIO_VALUES_CHANGED,
};

typedef enum audio_signal AudioSignal;

struct audio_event {
	AudioSignal signal;
	AudioUser user;
	gchar *card;
	gchar *channel;
	gboolean muted;
	gdouble volume;
};

typedef struct audio_event AudioEvent;

typedef void (*AudioCallback) (Audio *audio, AudioEvent *event, gpointer data);

void audio_signals_connect(Audio *audio, AudioCallback callback, gpointer data);
 /* Never forget to call that, otherwise it will segfault.
  * This is unlike Glib signal handling, where disconnecting
  * can be omitted. This works thanks to ref counting.
  * //TODO: make a better comment.
  */
void audio_signals_disconnect(Audio *audio, AudioCallback callback, gpointer data);

#endif				// _AUDIO_H
