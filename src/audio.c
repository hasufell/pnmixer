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
 * It is a middleman between the low-level audio backend (alsa),
 * and the high-level ui code.
 * This abstraction layer allows the high-level code to be completely
 * unaware of the underlying audio implementation, may it be alsa or whatever.
 * @brief Audio subsystem.
 */

#include <glib.h>

#include "audio.h"
#include "alsa.h"
#include "prefs.h"
#include "support-log.h"

/*
 * Enumeration to string, for friendly debug messages.
 */

static const gchar *
audio_user_to_str(AudioUser user)
{
	switch (user) {
	case AUDIO_USER_POPUP:
		return "popup";
	case AUDIO_USER_TRAY_ICON:
		return "tray icon";
	case AUDIO_USER_HOTKEYS:
		return "hotkeys";
	default:
		return "unknown";
	}
}

static const gchar *
audio_signal_to_str(AudioSignal signal)
{
	switch (signal) {
	case AUDIO_NO_CARD:
		return "no card";
	case AUDIO_CARD_INITIALIZED:
		return "card initialized";
	case AUDIO_CARD_CLEANED_UP:
		return "card cleaned up";
	case AUDIO_CARD_DISCONNECTED:
		return "card disconnected";
	case AUDIO_CARD_ERROR:
		return "card error";
	case AUDIO_VALUES_CHANGED:
		return "values changed";
	default:
		return "unknown";
	}
}

/*
 * Audio Actions.
 * An audio action is created each time the volume/mute is changed.
 * It contains the user who made the action, and a timestamp.
 * It is recorded, and will be user later on, since every volume/mute
 * change will trigger a callback afterward. This callback will deal
 * with this 'audio action'.
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

	/* Ensure the delay is not too big. The delay here is the time
	 * between the action was performed and the moment the callback
	 * is invoked. It is supposed to be very quick, a matter of
	 * milliseconds.
	 * We set the maximum delay to 1 second, it's probably far too much.
	 * However it doesn't hurt to set it too long. Setting it too short
	 * could hurt.
	 */
	return delay < 1000000 ? TRUE : FALSE;
}

/* Free an audio action */
static void
audio_action_free(AudioAction *action)
{
	g_free(action);
}

/* Create a new audio action */
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

/* Free an audio event */
static void
audio_event_free(AudioEvent *event)
{
	if (event == NULL)
		return;

	g_free(event);
}

/* Create a new audio event */
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

/* Free an audio handler */
static void
audio_handler_free(AudioHandler *handler)
{
	g_free(handler);
}

/* Create a new audio handler */
static AudioHandler *
audio_handler_new(AudioCallback callback, gpointer data)
{
	AudioHandler *handler;

	handler = g_new0(AudioHandler, 1);
	handler->callback = callback;
	handler->data = data;

	return handler;
}

/* Check if two handlers are the identical. We compare the content
 * of the structures, not the pointers.
 */
static gint
audio_handler_cmp(AudioHandler *h1, AudioHandler *h2)
{
	if (h1->callback == h2->callback && h1->data == h2->data)
		return 0;
	return -1;
}

/* Add a handler to a handler list */
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

/* Remove a handler from a handler list */
static GSList *
audio_handler_list_remove(GSList *list, AudioHandler *handler)
{
	GSList *item;

	/* Find the handler */
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
	/* Underlying sound card */
	AlsaCard *soundcard;
	/* Last action performed */
	AudioAction *last_action;
	/* User signal handlers, to be invoked
	 * when the audio status changes.
	 */
	GSList *handlers;
};

/**
 * Convenient function to invoke the handlers
 *
 * @param audio an Audio instance.
 * @param signal the signal to dispatch.
 * @param user the user that made the action.
 */
static void
invoke_handlers(Audio *audio, AudioSignal signal, AudioUser user)
{
	AudioEvent *event;
	GSList *item;

	/* Nothing to do if there is no handlers */
	if (audio->handlers == NULL)
		return;

	/* Create a new event */
	event = audio_event_new(audio, signal, user);

	/* Invoke the various handlers around */
	DEBUG("** Dispatching signal '%s' from '%s', vol=%lg, muted=%s",
	      audio_signal_to_str(signal), audio_user_to_str(user),
	      event->volume, event->muted ? "yes" : "no");

	for (item = audio->handlers; item; item = item->next) {
		AudioHandler *handler = item->data;
		handler->callback(audio, event, handler->data);
	}

	/* Then free the event */
	audio_event_free(event);
}

/**
 * Callback invoked when an alsa event happens.
 *
 * @param event the event that happened.
 * @param data associated data.
 */
