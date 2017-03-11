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

	bExcludeCheckInitialized = true;
}

static bool findParentProcessForChild(DWORD child, PROCESSENTRY32 *parent) {
	DWORD parentpid = 0;
	HANDLE hSnap = NULL;
	bool done = false;

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(pe);

	MODULEENTRY32 me;
	me.dwSize = sizeof(me);

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

static bool getModuleForParent(PROCESSENTRY32 *parent, MODULEENTRY32 *module) {
	HANDLE hSnap = NULL;
	bool done = false;

	MODULEENTRY32 me;
	me.dwSize = sizeof(me);

	hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE32 | TH32CS_SNAPMODULE, parent->th32ProcessID);
	if (hSnap == INVALID_HANDLE_VALUE) {
		goto out;
	}

	if (!Module32First(hSnap, &me)) {
		goto out;
	}

	memcpy(module, &me, sizeof(me));
	done = true;

out:
	CloseHandle(hSnap);
	return done;
}

static bool getParentProcessInfo(std::string &parentAbsExeName, std::string &parentExeName) {
	DWORD ourpid = GetCurrentProcessId();
	PROCESSENTRY32 parent;
	MODULEENTRY32 module;

	bool ok = findParentProcessForChild(ourpid, &parent);
	if (ok) {
		ok = getModuleForParent(&parent, &module);
		if (ok) {
			parentAbsExeName = std::string(module.szExePath);
			parentExeName = std::string(parent.szExeFile);
			return true;
		}
	}

	return false;
}

// It rhymes!
static inline void inPlaceLowerCase(std::string &s) {
	std::transform(s.begin(), s.end(), s.begin(), tolower);
}

static bool isAbsPath(const std::string &path) {
	return path.find("\\") != std::string::npos;
}

static bool isBlacklistedExe(const std::string &absExeName, const std::string &exeName) {
	for (size_t i = 0; i < vBlacklist.size(); i++) {
		std::string &val = vBlacklist.at(i);
		if (isAbsPath(val)) {
			if (val == absExeName) {
				return true;
			}
		} else {
			if (val == exeName) {
				return true;
			}
		}
	}
	return false;
}

static bool isWhitelistedExe(const std::string &absExeName, const std::string &exeName) {
	for (size_t i = 0; i < vWhitelist.size(); i++) {
		std::string &val = vWhitelist.at(i);
		if (isAbsPath(val)) {
			if (val == absExeName) {
				return true;
			}
		} else {
			if (val == exeName) {
				return true;
			}
		}
	}
	return false;
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

static bool isWhitelistedParent(const std::string &absParentExeName, const std::string &parentExeName) {
	for (size_t i = 0; i < vLaunchers.size(); i++) {
		std::string &val = vLaunchers.at(i);
		if (isAbsPath(val)) {
			if (val == absParentExeName) {
				return true;
			}
		} else {
			if (val == parentExeName) {
				return true;
			}
		}
	}
	return false;
}

bool ExcludeCheckIsOverlayEnabled(std::string absExeName, std::string exeName) {
	bool enableOverlay = true;

	inPlaceLowerCase(absExeName);
	inPlaceLowerCase(exeName);

	ExcludeCheckEnsureInitialized();

	if (iExcludeMode == 0) { // Launcher filter

		std::string absParentExeName, parentExeName;
		if (!getParentProcessInfo(absParentExeName, parentExeName)) {
			// Unable to find parent. Process is allowed.
			ods("ExcludeCheck: Unable to find parent. Process allowed.");
			enableOverlay = true;
		}
		inPlaceLowerCase(absParentExeName);
		inPlaceLowerCase(parentExeName);

		ods("ExcludeCheck: Parent is '%s'", absParentExeName.c_str());

		// The blacklist always wins.
		// If an exe is in the blacklist, never show the overlay, ever.
		if (isBlacklistedExe(absExeName, exeName)) {
			ods("ExcludeCheck: '%s' is blacklisted. Overlay disabled.", absExeName.c_str());
			enableOverlay = false;
		// If the process's exe name is whitelisted, allow the overlay to be shown.
		} else if (isWhitelistedExe(absExeName, exeName)) {
			ods("ExcludeCheck: '%s' is whitelisted. Overlay enabled.", absExeName.c_str());
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
		} else if (isWhitelistedParent(absParentExeName, parentExeName)) {
			ods("ExcludeCheck: Parent '%s' of '%s' is whitelisted. Overlay enabled.", parentExeName.c_str(), exeName.c_str());
			enableOverlay = true;
		// If nothing matched, do not show the overlay.
		} else {
			ods("ExcludeCheck: No matching overlay exclusion rule found. Overlay disabled.");
			enableOverlay = false;
		}
	} else if (iExcludeMode == 1) {
		if (isWhitelistedExe(absExeName, exeName)) {
			enableOverlay = true;
		} else {
			enableOverlay = false;
		}
	} else if (iExcludeMode == 2) { // Blacklist
		if (isBlacklistedExe(absExeName, exeName)) {
			enableOverlay = false;
		} else {
			enableOverlay = true;
		}
	}

	return enableOverlay;
}
