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

#include "audio.h"
#include "alsa.h"
#include "prefs.h"
#include "support.h"

#include "main.h"

static AlsaCard *soundcard;
static int scroll_step;

/*
 * Audio Closures.
 * An Audio Closure represents a callback and the data pointer associated.
 */

struct audio_closure {
	AudioCallback callback;
	gpointer data;
};

typedef struct audio_closure AudioClosure;

static void
audio_closure_free(AudioClosure *closure)
{
	g_free(closure);
}

static AudioClosure *
audio_closure_new(AudioCallback callback, gpointer data)
{
	AudioClosure *closure;

	closure = g_new0(AudioClosure, 1);
	closure->callback = callback;
	closure->data = data;
}

/*
 * Audio Signals
 */
static GSList *signal_handlers[AUDIO_N_SIGNALS];

static void
invoke_card_changed_handlers(GSList *handlers)
{
	GSList *list;

	DEBUG("Invoking card changed handlers");

	for (; list; list = list->next) {
		AudioClosure *closure = list->data;
		AudioCardChangedCallback callback = closure->callback;
		callback(closure->data);
	}
}

static void
invoke_values_changed_handlers(gboolean mute, gdouble volume)
{
	GSList *list = signal_handlers[AUDIO_VALUES_CHANGED];

	DEBUG("Invoking values changed handlers");

	for (; list; list = list->next) {
		AudioClosure *closure = list->data;
		AudioValuesChangedCallback callback =
			(AudioValuesChangedCallback) closure->callback;
		callback(closure->data, mute, volume);
	}
}

void
audio_signal_disconnect(AudioSignal signal, AudioCallback callback)
{
	GSList *item;
	GSList **list_ref;

	list_ref = &signal_handlers[signal];

	// TODO: tester ca
	for (item = *list_ref; item; item = item->next) {
		AudioClosure *closure = item->data;
		if (closure->callback == callback) {
			*list_ref = g_slist_remove_link(*list_ref, item);
			g_slist_free_full(item, (GDestroyNotify) audio_closure_free);
			break;
		}
	}

	if (item == NULL)
		WARN("Signal handler to disconnect wasn't found");
}

void
audio_signal_connect(AudioSignal signal, AudioCallback callback, gpointer data)
{
	AudioClosure *closure;
	GSList *list;

	closure = audio_closure_new(callback, data);
	list = signal_handlers[signal];
	list = g_slist_append(list, closure);
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

gboolean
audio_is_muted(void)
{
	return alsa_card_is_muted(soundcard);
}

void
audio_toggle_mute(void)
{
	alsa_card_toggle_mute(soundcard);
}

gdouble
audio_get_volume(void)
{
	return alsa_card_get_volume(soundcard);
}

void
audio_set_volume(gdouble volume)
{
	alsa_card_set_volume(soundcard, volume, 0);

	if (alsa_card_is_muted(soundcard))
		alsa_card_toggle_mute(soundcard);
}

void
audio_lower_volume(void)
{
	gdouble volume;

	volume = alsa_card_get_volume(soundcard);
	alsa_card_set_volume(soundcard, volume - scroll_step, -1);

	if (alsa_card_is_muted(soundcard))
		alsa_card_toggle_mute(soundcard);
}

void
audio_raise_volume(void)
{
	gdouble volume;

	volume = alsa_card_get_volume(soundcard);
	alsa_card_set_volume(soundcard, volume + scroll_step, 1);

	if (alsa_card_is_muted(soundcard))
		alsa_card_toggle_mute(soundcard);
}

void
audio_init(void)
{
	char *card_name;
	gboolean normalize;
	AlsaCard *card;
	GSList *card_list, *item;

	/* Set the global var for scroll step */
	scroll_step = prefs_get_double("ScrollStep", 5);

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

	alsa_card_set_values_changed_callback(card, invoke_values_changed_handlers);
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

	// TODO: remove that ?
	do_update_ui();
}
