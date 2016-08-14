# Copyright 2005-2016 The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

include(../plugins.pri)

TARGET = bfheroes
linux:TARGET = mumble_paplugin_win32_bfheroes
SOURCES = bfheroes.cpp
win32:LIBS += -luser32
