/* alsa.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE /* exp10() */
#include <math.h>
#include <glib.h>
#include <alsa/asoundlib.h>

#include "prefs.h"
#include "support.h"
#include "alsa.h"

#define ALSA_DEFAULT_CARD "(default)"
#define ALSA_DEFAULT_HCTL "default"

/*
 * Alsa log and debug macros.
 */

#define ALSA_DEBUG(fmt, ...)	  \
	DEBUG("Alsa: " fmt, ##__VA_ARGS__)

#define ALSA_WARN(fmt, ...)	  \
	WARN("Alsa: " fmt, ##__VA_ARGS__)

#define ALSA_ERR(err, fmt, ...)	  \
	ERROR("Alsa: " fmt ": %s", ##__VA_ARGS__, snd_strerror(err))

#define ALSA_CARD_DEBUG(card, fmt, ...)	  \
	DEBUG("Alsa: '%s': " fmt, card, ##__VA_ARGS__)

#define ALSA_CARD_WARN(card, fmt, ...)	  \
	WARN("Alsa: '%s': " fmt, card, ##__VA_ARGS__)

#define ALSA_CARD_ERR(card, err, fmt, ...)	  \
	ERROR("Alsa: '%s': " fmt ": %s", card, ##__VA_ARGS__, snd_strerror(err))

/*
 * Alsa volume management.
 * Parts of this code were taken and adapted from the original
 * alsa-utils package, `alsamixer` program, `volume_mapping.c` file.
 *
 * Copyright (c) 2010 Clemens Ladisch <clemens@ladisch.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#define MAX_LINEAR_DB_SCALE 24

static inline gboolean
use_linear_dB_scale(long db_min, long db_max)
{
	return db_max - db_min <= MAX_LINEAR_DB_SCALE * 100;
}

static long
lrint_dir(double x, int dir)
{
	if (dir > 0)
		return lrint(ceil(x));
	else if (dir < 0)
		return lrint(floor(x));
	else
		return lrint(x);
}

/* Return something between 0 and 1 */
static double
get_volume(const char *hctl, snd_mixer_elem_t *elem, double *volume)
{
	snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_FRONT_RIGHT;
	int err;
	long min, max, value;

	*volume = 0;

	err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't get playback volume range");
		return FALSE;
	}

	if (min >= max) {
		ALSA_CARD_WARN(hctl, "Invalid playback volume range [%ld - %ld]", min, max);
		return FALSE;
	}

	err = snd_mixer_selem_get_playback_volume(elem, channel, &value);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't get playback volume");
		return FALSE;
	}

	*volume = (value - min) / (double) (max - min);

	return TRUE;
}

static gboolean
set_volume(const char *hctl, snd_mixer_elem_t *elem,
           double volume, int dir)
{
	int err;
	long min, max, value;

	err = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't get playback volume range");
		return FALSE;
	}

	if (min >= max) {
		ALSA_CARD_WARN(hctl, "Invalid playback volume range [%ld - %ld]", min, max);
		return FALSE;
	}

	value = lrint_dir(volume * (max - min), dir) + min;

	err = snd_mixer_selem_set_playback_volume_all(elem, value);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't set playback volume to %ld", value);
		return FALSE;
	}

	// TODO: what about the 'intentionally set twice' thing ?

	return TRUE;
}


/* Return something between 0 and 1 */
static gboolean
get_normalized_volume(const char *hctl, snd_mixer_elem_t *elem, double *volume)
{
	snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_FRONT_RIGHT;
	int err;
	long min, max, value;
	double normalized, min_norm;

	*volume = 0;

	err = snd_mixer_selem_get_playback_dB_range(elem, &min, &max);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't get playback dB range");
		return FALSE;
	}

	if (min >= max) {
		ALSA_CARD_WARN(hctl, "Invalid playback dB range [%ld - %ld]", min, max);
		return FALSE;
	}
	
	err = snd_mixer_selem_get_playback_dB(elem, channel, &value);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't get playback dB");
		return FALSE;
	}

	if (use_linear_dB_scale(min, max)) {
		normalized = (value - min) / (double) (max - min);
	} else {
		normalized = exp10((value - max) / 6000.0);
		if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
			min_norm = exp10((min - max) / 6000.0);
			normalized = (normalized - min_norm) / (1 - min_norm);
		}
	}

	*volume = normalized;

	return TRUE;
}

