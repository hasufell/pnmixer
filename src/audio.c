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
#include "support-log.h"

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

	return closure;
}

/*
 * Audio Actions.
 * An audio action is created each time an user touches something in the
 * audio system. It's mainly volume change, but it can also be card changing.
 */

struct audio_action {
	AudioUser user;
	gint64 time;
};

typedef struct audio_action AudioAction;


static gboolean
audio_action_is_still_valid(AudioAction *action)
{
	gint64 now, last, delay;

	now = g_get_monotonic_time();
	last = action->time;
	delay = now - last;

	//DEBUG("delay: %ld", delay);
	
	/* Action is still valid if it happens less than a second ago.
	 * This is probably far too much, but if it doesn't hurt, let
	 * it be like that.
	 * I personnally witnessed delay up to 600.
	 */
	return delay < 1000000 ? TRUE : FALSE;
}

static void
audio_action_free(AudioAction *action)
{
	g_free(action);
}

static AudioAction *
audio_action_new(AudioUser user)
{
	AudioAction *action;

	action = g_new0(AudioAction, 1);
	action->user = user;
	action->time = g_get_monotonic_time();

	return action;
}



/*
 * Audio Signals
 */

struct audio {
	gdouble scroll_step;
	AlsaCard *soundcard;
	AudioAction *last_action;
	// TODO: find a better name ?
	GSList *handlers;
};

static void
invoke_handlers(Audio *audio, AudioSignal signal)
{
	AudioAction *last_action = audio->last_action;
	AudioEvent *event;
	GSList *item;

	/* Create the new event.
	 * Be careful, the soundcard may not exist anymore.
	 * That's why we use the 'audio_*' wrappers to access the soundcard.
	 */
	event = g_new0(AudioEvent, 1);
	event->signal = signal;
	event->user = AUDIO_USER_UNKNOWN;
	event->card = g_strdup(audio_get_card(audio));
	event->channel = g_strdup(audio_get_channel(audio));
	event->muted = audio_is_muted(audio);
	event->volume = audio_get_volume(audio);

	/* Check who is at the origin of this event */
	if (last_action) {
		/* Get user if the last action is still valid */
		if (audio_action_is_still_valid(last_action))
			event->user = last_action->user;
		else
			WARN("Discarding last action, too old");

		/* Consume */
		audio_action_free(last_action);
		audio->last_action = NULL;
	}

	/* Send that to various handlers around */
	for (item = audio->handlers; item; item = item->next) {
		AudioClosure *closure = item->data;
		closure->callback(audio, event, closure->data);
	}

	/* Then free the event */
	g_free(event->card);
	g_free(event->channel);
	g_free(event);
}

static void
on_alsa_event(enum alsa_event event, gpointer data)
{
	Audio *audio = (Audio *) data;

	switch (event) {
	case ALSA_CARD_ERROR:
		invoke_handlers(audio, AUDIO_CARD_ERROR);
		break;
	case ALSA_CARD_DISCONNECTED:
		invoke_handlers(audio, AUDIO_CARD_DISCONNECTED);
		break;
	case ALSA_CARD_VALUES_CHANGED:
		invoke_handlers(audio, AUDIO_VALUES_CHANGED);
		break;
	default:
		WARN("Unhandled alsa event: %d", event);
	}
}

void
audio_signals_disconnect(Audio *audio, AudioCallback callback, gpointer data)
{
	GSList *item;

	for (item = audio->handlers; item; item = item->next) {
		AudioClosure *closure = item->data;
		if (closure->data == data && closure->callback == callback)
			break;
	}

	if (item == NULL) {
		WARN("Signal handler to disconnect wasn't found");
		return;
	}

	audio->handlers = g_slist_remove_link(audio->handlers, item);
	g_slist_free_full(item, (GDestroyNotify) audio_closure_free);
}

void
audio_signals_connect(Audio *audio, AudioCallback callback, gpointer data)
{
	AudioClosure *closure;

	closure = audio_closure_new(callback, data);
	audio->handlers = g_slist_append(audio->handlers, closure);
}

