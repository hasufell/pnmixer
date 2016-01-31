## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = pnmixer

pnmixer_SOURCES =		\
	alsa.c			\
	audio.c			\
	hotkey.c		\
	hotkeys.c		\
	main.c			\
	notify.c		\
	prefs.c			\
	support.c		\
	ui-about-dialog.c	\
	ui-hotkey-dialog.c	\
	ui-popup-menu.c		\
	ui-popup-window.c	\
	ui-prefs-window.c	\
	ui-tray-icon.c

pnmixer_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)