static gboolean
set_normalized_volume(const char *hctl, snd_mixer_elem_t *elem,
                      double volume, int dir)
{
	int err;
	long min, max, value;

	err = snd_mixer_selem_get_playback_dB_range(elem, &min, &max);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't get playback dB range");
		return FALSE;
	}

	if (min >= max) {
		ALSA_CARD_WARN(hctl, "Invalid playback dB range [%ld - %ld]", min, max);
		return FALSE;
	}

	if (use_linear_dB_scale(min, max)) {
		value = lrint_dir(volume * (max - min), dir) + min;
	} else {
		if (min != SND_CTL_TLV_DB_GAIN_MUTE) {
			double min_norm = exp10((min - max) / 6000.0);
			volume = volume * (1 - min_norm) + min_norm;
		}
		value = lrint_dir(6000.0 * log10(volume), dir) + max;
	}

	err = snd_mixer_selem_set_playback_dB_all(elem, value, dir);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't set playback dB to %ld", value);
		return FALSE;
	}

	// TODO: what about the 'intentionally set twice' thing ?

	return TRUE;
}

static gboolean
get_mute(const char *hctl, snd_mixer_elem_t *elem, gboolean *muted)
{
	snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_FRONT_RIGHT;
	int err;
	int value;

	*muted = FALSE;

	if (snd_mixer_selem_has_playback_switch(elem)) {
		err = snd_mixer_selem_get_playback_switch(elem, channel, &value);
		if (err < 0) {
			ALSA_CARD_ERR(hctl, err, "Can't get playback switch");
			return FALSE;
		}
	} else {
		/* If there's no playback switch, assume not muted */
		value = 1;
	}

	/* Value returned: 0 = muted, 1 = not muted */
	*muted = value == 0 ? TRUE : FALSE;

	return TRUE;
}

static gboolean
set_mute(const char *hctl, snd_mixer_elem_t *elem, gboolean mute)
{
	int err;
	int value;

	/* Value to set: 0 = muted, 1 = not muted */
	value = mute ? 0 : 1;

	if (snd_mixer_selem_has_playback_switch(elem)) {
		err = snd_mixer_selem_set_playback_switch_all(elem, value);
		if (err < 0) {
			ALSA_CARD_ERR(hctl, err, "Can't set playback switch");
			return FALSE;
		}
	} else {
		/* If there's no playback switch, do nothing */
	}

	return TRUE;
}

/*
 * Mixer handling
 */

static char *
get_hctl_from_name(const char *card_name)
{
	int err;
	int number;

	/* Handle the special 'default' card */
	if (!g_strcmp0(card_name, ALSA_DEFAULT_CARD))
		return g_strdup(ALSA_DEFAULT_HCTL);

	/* Iterate on cards and try to find the one it */
	for (number = -1;;) {
		char *name;

		/* Get next soundcard */
		err = snd_card_next(&number);
		if (err < 0) {
			ALSA_ERR(err, "Can't enumerate sound cards");
			break;
		}

		if (number < 0)
			break;

		/* Get card name */
		err = snd_card_get_name(number, &name);
		if (err < 0) {
			ALSA_ERR(err, "Can't get card name");
			break;
		}

		/* Check if it's the card we're looking for */
		if (!g_strcmp0(name, card_name)) {
			g_free(name);
			return g_strdup_printf("hw:%d", number);
		}

		/* Nope. Don't forget to free before continuing. */
		g_free(name);
	}

	return NULL;
}

