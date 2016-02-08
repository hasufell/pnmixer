/* support-log.h
 * PNmixer is written by Nick Lanham, a fork of OBmixer
 * which was programmed by Lee Ferrett, derived
 * from the program "AbsVolume" by Paul Sherman
 * This program is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General
 * Public License v3. source code is available at
 * <http://github.com/nicklan/pnmixer>
 */

/**
 * @file support-log.h
 * Log handlers. Provides various macros to print debug, warning
 * and error messages. Debug messages are enabled by a command-line
 * argument.
 * This should be included by every file.
 * @brief Logging support, should be included by every file.
 */

#ifndef _SUPPORT_LOG_H_
#define _SUPPORT_LOG_H_

#include <stdio.h>
#include <glib.h>

extern gboolean want_debug;

/**
 * Macro to print verbose debug info in case we want debugging.
 */

#define VT_ESC    "\033"
#define VT_RESET  "[0m"
#define VT_RED    "[0;31m"
#define VT_GREY   "[0;37m"
#define VT_YELLOW "[1;33m"

#define ERROR(fmt, ...)	  \
	fprintf(stderr, \
	        VT_ESC VT_RED "error" VT_ESC VT_RESET ": %s: " fmt "\n", \
	        __FILE__, ##__VA_ARGS__)

#define WARN(fmt, ...)	  \
	fprintf(stderr, \
	        VT_ESC VT_YELLOW "warning" VT_ESC VT_RESET ": %s: " fmt "\n",	\
	        __FILE__, ##__VA_ARGS__)

#define _DEBUG(fmt, ...)	  \
	fprintf(stderr, \
	        VT_ESC VT_GREY "debug" VT_ESC VT_RESET ": %s: " fmt "\n", \
	        __FILE__, ##__VA_ARGS__)

#define DEBUG(fmt, ...)	  \
	do { \
		if (want_debug == TRUE) \
			_DEBUG(fmt, ##__VA_ARGS__); \
	} while (0) \
 
#endif				// _SUPPORT_LOG_H_
