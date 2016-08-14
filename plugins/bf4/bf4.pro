include(../plugins.pri)

TARGET = bf4
linux:TARGET = mumble_paplugin_win32_bf4
SOURCES	= bf4.cpp
win32:LIBS += -luser32
