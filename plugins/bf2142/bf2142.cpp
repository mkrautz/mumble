// Copyright 2005-2016 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "../mumble_plugin_win32.h"

BYTE *posptr;
BYTE *faceptr;
BYTE *topptr;

static int fetch(float *avatar_pos, float *avatar_front, float *avatar_top, float *camera_pos, float *camera_front, float *camera_top, std::string &context, std::wstring &) {
	for (int i=0;i<3;i++)
		avatar_pos[i] = avatar_front[i] = avatar_top[i] = camera_pos[i] = camera_front[i] = camera_top[i] = 0.0f;

	char ccontext[128];
	char state;
	char logincheck;
	bool ok;

	ok = peekProc((BYTE *) 0x00A1D908, &logincheck, 1);
	if (! ok)
		return false;

	if (logincheck == 0)
		return false;

	/*
		state value is:
		0						while not in game
		usually 1, never 0		if you create your own server ingame; this value will switch to 1 the instant you click "Join Game"
		usually 3, never 0		if you load into a server; this value will switch to 3 the instant you click "Join Game"
	*/
	ok = peekProc((BYTE *) 0x00B47968, &state, 1); // Magical state value
	if (! ok)
		return false;

	ok = peekProc(posptr, avatar_pos, 12) &&
	     peekProc(faceptr, avatar_front, 12) &&
	     peekProc(topptr, avatar_top, 12) &&
	     peekProc((BYTE *) 0x00B527B8, ccontext, 128);

	if (! ok)
		return false;

	/*
	    Get context string; in this plugin this will be an
	    ip:port (char 256 bytes) string
	*/
	ccontext[127] = 0;
	context = std::string(ccontext);

	if (state == 0)
		return true; // This results in all vectors beeing zero which tells Mumble to ignore them.

	for (int i=0;i<3;i++) {
		camera_pos[i] = avatar_pos[i];
		camera_front[i] = avatar_front[i];
		camera_top[i] = avatar_top[i];
	}

	return true;
}

static int trylock(const std::multimap<std::wstring, unsigned long long int> &pids) {
	posptr = faceptr = topptr = NULL;

	if (! initialize(pids, L"BF2142.exe", L"BF2142Audio.dll"))
		return false;

	BYTE *cacheaddr = pModule + 0x4745c;
	BYTE *cache = peekProc<BYTE *>(cacheaddr);

	posptr = peekProc<BYTE *>(cache + 0xc0);
	faceptr = peekProc<BYTE *>(cache + 0xc4);
	topptr = peekProc<BYTE *>(cache + 0xc8);

	float apos[3], afront[3], atop[3], cpos[3], cfront[3], ctop[3];
	std::string context;
	std::wstring identity;

	if (fetch(apos, afront, atop, cpos, cfront, ctop, context, identity)) {
		return true;
	} else {
		generic_unlock();
		return false;
	}
}

static const std::wstring longdesc() {
	return std::wstring(L"Supports Battlefield 2142 v1.50. No identity support yet.");
}

static std::wstring description(L"Battlefield 2142 v1.50");
static std::wstring shortname(L"Battlefield 2142");

static int trylock1() {
	return trylock(std::multimap<std::wstring, unsigned long long int>());
}

static MumblePlugin bf2142plug = {
	MUMBLE_PLUGIN_MAGIC,
	description,
	shortname,
	NULL,
	NULL,
	trylock1,
	generic_unlock,
	longdesc,
	fetch
};

static MumblePlugin2 bf2142plug2 = {
	MUMBLE_PLUGIN_MAGIC_2,
	MUMBLE_PLUGIN_VERSION,
	trylock
};

extern "C" __declspec(dllexport) MumblePlugin *getMumblePlugin() {
	return &bf2142plug;
}

extern "C" __declspec(dllexport) MumblePlugin2 *getMumblePlugin2() {
	return &bf2142plug2;
}