static snd_mixer_elem_t *
mixer_elem_get(const char *hctl, snd_mixer_t *mixer, const char *channel)
{
	snd_mixer_elem_t *elem = NULL;

	/* Look for the given channel amongst mixer elements */
	if (channel) {
		snd_mixer_selem_id_t *sid;
		snd_mixer_selem_id_alloca(&sid);
		snd_mixer_selem_id_set_name(sid, channel);
		elem = snd_mixer_find_selem(mixer, sid);
		if (elem == NULL)
			ALSA_CARD_WARN(hctl, "Can't find channel %s", channel);
	}

	/* If we got a playable mixer element at this point, we're done ! */
	if (elem && snd_mixer_selem_has_playback_volume(elem))
		return elem;

	/* Otherwise, iterate on mixer elements until we find a playable one */
	ALSA_CARD_DEBUG(hctl, "Looking for a playable mixer element...");
	elem = snd_mixer_first_elem(mixer);
	while (elem) {
		if (snd_mixer_selem_has_playback_volume(elem))
			break;
		elem = snd_mixer_elem_next(elem);
	}

	if (elem == NULL)
		ALSA_CARD_DEBUG(hctl, "No playable mixer element found");

	return elem;
}

static void
mixer_close(const char *hctl, snd_mixer_t *mixer)
{
	int err;

	ALSA_CARD_DEBUG(hctl, "Closing mixer");

	snd_mixer_free(mixer);

	err = snd_mixer_detach(mixer, hctl);
	if (err < 0)
		ALSA_CARD_ERR(hctl, err, "Can't detach mixer");

	err = snd_mixer_close(mixer);
	if (err < 0)
		ALSA_CARD_ERR(hctl, err, "Can't close mixer");
}

static snd_mixer_t *
mixer_open(const char *hctl)
{
	int err;
	snd_mixer_t *mixer = NULL;

	ALSA_CARD_DEBUG(hctl, "Opening mixer");

	err = snd_mixer_open(&mixer, 0);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't open mixer");
		return NULL;
	}

	err = snd_mixer_attach(mixer, hctl);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't attach card to mixer");
		goto failure;
	}
	
	err = snd_mixer_selem_register(mixer, NULL, NULL);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't register mixer simple element");
		goto failure;
	}

	err = snd_mixer_load(mixer);
	if (err < 0) {
		ALSA_CARD_ERR(hctl, err, "Can't load mixer elements");
		goto failure;
	}

	return mixer;

failure:
	snd_mixer_close(mixer);
	return NULL;
}


/* Private card-related helpers and callback */

/* This is a very arbitrary value.
 * The number of poll descriptors I witnessed has always been one.
 */
#define MIXER_WATCHES_N_MAX 8

struct alsa_card {
	char *name;
	char *hctl;
	char *channel;
	snd_mixer_t *mixer;
	guint mixer_watch_ids[MIXER_WATCHES_N_MAX];
	AlsaValuesChangedCb mixer_values_changed_cb;
	snd_mixer_elem_t *mixer_elem;
	gboolean normalize;
};

/**
 * Alsa callback for a mixer element.
 * This is triggered in the following cases:
 * - when the channel has been removed (cleanup, soundcard unplugged)
 * - when there's an EXTERNAL volume/mute change
 * When PNMixer changes the volume/mute, this callback is not invoked.
 * Alsa documentation doesn't explain anything about this behavior.
 * I suppose it's normal.
 */
