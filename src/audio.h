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

#ifndef AUDIO_H_
#define AUDIO_H_

#include <glib.h>

enum audio_signal {
	AUDIO_CARD_INITIALIZED,
	AUDIO_CARD_CLEANED_UP,
	AUDIO_VALUES_CHANGED,
	AUDIO_N_SIGNALS
};

typedef enum audio_signal AudioSignal;

typedef void (*AudioCallback) (gpointer);
typedef void (*AudioCardChangedCallback) (gpointer);
typedef void (*AudioValuesChangedCallback) (gpointer, gboolean, gdouble);

#define AUDIO_CALLBACK(func) ((AudioCallback) func)

	







void audio_init(void);
void audio_cleanup(void);
void audio_reinit(void);

gdouble audio_get_volume(void);
void audio_set_volume(gdouble volume);
void audio_lower_volume(void);
void audio_raise_volume(void);

gboolean audio_is_muted(void);
void audio_toggle_mute(void);

const char *audio_get_card(void);
const char *audio_get_channel(void);

GSList *audio_get_card_list(void);
GSList *audio_get_channel_list(const char *card);

#endif				// AUDIO_H
