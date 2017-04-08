// Copyright 2005-2017 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_PLUGIN_H_
#define MUMBLE_MUMBLE_PLUGIN_H_

#include <string>
#include <map>

#if defined(_MSC_VER)
# define MUMBLE_PLUGIN_CALLING_CONVENTION __cdecl
#elif defined(__MINGW32__)
# define MUMBLE_PLUGIN_CALLING_CONVENTION __attribute__((cdecl))
#else
# define MUMBLE_PLUGIN_CALLING_CONVENTION
#endif

#if defined(__GNUC__) && !defined(__MINGW32__) // GCC on Unix-like systems
# define MUMBLE_PLUGIN_EXPORT __attribute__((visibility("default")))
#elif defined(_MSC_VER)
# define MUMBLE_PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__MINGW32__)
# define MUMBLE_PLUGIN_EXPORT __attribute__((dllexport))
#else
# error No MUMBLE_PLUGIN_EXPORT definition available
#endif

// Visual Studio 2008 x86
#if _MSC_VER == 1500 && defined(_M_IX86)
# define MUMBLE_PLUGIN_MAGIC     0xd63ab7ef
# define MUMBLE_PLUGIN_MAGIC_2   0xd63ab7fe
# define MUMBLE_PLUGIN_MAGIC_QT  0xd63ab7ee
// Visual Studio 2010 x86
#elif _MSC_VER == 1600 && defined(_M_IX86)
# define MUMBLE_PLUGIN_MAGIC    0xd63ab7f0
# define MUMBLE_PLUGIN_MAGIC_2  0xd63ab7ff
# define MUMBLE_PLUGIN_MAGIC_QT 0xd63ab70f
// Visual Studio 2013 x86
#elif _MSC_VER == 1800 && defined(_M_IX86)
# define MUMBLE_PLUGIN_MAGIC    0xd63ab7c0
# define MUMBLE_PLUGIN_MAGIC_2  0xd63ab7cf
# define MUMBLE_PLUGIN_MAGIC_QT 0xd63ab7ca
// Visual Studio 2013 x64
#elif _MSC_VER == 1800 && defined(_M_X64)
# define MUMBLE_PLUGIN_MAGIC    0x9f3ed4c0
# define MUMBLE_PLUGIN_MAGIC_2  0x9f3ed4cf
# define MUMBLE_PLUGIN_MAGIC_QT 0x9f3ed4ca
// Visual Studio 2015 x86
#elif _MSC_VER == 1900 && defined(_M_IX86)
# define MUMBLE_PLUGIN_MAGIC    0xa9b8c7c0
# define MUMBLE_PLUGIN_MAGIC_2  0xa9b8c7cf
# define MUMBLE_PLUGIN_MAGIC_QT 0xa9b8c7ca
// Visual Studio 2015 x64
#elif _MSC_VER == 1900 && defined(_M_X64)
# define MUMBLE_PLUGIN_MAGIC    0x1f2e3dc0
# define MUMBLE_PLUGIN_MAGIC_2  0x1f2e3dcf
# define MUMBLE_PLUGIN_MAGIC_QT 0x1f2e3dca
// Generic plugin magic values for platforms
// where we do not officially plugins other
// than "link".
#else
# define MUMBLE_PLUGIN_MAGIC    0xf4573570
# define MUMBLE_PLUGIN_MAGIC_2  0xf457357f
# define MUMBLE_PLUGIN_MAGIC_QT 0xf457357a
#endif

#define MUMBLE_PLUGIN_MAGIC_C   0xf010101d

#define MUMBLE_PLUGIN_VERSION 2

typedef struct _MumblePlugin {
	unsigned int magic;
	const std::wstring &description;
	const std::wstring &shortname;
	void (MUMBLE_PLUGIN_CALLING_CONVENTION *about)(void *);
	void (MUMBLE_PLUGIN_CALLING_CONVENTION *config)(void *);
	int (MUMBLE_PLUGIN_CALLING_CONVENTION *trylock)();
	void (MUMBLE_PLUGIN_CALLING_CONVENTION *unlock)();
	const std::wstring(MUMBLE_PLUGIN_CALLING_CONVENTION *longdesc)();
	int (MUMBLE_PLUGIN_CALLING_CONVENTION *fetch)(float *avatar_pos, float *avatar_front, float *avatar_top, float *camera_pos, float *camera_front, float *camera_top, std::string &context, std::wstring &identity);
} MumblePlugin;

typedef struct _MumblePlugin2 {
	unsigned int magic;
	unsigned int version;
	int (MUMBLE_PLUGIN_CALLING_CONVENTION *trylock)(const std::multimap<std::wstring, unsigned long long int> &);
} MumblePlugin2;

/// MumblePluginQt provides an extra set of functions that will
/// provide a plugin with a pointer to a QWidget that should be
/// used as the parent for any dialogs Qt widgets shown by the
/// plugin.
///
/// This interface should only be used if a plugin intends to
/// present Qt-based dialogs to the user.
///
/// This interface is most useful for plugins that are internal
/// to Mumble. This is because plugins can't integrate with Mumble's
/// QWidgets unless they're built into Mumble itself.
typedef struct _MumblePluginQt {
	unsigned int magic;

	/// about is called when Mumble requests the plugin
	/// to show an about dialog.
	///
	/// The ptr argument is a pointer to a QWidget that
	/// should be used as the parent for a Qt-based
	/// about dialog.
	void (MUMBLE_PLUGIN_CALLING_CONVENTION *about)(void *ptr);

	/// config is called when Mumble requests the plugin
	/// to show its configuration dialog.
	///
	/// The ptr argument is a pointer to a QWidget that
	/// should be used as the parent for a Qt-based
	/// configuration dialog.
	void (MUMBLE_PLUGIN_CALLING_CONVENTION *config)(void *ptr);
} MumblePluginQt;