static void
on_alsa_event(enum alsa_event event, gpointer data)
{
	Audio *audio = (Audio *) data;
	AudioAction *last_action = audio->last_action;

	/* Check if we expected this event */
	if (last_action) {
		/* If we are responsible for this event (aka we changed
		 * the volume/mute state), there should be a 'last_action'
		 * defined and valid. We just need to consume it.
		 * There's no handler to invoke, we've done that already
		 * when we changed the vloume/mute.
		 */
		if (audio_action_is_still_valid(last_action)) {
			audio_action_free(last_action);
			audio->last_action = NULL;
			return;
		}

		/* In some situation we can find a pending 'last_action' that
		 * was never consumed. This happens for example when we try
		 * to raise the volume and it's already at its maximum value.
		 */
		DEBUG("Discarding last action, too old");
		audio_action_free(last_action);
		audio->last_action = NULL;
	}

	/* Here, we are not at the origin of this change.
	 * We must invoke the handlers.
	 */
	switch (event) {
	case ALSA_CARD_ERROR:
		invoke_handlers(audio, AUDIO_CARD_ERROR, AUDIO_USER_UNKNOWN);
		break;
	case ALSA_CARD_DISCONNECTED:
		invoke_handlers(audio, AUDIO_CARD_DISCONNECTED, AUDIO_USER_UNKNOWN);
		break;
	case ALSA_CARD_VALUES_CHANGED:
		invoke_handlers(audio, AUDIO_VALUES_CHANGED, AUDIO_USER_UNKNOWN);
		break;
	default:
		WARN("Unhandled alsa event: %d", event);
	}
}

/**
 * Disconnect a signal handler designed by 'callback' and 'data'.
 *
 * @param audio an Audio instance.
 * @param callback the callback to disconnect.
 * @param data the data to pass to the callback.
 */
void
audio_signals_disconnect(Audio *audio, AudioCallback callback, gpointer data)
{
	AudioHandler *handler;

	handler = audio_handler_new(callback, data);
	audio->handlers = audio_handler_list_remove(audio->handlers, handler);
	audio_handler_free(handler);
}

/**
 * Connect a signal handler designed by 'callback' and 'data'.
 * Remember to always pair 'connect' calls with 'disconnect' calls,
 * otherwise you'll be in trouble.
 *
 * @param audio an Audio instance.
 * @param callback the callback to connect.
 * @param data the data to pass to the callback.
 */
void
audio_signals_connect(Audio *audio, AudioCallback callback, gpointer data)
{
	AudioHandler *handler;

	handler = audio_handler_new(callback, data);
	audio->handlers = audio_handler_list_append(audio->handlers, handler);
}

/**
 * Get the name of the card currently hooked.
 * This is an internal string that shouldn't be modified.
 *
 * @param audio an Audio instance.
 * @return the name of the card.
 */
const char *
audio_get_card(Audio *audio)
{
	return audio->card;
}

/**
 * Get the name of the channel currently in use.
 * This is an internal string that shouldn't be modified.
 *
 * @param audio a Audio instance.
 * @return the name of the channel.
 */
const char *
audio_get_channel(Audio *audio)
{
	return audio->channel;
}

/**
 * Get the mute state, either TRUE or FALSE.
 *
 * @param audio an Audio instance.
 * @return TRUE if the card is muted, FALSE otherwise.
 */
gboolean
audio_is_muted(Audio *audio)
{
	AlsaCard *soundcard = audio->soundcard;

	if (!soundcard)
		return TRUE;

	return alsa_card_is_muted(soundcard);
}

/**
 * Toggle the mute state.
 *
 * @param audio an Audio instance.
 * @param user the user who performs the action.
 */
void
audio_toggle_mute(Audio *audio, AudioUser user)
{
	AlsaCard *soundcard = audio->soundcard;

	/* Discard if no soundcard available */
	if (!soundcard)
		return;

	/* Create a new action (the callback will dispose of it) */
	audio_action_free(audio->last_action);
	audio->last_action = audio_action_new(user);

	/* Toggle mute state */
	alsa_card_toggle_mute(soundcard);

	/* Invoke the handlers */
	invoke_handlers(audio, AUDIO_VALUES_CHANGED, user);
}

/**
 * Get the volume in percent (value between 0 and 100).
 *
 * @param audio an Audio instance.
 * @return the volume in percent.
 */
gdouble
audio_get_volume(Audio *audio)
{
	AlsaCard *soundcard = audio->soundcard;

	if (!soundcard)
		return 0;

	return alsa_card_get_volume(soundcard);
}

/**
 * Actually set the volume.
 *
 * @param audio an Audio instance.
 * @param user the user who performs the action.
 * @param volume the volume value to set, in percent.
 * @param dir the direction for the volume change
 *        (-1: lowering, +1: raising, 0: setting).
 */
static void
_audio_set_volume(Audio *audio, AudioUser user, gdouble volume, gint dir)
{
	AlsaCard *soundcard = audio->soundcard;

	/* Discard if no soundcard available */
	if (!soundcard)
		return;

	/* Create a new action (the callback will dispose of it) */
	audio_action_free(audio->last_action);
	audio->last_action = audio_action_new(user);

	/* Set the volume */
	DEBUG("Setting volume to %lg (dir:%d)", volume, dir);
	alsa_card_set_volume(soundcard, volume, dir);

	/* Automatically unmute the volume */
	if (alsa_card_is_muted(soundcard))
		alsa_card_toggle_mute(soundcard);

	/* Invoke the handlers */
	invoke_handlers(audio, AUDIO_VALUES_CHANGED, user);
}