static int
mixer_elem_cb(snd_mixer_elem_t *elem, unsigned int mask)
{
	const char *channel = snd_mixer_selem_get_name(elem);

	/* Test MASK_REMOVE first. Quoting Alsa documentation:
	 *   Element has been removed. (Warning: test this first
	 *   and if set don't test the other masks).
	 */
	if (mask == SND_CTL_EVENT_MASK_REMOVE) {
		ALSA_DEBUG("Channel '%s' has been removed", channel);
		return 0;
	}

	/* Now test for a value change */
	if (mask & SND_CTL_EVENT_MASK_VALUE) {
		AlsaCard *card;
		AlsaValuesChangedCb callback;

		ALSA_DEBUG("Event value changed", channel);
		card = snd_mixer_elem_get_callback_private(elem);
		callback = card->mixer_values_changed_cb;

		if (callback) {
			gboolean muted = alsa_card_is_muted(card);
			gdouble volume = alsa_card_get_volume(card);
			callback(muted, volume);
		}
		
#if 0		
		// TODO: send signal instead !
		int muted;
		muted = ismuted();
		do_update_ui();
		if (enable_noti && external_noti) {
			int vol = getvol();
			if (muted)
				do_notify_volume(vol, FALSE);
			else
				do_notify_volume(vol, TRUE);
		}
#endif
	} else {
		ALSA_DEBUG("Unhandled event mask: %d", mask);
	}

	return 0;
}

static void
alsa_card_set_mixer_elem_cb(AlsaCard *card)
{
	snd_mixer_elem_t *mixer_elem = card->mixer_elem;

	snd_mixer_elem_set_callback(mixer_elem, mixer_elem_cb);
	snd_mixer_elem_set_callback_private(mixer_elem, card);
}

/**
 * Callback function for volume changes.
 *
 * @param source the GIOChannel event source
 * @param condition the condition which has been satisfied
 * @param data user data set inb g_io_add_watch() or g_io_add_watch_full()
 * @return FALSE if the event source should be removed
 */
static gboolean
mixer_gio_cb(GIOChannel *source, GIOCondition condition, AlsaCard *card)
{
	gchar sbuf[256];
	gsize sread = 1;

	ALSA_DEBUG("Entering mixer_gio_cb");

	/* Handle pending mixer events invoking callbacks */
	snd_mixer_handle_events(card->mixer);

#if 0
	if (condition == G_IO_ERR) {
		/* This happens when the file descriptor we're watching disappeared.
		 * For example, if the USB soundcard has been unplugged.
		 * In this case, reloading alsa is the nice thing to do, it will
		 * cause PNMixer to select the first card available.
		 */
		do_notify_text(_("Soundcard disconnected"),
			       _("Soundcard has been disconnected, reloading Alsa..."));
		g_idle_add(idle_alsa_reinit, NULL);
		return FALSE;
	}
#endif

	sread = 1;
	while (sread) {
		GIOStatus stat;

		/* This handles the case where mixer_elem_cb() doesn't read all
		 * the data on source. If we don't clear it out we'll go into an infinite
		 * callback loop since there will be data on the channel forever.
		 */
		stat = g_io_channel_read_chars(source, sbuf, 256, &sread, NULL);

		switch (stat) {
		case G_IO_STATUS_AGAIN:
			// normal, means alsa_cb cleared out the channel
			continue;
		case G_IO_STATUS_NORMAL:
			// actually bad, alsa failed to clear channel
			warn_sound_conn_lost();
			break;
		case G_IO_STATUS_ERROR:
		case G_IO_STATUS_EOF:
			report_error("Error: GIO error has occurred. Won't respond to "
				     "external volume changes anymore.");
			break;
		default:
			report_error("Error: Unknown status from "
				     "g_io_channel_read_chars.");
		}

		return TRUE;
	}

	return TRUE;
}

static void
alsa_card_unset_mixer_watches(AlsaCard *card)
{
	guint *watch_ids = card->mixer_watch_ids;
	gsize n_watches_max = G_N_ELEMENTS(card->mixer_watch_ids);
	int i;

	/* Unwatch everything */
	for (i = 0; i < n_watches_max; i++) {
		if (watch_ids[i] == 0)
			break;
		g_source_remove(watch_ids[i]);
		watch_ids[i] = 0;
	}
}

