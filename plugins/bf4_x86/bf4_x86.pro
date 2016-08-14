include(../plugins.pri)

TARGET = bf4_x86
linux:TARGET = mumble_paplugin_win32_bf4_x86
SOURCES = bf4_x86.cpp
win32:LIBS += -luser32