typedef struct _MumbleByteString {
	unsigned char *buf;
	size_t len;
} MumbleByteString;

typedef struct _MumbleWideString {
	wchar_t *str;
	size_t len;
} MumbleWideString;

#define MumbleInitConstWideString(cs) \
	{ cs, sizeof(cs)/sizeof(cs[0]) }

static inline void MumbleByteStringClear(MumbleByteString *bs) {
	if (bs != NULL && bs->buf != NULL) {
		memset(bs->buf, 0, bs->len);
	}
}

static inline void MumbleByteStringAssign(MumbleByteString *bs, const std::string &s) {
	if (bs != NULL && bs->buf != NULL) {
		memset(bs->buf, 0, bs->len);
		
		size_t nc = s.size();
		if (nc > bs->len - 1) {
			nc = bs->len - 1;
		}

		memcpy(bs->buf, s.c_str(), nc);
	}
}

static inline void MumbleWideStringClear(MumbleWideString *mws) {
	if (mws != NULL && mws->str != NULL) {
		memset(mws->str, 0, mws->len * sizeof(wchar_t));
	}
}

static inline void MumbleWideStringAssign(MumbleWideString *mws, const std::wstring &ws) {
	if (mws != NULL && mws->str != NULL) {
		memset(mws->str, 0, mws->len * sizeof(wchar_t));
		
		size_t nc = ws.size() * sizeof(wchar_t);
		if (nc > (mws->len * sizeof(wchar_t)) - sizeof(wchar_t)) {
			nc = (mws->len * sizeof(wchar_t)) - sizeof(wchar_t);
		}

		memcpy(mws->str, ws.c_str(), nc);
	}
}

typedef int MumblePIDLookupStatus;

#define MUMBLE_PID_LOOKUP_EOF  0
#define MUMBLE_PID_LOOKUP_OK   1

typedef void * MumblePIDLookupContext; 
typedef MumblePIDLookupStatus (*MumblePIDLookup)(MumblePIDLookupContext ctx, const wchar_t *str, unsigned long long *pid);

#define MUMBLE_PLUGIN_C_VERSION 1

typedef struct _MumblePluginC {
	unsigned int magic;
	unsigned int version;
	MumbleWideString description;
	MumbleWideString shortname;
	int (MUMBLE_PLUGIN_CALLING_CONVENTION *trylock)(MumblePIDLookup lookupFunc);
	void (MUMBLE_PLUGIN_CALLING_CONVENTION *unlock)();
	int (MUMBLE_PLUGIN_CALLING_CONVENTION *fetch)(float *avatar_pos, float *avatar_front, float *avatar_top, float *camera_pos, float *camera_front, float *camera_top, MumbleByteString *context, MumbleWideString *identity);
} MumblePluginC;

typedef MumblePlugin *(MUMBLE_PLUGIN_CALLING_CONVENTION *mumblePluginFunc)();
typedef MumblePlugin2 *(MUMBLE_PLUGIN_CALLING_CONVENTION *mumblePlugin2Func)();
typedef MumblePluginQt *(MUMBLE_PLUGIN_CALLING_CONVENTION *mumblePluginQtFunc)();
typedef MumblePluginC *(MUMBLE_PLUGIN_CALLING_CONVENTION *mumblePluginCFunc)();

/*
 * All plugins must implement one function called mumbleGetPlugin(), which
 * follows the mumblePluginFunc type and returns a MumblePlugin struct.
 *
 * magic should be initialized to MUMBLE_PLUGIN_MAGIC. description is the
 * name of the plugin shown in the GUI, while shortname is used for TTS.
 *
 * The individual functions are:
 * about(void *parent) - Player clicked the about button over plugin
 * config(void *parent) - Player clicked the config button
 * trylock() - Search system for likely process and try to lock on.
 *      The parameter is a set of process names and associated PIDs.
 *		Return 1 if succeeded, else 0. Note that once a plugin is locked on,
 *		no other plugins will be queried.
 * unlock() - Unlock from process. Either from user intervention or
 *		because fetch failed.
 * fetch(...) - Fetch data from locked process. avatar_pos is position in
 *		world coordinates (1 meter per unit). avatar_front and avatar_top
 *      specify the heading of the player, as in where he is looking.
 *		You need at minimum to figure out pos and front, otherwise
 *		sounds cannot be placed. If you do not fetch top, make it the
 *		same as front but rotated 90 degrees "upwards".
 *
 *      camera_x is the same, but for the camera. Make this identical to the
 *      avatar position if you don't know (or if it's a 1st person
 *      perspective).
 *
 *		It is important that you set all fields to 0.0 if you can't
 *		fetch meaningfull values, like between rounds and such.
 *
 *      context and identity are transmitted to the server. Only players
 *      with identical context will hear positional audio from each other.
 *      Mumble will automatically prepend the shortname of the plugin to
 *      the context, so make this a representation of the game server and
 *      team the player is on.
 *
 *      identity is retained by the server and is pollable over Ice/DBus,
 *      to be used by external scripts. This should uniquiely identify the
 *      player inside the game.
 *
 *      ctx_len and id_len are initialized to the bufferspace available. Set these
 *      to -1 to keep the previous value (as parsing and optimizing can be CPU
 *      intensive)
 *
 *		The function should return 1 if it is still "locked on",
 *		otherwise it should return 0. Mumble will call unlock()
 *		if it return 0, and go back to polling with trylock()
 *		once in a while.
 */

#endif