static void
alsa_card_set_mixer_watches(AlsaCard *card)
{
	snd_mixer_t *mixer = card->mixer;
	guint *watch_ids = card->mixer_watch_ids;
	gsize n_watches_max = G_N_ELEMENTS(card->mixer_watch_ids);
	struct pollfd fds[n_watches_max];
	int i, pcount;

	/* This should be called once only */
	g_assert(watch_ids[0] == 0);

	/* Get the mixer poll descriptors */
	pcount = snd_mixer_poll_descriptors_count(mixer);
	g_assert(pcount <= n_watches_max);
	pcount = snd_mixer_poll_descriptors(mixer, fds, pcount);
	if (pcount <= 0) {
		report_error("Warning: Couldn't get any poll descriptors. "
			     "Won't respond to external volume changes.");
		return;
	}

	/* Add a watch for each poll descriptor */
	for (i = 0; i < pcount; i++) {
		GIOChannel *gioc = g_io_channel_unix_new(fds[i].fd);
		watch_ids[i] = g_io_add_watch(gioc, G_IO_IN | G_IO_ERR,
		                              (GIOFunc) mixer_gio_cb, card);
		g_io_channel_unref(gioc);
	}

	ALSA_DEBUG("%d poll descriptors are now watched", pcount);
}

/* Public card-related functions */

const char *
alsa_card_get_name(AlsaCard *card)
{
	return card->name;
}

const char *
alsa_card_get_channel(AlsaCard *card)
{
	return card->channel;
}

/* Return volume in percent */
gdouble
alsa_card_get_volume(AlsaCard *card)
{
	gdouble volume = 0;
	gboolean gotten = FALSE;

	if (card->normalize)
		gotten = get_normalized_volume(card->hctl, card->mixer_elem, &volume);

	if (!gotten)
		get_volume(card->hctl, card->mixer_elem, &volume);

	return volume * 100;
}

/* Set volume in percent */
void
alsa_card_set_volume(AlsaCard *card, gdouble value, int dir)
{
	gboolean muted;
	gdouble volume;
	gboolean set = FALSE;
	AlsaValuesChangedCb callback;

	volume = value / 100.0;

	if (card->normalize)
	        set = set_normalized_volume(card->hctl, card->mixer_elem, volume, dir);

	if (!set)
		set_volume(card->hctl, card->mixer_elem, volume, dir);

	callback = card->mixer_values_changed_cb;
	if (callback) {
		muted = alsa_card_is_muted(card);
		volume = alsa_card_get_volume(card);
		callback(muted, volume);
	}
}

gboolean
alsa_card_is_muted(AlsaCard *card)
{
	gboolean muted;

	get_mute(card->hctl, card->mixer_elem, &muted);
	return muted;
}

void
alsa_card_toggle_mute(AlsaCard *card)
{
	gboolean muted;
	gdouble volume;
	AlsaValuesChangedCb callback;

	muted = alsa_card_is_muted(card);
	set_mute(card->hctl, card->mixer_elem, !muted);
	
	callback = card->mixer_values_changed_cb;
	if (callback) {
		muted = alsa_card_is_muted(card);
		volume = alsa_card_get_volume(card);
		callback(muted, volume);
	}
}

void
alsa_card_set_values_changed_callback(AlsaCard *card, AlsaValuesChangedCb cb)
{
	card->mixer_values_changed_cb = cb;

	/* Set callbacks */
	alsa_card_set_mixer_elem_cb(card);
	alsa_card_set_mixer_watches(card);
}

void
alsa_card_free(AlsaCard *card)
{
	if (card == NULL)
		return;

	if (card->mixer) {
		alsa_card_unset_mixer_watches(card);
		mixer_close(card->hctl, card->mixer);
	}

	g_free(card->channel);
	g_free(card->hctl);
	g_free(card->name);
	g_free(card);
}

