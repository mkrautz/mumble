# Overlay payload for Unix-like systems and OS X.

include(../compiler.pri)

TEMPLATE = lib
CONFIG -= qt gui
CONFIG *= debug_and_release
VERSION = 1.3.0
SOURCES = overlay.c

CONFIG(static) {
	CONFIG -= static
}

unix:!macx {
	TARGET = mumble$(TARGET_ADD)

	DEFINES += TARGET_UNIX

	LIBS *= -lrt -ldl
	QMAKE_CFLAGS *= -fvisibility=hidden $(CFLAGS_ADD)
	QMAKE_LFLAGS -= -Wl,--no-undefined
	QMAKE_LFLAGS *= $(LFLAGS_ADD)
	equals(QMAKE_LINK,g++) {
	  message(Overriding linker)
	  QMAKE_LINK = gcc
	  QMAKE_LINK_SHLIB = gcc
	}
}

macx {
	TARGET = mumbleoverlay

	CONFIG *= x86_64 x86 plugin
	CONFIG(universal) {
		CONFIG *= ppc
	}

	DEFINES += TARGET_MAC

	QMAKE_CFLAGS *= -x objective-c
	QMAKE_LFLAGS_PLUGIN += -undefined dynamic_lookup -dynamic
	QMAKE_LFLAGS = -framework CoreFoundation
	QMAKE_INFO_PLIST = overlay_gl.plist


	INCLUDEPATH *= ../3rdparty/mach-override-src
	LIBS *= -lmach-override
}

CONFIG(debug, debug|release) {
	QMAKE_LIBDIR *= ../debug$(DESTDIR_ADD)
	DESTDIR = ../debug$(DESTDIR_ADD)
}

CONFIG(release, debug|release) {
	QMAKE_LIBDIR *= ../release$(DESTDIR_ADD)
	DESTDIR = ../release$(DESTDIR_ADD)
}

include(../symbols.pri)
