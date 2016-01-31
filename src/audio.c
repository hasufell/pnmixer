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

#if 0
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

const char *
audio_get_card(void)
{
	return (alsa_get_active_card())->name;
}

const char *
audio_get_channel(void)
{
	return alsa_get_active_channel();
}


#else

#include "prefs.h"
#include "alsa-card.h"

static AlsaCard *soundcard;

int
audio_is_muted(void)
{
	return alsa_card_is_muted(soundcard);
}

void
audio_mute(gboolean notify)
{
	alsa_card_toggle_mute(soundcard);

	// TODO: remove that
	do_update_ui();
}

int
audio_get_volume(void)
{
	return alsa_card_get_volume(soundcard);
}

void
audio_set_volume(int volume, gboolean notify)
{
	alsa_card_set_volume(soundcard, volume, 0);

	if (alsa_card_is_muted(soundcard))
		alsa_card_toggle_mute(soundcard);

	// TODO: remove that
	do_update_ui();
}

void
audio_lower_volume(gboolean notify)
{
	int volume = alsa_card_get_volume(soundcard);

	alsa_card_set_volume(soundcard, volume - scroll_step, -1);

	if (alsa_card_is_muted(soundcard))
		alsa_card_toggle_mute(soundcard);

	// TODO: remove that
	do_update_ui();
}

void
audio_raise_volume(gboolean notify)
{
	int volume = alsa_card_get_volume(soundcard);

	alsa_card_set_volume(soundcard, volume + scroll_step, 1);

	if (alsa_card_is_muted(soundcard))
		alsa_card_toggle_mute(soundcard);

	// TODO: remove that
	do_update_ui();
}

const char *
audio_get_card(void)
{
	return alsa_card_get_name(soundcard);
}

const char *
audio_get_channel(void)
{
	return alsa_card_get_channel(soundcard);
}

GSList *
audio_get_card_list(void)
{
	return alsa_list_cards();
}

GSList *
audio_get_channel_list(const char *card_name)
{
	return alsa_list_channels(card_name);
}

void
audio_init(void)
{
	char *card_name;
	gboolean normalize;
	AlsaCard *card;
	GSList *card_list, *item;

	/* Get preferences */
	card_name = prefs_get_string("AlsaCard", NULL);
	normalize = prefs_get_boolean("NormalizeVolume", TRUE);

	/* Attempt to create the card */
	card = alsa_card_new_from_name(normalize, card_name);
	if (card)
		goto end;

	/* On failure, try to create the card from the list of available cards.
	 * We don't try the card name that just failed. 
	 */
	// TODO: tester
	card_list = alsa_list_cards();
	item = g_slist_find_custom(card_list, card_name, (GCompareFunc) g_strcmp0);
	if (item) {
		// TODO: degager ca
		DEBUG("Removing '%s' from list", (char *) item->data);
		card_list = g_slist_remove(card_list, item);
		g_slist_free_full(item, g_free);
	}

	card = alsa_card_new_from_list(normalize, card_list);

	g_slist_free_full(card_list, g_free);

end:
	g_free(card_name);

	/* If there's no card, let's suicide */
	soundcard = card;
	g_assert(soundcard);
}

void
audio_cleanup(void)
{
	if (soundcard == NULL)
		return;

	alsa_card_free(soundcard);
	soundcard = NULL;
}

void
audio_reinit(void)
{
	audio_cleanup();
	audio_init();

	// TODO: keep that ?
	do_update_ui();
}


#endif