/**
 * Set the volume.
 *
 * @param audio an Audio instance.
 * @param user the user who performs the action.
 * @param volume the volume value to set, in percent.
 */
void
audio_set_volume(Audio *audio, AudioUser user, gdouble volume)
{
	AlsaCard *soundcard = audio->soundcard;
	gdouble cur_volume;
	gint dir;

	cur_volume = alsa_card_get_volume(soundcard);

	if (cur_volume < volume)
		dir = +1;
	else if (cur_volume > volume)
		dir = -1;
	else
		dir = 0;

	_audio_set_volume(audio, user, volume, dir);
}

/**
 * Lower the volume.
 *
 * @param audio an Audio instance.
 * @param user the user who performs the action.
 */
void
audio_lower_volume(Audio *audio, AudioUser user)
{
	AlsaCard *soundcard = audio->soundcard;
	gdouble scroll_step = audio->scroll_step;
	gdouble volume;

	volume = alsa_card_get_volume(soundcard);
	volume -= scroll_step;
	if (volume < 0)
		volume = 0;
	_audio_set_volume(audio, user, volume, -1);
}

/**
 * Raise the volume.
 *
 * @param audio an Audio instance.
 * @param user the user who performs the action.
 */
void
audio_raise_volume(Audio *audio, AudioUser user)
{
	AlsaCard *soundcard = audio->soundcard;
	gdouble scroll_step = audio->scroll_step;
	gdouble volume;

	volume = alsa_card_get_volume(soundcard);
	volume += scroll_step;
	if (volume > 100)
		volume = 100;
	_audio_set_volume(audio, user, volume, +1);
}

/**
 * Unhook the currently hooked audio card.
 *
 * @param audio an Audio instance.
 */
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
	invoke_handlers(audio, AUDIO_CARD_CLEANED_UP, AUDIO_USER_UNKNOWN);
}

/**
 * Attempt to hook an audio soundcard.
 * Try everything possible, the goal is to have a working soundcard.
 * So if the selected soundcard fails, we try any others until at some
 * point we have a working soundcard.
 *
 * @param audio an Audio instance.
 */
static void
audio_hook_soundcard(Audio *audio)
{
	AlsaCard *soundcard;
	GSList *card_list, *item;

	g_assert(audio->soundcard == NULL);

	/* Attempt to create the card */
	DEBUG("Hooking soundcard '%s (%s)' to the audio system", audio->card, audio->channel);

	soundcard = alsa_card_new(audio->card, audio->channel, audio->normalize);
	if (soundcard)
		goto end;

	/* On failure, try to create the card from the list of available cards.
	 * We don't try with the card name that just failed.
	 */
	DEBUG("Could not hook soundcard, trying every card available");

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
		DEBUG("No soundcard could be hooked !");

		/* Card and channel names set to emptry string */
		g_free(audio->card);
		audio->card = g_strdup("");
		g_free(audio->channel);
		audio->channel = g_strdup("");

		/* Tell the world */
		invoke_handlers(audio, AUDIO_NO_CARD, AUDIO_USER_UNKNOWN);
	} else {
		DEBUG("Soundcard successfully hooked (scroll step: %lg, normalize: %s)",
		      audio->scroll_step, audio->normalize ? "true" : "false");

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
		invoke_handlers(audio, AUDIO_CARD_INITIALIZED, AUDIO_USER_UNKNOWN);
	}
}

/**
 * Reload the current preferences, and reload the hooked soundcard.
 * This has to be called each time the preferences are modified.
 *
 * @param audio an Audio instance.
 */
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

/**
 * Free an audio instance, therefore unhooking the sound card and
 * freeing any allocated ressources.
 *
 * @param audio an Audio instance.
 */
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

/**
 * Create a new Audio instance.
 * This does almost nothing actually, all the heavy job is done
 * in the audio_hook_soundcard() function.
 *
 * @return a newly allocated Audio instance.
 */
Audio *
audio_new(void)
{
	Audio *audio;

	audio = g_new0(Audio, 1);

	return audio;
}

/**
 * Return the list of playable cards as a GSList.
 * Must be freed using g_slist_free_full() and g_free().
 *
 * @return a list of playable cards.
 */
GSList *
audio_get_card_list(void)
{
	return alsa_list_cards();
}

/**
 * For a given card name, return the list of playable channels as a GSList.
 * Must be freed using g_slist_free_full() and g_free().
 *
 * @param card_name the name of the card for which we list the channels
 * @return a list of playable channels.
 */
GSList *
audio_get_channel_list(const char *card_name)
{
	return alsa_list_channels(card_name);
}

