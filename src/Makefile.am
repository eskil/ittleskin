NULL=

INCLUDES = 							\
	$(GNOME_INCLUDEDIR)					\
	$(LITTLESKIN_CFLAGS)					\
	-DGNOME_ICONDIR=\""$(datadir)/pixmaps"\"		\
	$(NULL)

lib_LTLIBRARIES = libittleskin.la
noinst_PROGRAMS = test-littleskin

libittleskinincludedir = $(includedir)/littleskin/

libittleskininclude_HEADERS	=	\
	littleskin.h		\
	$(NULL)

noinst_HEADERS =		\
	littleskin-private.h	\
	$(NULL)

libittleskin_la_LDFLAGS =	\
	$(NULL)

libittleskin_la_SOURCES =	\
	littleskin.c		\
	$(NULL)

libittleskin_la_LDFLAGS =	\
	-version-info 0:0:0	\
	$(NULL)

test_littleskin_SOURCES =	\
	test-littleskin.c	\
	$(NULL)

test_littleskin_LDADD =		\
	libittleskin.la		\
	$(LITTLESKIN_LIBS)	\
	$(NULL)