const char *
audio_get_card(Audio *audio)
{
	AlsaCard *soundcard = audio->soundcard;

	if (!soundcard)
		return "";

	return alsa_card_get_name(soundcard);
}

const char *
audio_get_channel(Audio *audio)
{
	AlsaCard *soundcard = audio->soundcard;

	if (!soundcard)
		return "";

	return alsa_card_get_channel(soundcard);
}

gboolean
audio_is_muted(Audio *audio)
{
	AlsaCard *soundcard = audio->soundcard;

	if (!soundcard)
		return TRUE;

	return alsa_card_is_muted(soundcard);
}

void
audio_toggle_mute(Audio *audio, AudioUser user)
{
	AlsaCard *soundcard = audio->soundcard;

	/* Discard if no soundcard available */
	if (!soundcard)
		return;

	/* Create a new action and add it to the actions list */
	audio_action_free(audio->last_action);
	audio->last_action = audio_action_new(user);

	/* Now do the job */
	alsa_card_toggle_mute(soundcard);
}

gdouble
audio_get_volume(Audio *audio)
{
	AlsaCard *soundcard = audio->soundcard;

	if (!soundcard)
		return 0;

	return alsa_card_get_volume(soundcard);
}

static void
_audio_set_volume(Audio *audio, AudioUser user, gdouble volume, gint dir)
{
	AlsaCard *soundcard = audio->soundcard;

	/* Discard if no soundcard available */
	if (!soundcard)
		return;

	/* Create a new action and add it to the actions list */
	audio_action_free(audio->last_action);
	audio->last_action = audio_action_new(user);

	/* Now do the job */
	alsa_card_set_volume(soundcard, volume, dir);
	if (alsa_card_is_muted(soundcard))
		alsa_card_toggle_mute(soundcard);
}

void
audio_set_volume(Audio *audio, AudioUser user, gdouble volume)
{
	_audio_set_volume(audio, user, volume, 0);
}

void
audio_lower_volume(Audio *audio, AudioUser user)
{
	AlsaCard *soundcard = audio->soundcard;
	gdouble scroll_step = audio->scroll_step;
	gdouble volume;

	volume = alsa_card_get_volume(soundcard);
	volume -= scroll_step;
	_audio_set_volume(audio, user, volume, -1);
}

void
audio_raise_volume(Audio *audio, AudioUser user)
{
	AlsaCard *soundcard = audio->soundcard;
	gdouble scroll_step = audio->scroll_step;
	gdouble volume;

	volume = alsa_card_get_volume(soundcard);
	volume += scroll_step;
	_audio_set_volume(audio, user, volume, +1);
}

void
audio_unhook_soundcard(Audio *audio)
{
	DEBUG("Unhooking soundcard from the audio system");

	g_assert(audio->soundcard);

	/* Free the soundcard */
	alsa_card_free(audio->soundcard);
	audio->soundcard = NULL;

	/* Invoke user handlers */
	invoke_handlers(audio, AUDIO_CARD_CLEANED_UP);
}

void
audio_hook_soundcard(Audio *audio)
{
	char *card_name;
	gboolean normalize;
	AlsaCard *card;
	GSList *card_list, *item;

	DEBUG("Hooking soundcard to the audio system");

	g_assert(audio->soundcard == NULL);

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

	/* If there's no card, just tell it ! */
	audio->soundcard = card;
	if (!audio->soundcard) {
		invoke_handlers(audio, AUDIO_NO_CARD);
		return;
	}

	/* Install callbacks */
	alsa_card_install_callback(card, on_alsa_event, audio);

	/* Tell the world */
	invoke_handlers(audio, AUDIO_CARD_INITIALIZED);
}

void
audio_reload_prefs(Audio *audio)
{
	audio->scroll_step = prefs_get_double("ScrollStep", 5);
}

void
audio_free(Audio *audio)
{
	if (audio == NULL)
		return;

	audio_unhook_soundcard(audio);
	g_free(audio);
}

Audio *
audio_new(void)
{
	Audio *audio;

	audio = g_new0(Audio, 1);
	audio_reload_prefs(audio);

	return audio;
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

