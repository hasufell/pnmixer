/* hotkeys.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file hotkeys.h
 * Header for hotkeys.c.
 * @brief Header for hotkeys.c.
 */

#ifndef _HOTKEYS_H_
#define _HOTKEYS_H_

#include "audio.h"

typedef struct hotkeys Hotkeys;

Hotkeys *hotkeys_new(Audio *audio);
void hotkeys_free(Hotkeys *hotkeys);
void hotkeys_reload(Hotkeys *hotkeys);
void hotkeys_bind(Hotkeys *hotkeys);
void hotkeys_unbind(Hotkeys *hotkeys);

#endif				// _HOTKEYS_H