static AlsaCard *
alsa_card_new(gboolean normalize, const char *card_name, const char *channel)
{
	AlsaCard *card;

	card = g_new0(AlsaCard, 1);

	/* Save normalize param */
	card->normalize = normalize;

	/* Save card name */
	if (!card_name)
		card_name = ALSA_DEFAULT_CARD;
	card->name = g_strdup(card_name);

	/* Get corresponding HCTL name */
	card->hctl = get_hctl_from_name(card_name);
	if (card->hctl == NULL)
		goto failure;

	/* Open mixer */
	card->mixer = mixer_open(card->hctl);
	if (card->mixer == NULL)
		goto failure;

	/* Get mixer element */
	card->mixer_elem = mixer_elem_get(card->hctl, card->mixer, channel);
	if (card->mixer_elem == NULL)
		goto failure;

	/* Save channel name - it may be different from the one given in param ! */
	card->channel = g_strdup(snd_mixer_selem_get_name(card->mixer_elem));

	/* Sum up the situation */
	ALSA_DEBUG("Initialized ! Card '%s', channel '%s'",
	           card->name, card->channel);

	return card;

failure:
	alsa_card_free(card);
	return NULL;
}

AlsaCard *
alsa_card_new_from_name(gboolean normalize, const char *card_name)
{
	AlsaCard *card;
	char *channel;

	channel = prefs_get_channel(card_name);
	card = alsa_card_new(normalize, card_name, channel);
	g_free(channel);

	return card;
}

AlsaCard *
alsa_card_new_from_list(gboolean normalize, GSList *card_list)
{
	AlsaCard *card = NULL;
	GSList *item;

	for (item = card_list; item; item = item->next) {
		char *channel;
		const char *card_name;

		card_name = item->data;
		channel = prefs_get_channel(card_name);
		card = alsa_card_new(normalize, card_name, channel);
		g_free(channel);

		if (card)
			break;
	}

	return card;
}

/*
 * Alsa listing functions
 */

/* Must be freed */
GSList *
alsa_list_cards(void)
{
	int err;
	int number;
	GSList *list = NULL;

	/* Always first in the list, the 'default' card */
	list = g_slist_append(list, g_strdup(ALSA_DEFAULT_CARD));

	/* Then, iterate on available cards */
	for (number = -1;;) {
		char *name;

		/* Get next soundcard */
		err = snd_card_next(&number);
		if (err < 0) {
			ALSA_ERR(err, "Can't enumerate sound cards");
			break;
		}

		if (number < 0)
			break;

		/* Get card name */
		err = snd_card_get_name(number, &name);
		if (err < 0) {
			ALSA_ERR(err, "Can't get card name");
			break;
		}

		/* Add it to the list */
		list = g_slist_append(list, name);
	}

	return list;
}

/* For a given card name, return the list of playable channels,
 * as a GSList.
 * Must be freed using g_free.
 */
GSList *
alsa_list_channels(const char *card_name)
{
	char *hctl = NULL;
	snd_mixer_t *mixer = NULL;
	snd_mixer_elem_t *elem = NULL;
	GSList *list = NULL;

	/* Get HCTL */
	hctl = get_hctl_from_name(card_name);
	if (hctl == NULL)
		goto exit;

	/* Open mixer */
	mixer = mixer_open(hctl);
	if (mixer == NULL)
		goto exit;

	/* Build channel list */
	elem = snd_mixer_first_elem(mixer);
	while (elem) {
		if (snd_mixer_selem_has_playback_volume(elem)) {
			const char *chan_name = snd_mixer_selem_get_name(elem);
			list = g_slist_append(list, g_strdup(chan_name));
		}
		elem = snd_mixer_elem_next(elem);
	}

exit:
	/* Close mixer */
	if (mixer)
		mixer_close(hctl, mixer);

	if (hctl)
		g_free(hctl);

	return list;
}
