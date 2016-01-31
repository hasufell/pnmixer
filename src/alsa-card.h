/* alsa-card.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file alsa-card.h
 * Header for alsa-card.c.
 * @brief header for alsa-card.c
 */

#ifndef ALSA_CARD_H_
#define ALSA_CARD_H_

#include <glib.h>

typedef struct alsa_card AlsaCard;

GSList *alsa_list_cards(void);
GSList *alsa_list_channels(const char *card_name);

AlsaCard *alsa_card_new_from_name(gboolean normalize, const char *card);
AlsaCard *alsa_card_new_from_list(gboolean normalize, GSList *card_list);
void alsa_card_free(AlsaCard *card);

const char *alsa_card_get_name(AlsaCard *card);
const char *alsa_card_get_channel(AlsaCard *card);
int alsa_card_get_volume(AlsaCard *card);
void alsa_card_set_volume(AlsaCard *card, int value, int dir);
gboolean alsa_card_is_muted(AlsaCard *card);
void alsa_card_toggle_mute(AlsaCard *card);


#endif				// ALSA_CARD_H_
