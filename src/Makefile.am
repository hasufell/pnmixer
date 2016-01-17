## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = pnmixer

pnmixer_SOURCES = \
	support.c support.h \
	main.c main.h \
	hotkeys.c hotkeys.h \
	notify.c notify.h \
	alsa.c alsa.h \
	prefs.c prefs.h \
	debug.h \
	ui-about-dialog.c ui-about-dialog.h \
	ui-prefs-window.c ui-prefs-window.h \
	ui-popup-menu.c ui-popup-menu.h \
	ui-popup-window.c ui-popup-window.h \
	ui-tray-icon.c ui-tray-icon.h

pnmixer_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)

