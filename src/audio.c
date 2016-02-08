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

#include <glib.h>

#include "audio.h"
#include "alsa.h"
#include "prefs.h"
#include "support-log.h"

/*
 * Audio Actions.
 * An audio action is created each time the volume/mute is changed.
 * It contains the user who made the action, and a timestamp.
 * Each volume/mute change will trigger a callback afterward.
 * In this callback, we look for the last audio action that happens,
 * validate its timestamp, then notify the system that a change made
 * by 'user' has been made.
 *
 * For this to work, we assume that each volume/mute change will be
 * followed by one callback invocation. The callback consumes the
 * audio action. This seems to work until now.
 */

struct audio_action {
	AudioUser user;
	gint64 time;
};

typedef struct audio_action AudioAction;

/* Decide if the action is still valid, that's to say,
 * if the timestamp is not too old.
 */
static gboolean
audio_action_is_still_valid(AudioAction *action)
{
	gint64 now, last, delay;

	now = g_get_monotonic_time();
	last = action->time;
	delay = now - last;

	//DEBUG("delay: %ld", delay);

	/* Maximum delay for an action is set to 1 second.
	 * This is probably too much, but it doesn't hurt.
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
 * Audio Event.
 * An audio event is a struct that contains the current audio status.
 * It's passed in parameters of the user signal handlers, so that
 * user doesn't have to query the audio system to know about it.
 * It make things a little more efficient.
 */

static void
audio_event_free(AudioEvent *event)
{
	if (event == NULL)
		return;

	g_free(event);
}

static AudioEvent *
audio_event_new(Audio *audio, AudioSignal signal, AudioUser user)
{
	AudioEvent *event;

	/* At the moment there's no need to duplicate card/channel 
	 * name strings, so let's optimize a very little and make
	 * them const pointers.
	 */

	event = g_new0(AudioEvent, 1);

	event->signal = signal;
	event->user = user;
	event->card = audio_get_card(audio);
	event->channel = audio_get_channel(audio);
	event->muted = audio_is_muted(audio);
	event->volume = audio_get_volume(audio);

	return event;
}

/*
 * Audio Signal Handlers.
 * An audio signal handler is made of a callback and a data pointer.
 * It's the basic type that holds user signal handlers.
 */

struct audio_handler {
	AudioCallback callback;
	gpointer data;
};

typedef struct audio_handler AudioHandler;

static void
audio_handler_free(AudioHandler *handler)
{
	g_free(handler);
}

static AudioHandler *
audio_handler_new(AudioCallback callback, gpointer data)
{
	AudioHandler *handler;

	handler = g_new0(AudioHandler, 1);
	handler->callback = callback;
	handler->data = data;

	return handler;
}

static gint
audio_handler_cmp(AudioHandler *h1, AudioHandler *h2)
{
	if (h1->callback == h2->callback && h1->data == h2->data)
		return 0;
	return -1;
}

static GSList *
audio_handler_list_append(GSList *list, AudioHandler *handler)
{
	GSList *item;
	
	/* Ensure that the handler is not already part of the list.
	 * It's probably an error to have a duplicated handler.
	 */
	item = g_slist_find_custom(list, handler, (GCompareFunc) audio_handler_cmp);
	if (item) {
		WARN("Audio handler already in the list");
		return list;
	}

	/* Append the handler to the list */
	return g_slist_append(list, handler);
}

static GSList *
audio_handler_list_remove(GSList *list, AudioHandler *handler)
{
	GSList *item;

	/* find the handler */
	item = g_slist_find_custom(list, handler, (GCompareFunc) audio_handler_cmp);
	if (item == NULL) {
		WARN("Audio handler wasn't found in the list");
		return list;
	}

	/* Remove the handler from the list.
	 * We assume there's only one such handler.
	 */
	list = g_slist_remove_link(list, item);
	g_slist_free_full(item, (GDestroyNotify) audio_handler_free);
	return list;
}

/*
 * Public functions & signals handlers
 */

struct audio {
	/* Preferences */
	gchar *card;
	gchar *channel;
	gdouble scroll_step;
	gboolean normalize;
	/* Underlying sound system */
	AlsaCard *soundcard;
	/* Last action performed */
	AudioAction *last_action;
	/* User signal handlers */
	GSList *handlers;
};

static void
invoke_handlers(Audio *audio, AudioSignal signal)
{
	AudioAction *last_action = audio->last_action;
	AudioEvent *event;
	GSList *item;

	/* Create a new event */
	event = audio_event_new(audio, signal, AUDIO_USER_UNKNOWN);

	/* Check if we're at the origin of this event. In this case,
	 * a 'last_action' have been created beforehand.
	 */
	if (last_action) {
		if (audio_action_is_still_valid(last_action))
			/* Alright, we know who performed the action */
			event->user = last_action->user;
		else
			/* If we find an action that's too old, it probably means
			 * that somehow, a callback was never invocated.
			 * This shouldn't happen, so let's warn about it.
			 */
			WARN("Discarding last action, too old");

		/* Consume the action */
		audio_action_free(last_action);
		audio->last_action = NULL;
	}

	/* Invoke the various handlers around */
	for (item = audio->handlers; item; item = item->next) {
		AudioHandler *handler = item->data;
		handler->callback(audio, event, handler->data);
	}

	/* Then free the event */
	audio_event_free(event);
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

/* Disconnect a signal handler designed by 'callback' and 'data'. */
void
audio_signals_disconnect(Audio *audio, AudioCallback callback, gpointer data)
{
	AudioHandler *handler;

	handler = audio_handler_new(callback, data);
	audio->handlers = audio_handler_list_remove(audio->handlers, handler);
	audio_handler_free(handler);
}

/* Connect a signal handler designed by 'callback' and 'data'.
 * Remember to always pair 'connect' calls with 'disconnect' calls,
 * otherwise you'll be in trouble.
 */
void
audio_signals_connect(Audio *audio, AudioCallback callback, gpointer data)
{
	AudioHandler *handler;

	handler = audio_handler_new(callback, data);
	audio->handlers = audio_handler_list_append(audio->handlers, handler);
}

const char *
audio_get_card(Audio *audio)
{
	return audio->card;
}

const char *
audio_get_channel(Audio *audio)
{
	return audio->channel;
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

	/* Create a new action */
	audio_action_free(audio->last_action);
	audio->last_action = audio_action_new(user);

	/* Do the job */
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

	/* Create a new action */
	audio_action_free(audio->last_action);
	audio->last_action = audio_action_new(user);

	/* Do the job */
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

static void
audio_unhook_soundcard(Audio *audio)
{
	if (audio->soundcard == NULL)
		return;

	DEBUG("Unhooking soundcard from the audio system");

	/* Free the soundcard */
	alsa_card_free(audio->soundcard);
	audio->soundcard = NULL;

	/* Invoke user handlers */
	invoke_handlers(audio, AUDIO_CARD_CLEANED_UP);
}

static void
audio_hook_soundcard(Audio *audio)
{
	AlsaCard *soundcard;
	GSList *card_list, *item;

	g_assert(audio->soundcard == NULL);

	DEBUG("Hooking soundcard to the audio system");

	/* Attempt to create the card */
	soundcard = alsa_card_new(audio->card, audio->channel, audio->normalize);
	if (soundcard)
		goto end;

	/* On failure, try to create the card from the list of available cards.
	 * We don't try the card name that just failed. 
	 */
	card_list = alsa_list_cards();
	item = g_slist_find_custom(card_list, audio->card, (GCompareFunc) g_strcmp0);
	if (item) {
		DEBUG("Removing '%s' from card list", (char *) item->data);
		card_list = g_slist_remove(card_list, item);
		g_slist_free_full(item, g_free);
	}

	/* Now iterate on card list and attempt to get a working soundcard */
	for (item = card_list; item; item = item->next) {
		const char *card = item->data;
		char *channel;

		channel = prefs_get_channel(card);
		soundcard = alsa_card_new(card, channel, audio->normalize);
		g_free(channel);

		if (soundcard)
			break;
	}

	/* Free card list */
	g_slist_free_full(card_list, g_free);

end:
	/* Save soundcard NOW !
	 * We're going to invoke handlers later on, and these guys
	 * need a valid soundcard pointer.
	 */
	audio->soundcard = soundcard;

	/* Finish making everything ready */
	if (soundcard == NULL) {
		/* Card and channel names set to emptry string */
		g_free(audio->card);
		audio->card = g_strdup("");
		g_free(audio->channel);
		audio->channel = g_strdup("");

		/* Tell the world */
		invoke_handlers(audio, AUDIO_NO_CARD);
	} else {
		/* Card and channel names must match the truth.
		 * Indeed, in case of failure, we may end up using a soundcard
		 * different from the one specified in the preferences.
		 */
		g_free(audio->card);
		audio->card = g_strdup(alsa_card_get_name(soundcard));
		g_free(audio->channel);
		audio->channel = g_strdup(alsa_card_get_channel(soundcard));

		/* Install callbacks */
		alsa_card_install_callback(soundcard, on_alsa_event, audio);

		/* Tell the world */
		invoke_handlers(audio, AUDIO_CARD_INITIALIZED);
	}
}

void
audio_reload(Audio *audio)
{
	/* Get preferences */
	g_free(audio->card);
	audio->card = prefs_get_string("AlsaCard", NULL);
	g_free(audio->channel);
	audio->channel = prefs_get_channel(audio->card);
	audio->normalize = prefs_get_boolean("NormalizeVolume", TRUE);
	audio->scroll_step = prefs_get_double("ScrollStep", 5);

	/* Rehook soundcard */
	audio_unhook_soundcard(audio);
	audio_hook_soundcard(audio);
}

void
audio_free(Audio *audio)
{
	if (audio == NULL)
		return;

	audio_unhook_soundcard(audio);
	g_free(audio->channel);
	g_free(audio->card);
	g_free(audio);
}

Audio *
audio_new(void)
{
	Audio *audio;

	audio = g_new0(Audio, 1);

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

