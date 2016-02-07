/* support-log.c
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file support-log.c
 * @brief Logging support, should be included by every file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

/**
 * Global variable to control whether we want debugging.
 * This variable is initialized in main() and depends on the
 * '--debug'/'-d' command line argument.
 */

gboolean want_debug = FALSE;
