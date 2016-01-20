/* audio.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file audio.c
 * This file holds the audio related code.
 * It is a middleman between the low-level alsa code, and the
 * high-level ui code.
 * @brief Audio subsystem
 */

#include "alsa.h"
#include "main.h"

void
audio_init(void)
{
	alsa_init();
}

void
audio_cleanup(void)
{
	alsa_close();
}

void
audio_reinit(void)
{
	alsa_init();

	do_update_ui();
}

int
audio_is_muted(void)
{
	return ismuted();
}

void
audio_mute(gboolean notify)
{
        setmute(notify);

        do_update_ui();
}

int
audio_get_volume(void)
{
	return getvol();
}

void
audio_raise_volume(gboolean notify)
{
	int volume = getvol();

	setvol(volume + scroll_step, 1, notify);

	if (ismuted() == 0)
		setmute(notify);

	do_update_ui();
}

void
audio_lower_volume(gboolean notify)
{
	int volume = getvol();

	setvol(volume - scroll_step, -1, notify);

	if (ismuted() == 0)
		setmute(notify);

	do_update_ui();
}

void
audio_set_volume(int volume, gboolean notify)
{
	setvol(volume, 0, notify);

	if (ismuted() == 0)
		setmute(notify);

	do_update_ui();
}

const char*
audio_get_card(void)
{
	return (alsa_get_active_card())->name;
}

const char*
audio_get_channel(void)
{
	return alsa_get_active_channel();
}
