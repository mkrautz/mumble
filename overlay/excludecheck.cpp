// Copyright 2005-2017 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "lib.h"
#include "exclude.h"

#include <tlhelp32.h>
#include <algorithm>
#include <string>
#include <vector>

static bool bExcludeCheckInitialized = false;

static int iExcludeMode = 0;
static std::vector<std::string> vLaunchers;
static std::vector<std::string> vWhitelist;
static std::vector<std::string> vPaths;
static std::vector<std::string> vBlacklist;

static void ExcludeCheckEnsureInitialized() {
	if (bExcludeCheckInitialized) {
		return;
	}

	int mode = ExcludeGetMode();
	if (mode == -1) {
		ods("ExcludeCheck: No setting for overlay exclusion mode. Using 'launcher' mode (0)");
		mode = 0;
	}
	iExcludeMode = mode;
	vLaunchers = ExcludeGetLaunchers();
	vWhitelist = ExcludeGetWhitelist();
	vPaths = ExcludeGetPaths();
	vBlacklist = ExcludeGetBlacklist();
}

static bool findParentProcessForChild(DWORD child, PROCESSENTRY32 *parent) {
	DWORD parentpid = 0;
	HANDLE hSnap = NULL;
	bool done = false;

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(pe);

	// Find our parent's process ID.
	{
		BOOL ok = FALSE;

		hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnap == INVALID_HANDLE_VALUE) {
			return false;
		}
		ok = Process32First(hSnap, &pe);
		while (ok) {
			if (pe.th32ProcessID == child) {
				parentpid = pe.th32ParentProcessID;
				break;
			}
			ok = Process32Next(hSnap, &pe);
		}
		CloseHandle(hSnap);
	}

	if (parentpid == 0) {
		return false;
	}

	// Find our parent's name.
	{
		BOOL ok = FALSE;

		hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnap == INVALID_HANDLE_VALUE) {
			return false;
		}
		ok = Process32First(hSnap, &pe);
		while (ok) {
			if (pe.th32ProcessID == parentpid) {
				memcpy(parent, &pe, sizeof(pe));
				ok = FALSE;
				done = true;
				break;
			}
			ok = Process32Next(hSnap, &pe);
		}
		CloseHandle(hSnap);
	}

	return done;
}

static bool findParentProcess(PROCESSENTRY32 *parent) {
	DWORD ourpid = GetCurrentProcessId();
	return findParentProcessForChild(ourpid, parent);
}

template <typename T> static inline bool vecContains(const std::vector<T> &v, T val) {
	return std::find(v.begin(), v.end(), val) != v.end();
}

static bool isBlacklistedExe(const std::string &exeName) {
	return vecContains(vBlacklist, exeName);
}

static bool isWhitelistedExe(const std::string &exeName) {
	return vecContains(vWhitelist, exeName);
}

static bool isWhitelistedPath(const std::string &absExeName) {
	for (size_t i = 0; i < vPaths.size(); i++) {
		const std::string &path = vPaths.at(i);
		if (absExeName.find(path) == 0) {
			return true;
		}
	}
	return false;
}

static bool isWhitelistedParent(const std::string &parentExeName) {
	return vecContains(vLaunchers, parentExeName);
}

bool ExcludeCheckIsOverlayEnabled(const std::string &absExeName, const std::string &exeName) {
	bool enableOverlay = true;

	ExcludeCheckEnsureInitialized();

	if (iExcludeMode == 0) { // Launcher filter

		PROCESSENTRY32 parent;
		if (!findParentProcess(&parent)) {
			// Unable to find parent. Process is allowed.
			ods("ExcludeCheck: Unable to find parent. Process allowed.");
			enableOverlay = true;
		}

		std::string parentExeName(parent.szExeFile);
		std::transform(parentExeName.begin(), parentExeName.end(), parentExeName.begin(), tolower);

		ods("ExcludeCheck: Parent is '%s'", parentExeName.c_str());

		// The blacklist always wins.
		// If an exe is in the blacklist, never show the overlay, ever.
		if (isBlacklistedExe(exeName)) {
			ods("ExcludeCheck: '%s' is blacklisted. Overlay disabled.", exeName.c_str());
			enableOverlay = false;
		// If the process's exe name is whitelisted, allow the overlay to be shown.
		} else if (isWhitelistedExe(exeName)) {
			ods("ExcludeCheck: '%s' is whitelisted. Overlay enabled.", exeName.c_str());
			enableOverlay = true;
		// If the exe is within a whitelisted path, allow the overlay to be shown.
		// An example is:
		// Whitelisted path: d:\games
		// absExeName: d:\games\World of Warcraft\Wow-64.exe
		// The overlay would be shown in WoW (and any game that lives under d:\games)
		 } else if (isWhitelistedPath(absExeName)) {
			ods("ExcludeCheck: '%s' is within a whitelisted path. Overlay enabled.", absExeName.c_str());
		 	enableOverlay = true;
		// If the direct parent is whitelisted, allow the process through.
		// This allows us to whitelist Steam.exe, etc. -- and have the overlay
		// show up in all Steam titles.
		} else if (isWhitelistedParent(parentExeName)) {
			ods("ExcludeCheck: Parent '%s' of '%s' is whitelisted. Overlay enabled.", parentExeName.c_str(), exeName.c_str());
			enableOverlay = true;
		// If nothing matched, do not show the overlay.
		} else {
			ods("ExcludeCheck: No matching overlay exclusion rule found. Overlay disabled.");
			enableOverlay = false;
		}
	} else if (iExcludeMode == 1) {
		if (isWhitelistedExe(exeName)) {
			enableOverlay = true;
		} else {
			enableOverlay = false;
		}
	} else if (iExcludeMode == 2) { // Blacklist
		if (isBlacklistedExe(exeName)) {
			enableOverlay = false;
		} else {
			enableOverlay = true;
		}
	}

	return enableOverlay;
}
