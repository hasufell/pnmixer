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

void audio_init(void);
void audio_cleanup(void);
void audio_reinit(void);

int audio_is_muted(void);
void audio_mute(gboolean notify);

int audio_get_volume(void);
void audio_raise_volume(gboolean notify);
void audio_lower_volume(gboolean notify);
void audio_set_volume(int volume, gboolean notify);

const char *audio_get_card(void);
const char *audio_get_channel(void);

#endif				// AUDIO_H
