// Copyright 2005-2017 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "lib.h"
#include "settings.h"
#include "util.h"

#include <tlhelp32.h>
#include <algorithm>
#include <string>
#include <vector>

static bool bExcludeCheckInitialized = false;

static OverlayExclusionMode oemExcludeMode = LauncherFilterExclusionMode;
static std::vector<std::string> vLaunchers;
static std::vector<std::string> vWhitelist;
static std::vector<std::string> vPaths;
static std::vector<std::string> vBlacklist;

/// Ensure the ExcludeCheck module is initialized.
static void ExcludeCheckEnsureInitialized() {
	if (bExcludeCheckInitialized) {
		return;
	}

	oemExcludeMode = SettingsGetExclusionMode();
	vLaunchers = SettingsGetLaunchers();
	vWhitelist = SettingsGetWhitelist();
	vPaths = SettingsGetPaths();
	vBlacklist = SettingsGetBlacklist();

	bExcludeCheckInitialized = true;
}

/// Find the PROCESSENTRY32 entry for the parent of the process with the |childpid| PID.
///
/// Returns true on success, and fills out |parent| with the correct PROCESSENTRY32.
/// Returns false on failuire, and does not touch |parent|.
static bool findParentProcessForChild(DWORD childpid, PROCESSENTRY32 *parent) {
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
			if (pe.th32ProcessID == childpid) {
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

/// Find the MODULEENTRY32 for the PROCESSENTRY32 |parent|.
/// The MODULEENTRY32 allows us to access the absolute path of the parent executable.
///
/// Returns true on success, and fills out |module| with the proper MODULEENTRY32 entry.
/// Returns false on failure, and does not touch |module|.
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

/// Return the absolute and relative exe names of this process's ancestors in
/// |absAncestorExeNames| and |ancestorExeNames|.
///
/// Returns true on success and fills out |absAncestorExeNames| and |ancestorExeNames|.
/// Returns false on failure, and does not change |absAncestorExeNames| and |ancestorExeNames|.
static bool getAncestorChain(std::vector<std::string> &absAncestorExeNames, std::vector<std::string> &ancestorExeNames) {
	PROCESSENTRY32 parent;
	MODULEENTRY32 module;

	std::vector<std::string> abs;
	std::vector<std::string> rel;

	bool ok = true;
	DWORD childpid = GetCurrentProcessId();
	while (ok) {
		ok = findParentProcessForChild(childpid, &parent);
		if (ok) {
			ok = getModuleForParent(&parent, &module);
			if (ok) {
				abs.push_back(std::string(module.szExePath));
				rel.push_back(std::string(parent.szExeFile));
				childpid = parent.th32ProcessID;
			}
		}
	}

	ok = abs.size() > 0;
	if (ok) {
		absAncestorExeNames = abs;
		ancestorExeNames = rel;
	}

	return ok;
}

/// Check whether the program at |absExeName|
/// (with basename of |exeName|) is in the Mumble
/// overlay program blacklist.
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

/// Check whether the program at |absExeName|
/// (with basename of |exeName|) is in the Mumble
/// overlay program whitelist.
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

/// Check whether the program at |absExeName|
/// is in the Mumble overlay path whitelist.
static bool isWhitelistedPath(const std::string &absExeName) {
	for (size_t i = 0; i < vPaths.size(); i++) {
		const std::string &path = vPaths.at(i);
		if (absExeName.find(path) == 0) {
			return true;
		}
	}
	return false;
}

/// Check whether an ancestor executable in
/// |absAncestorExeNames| (or |ancestorExeNames|,
/// for relative paths) is in the Mumble overlay's
/// launcher whitelist.
static bool hasWhitelistedAncestor(const std::vector<std::string> &absAncestorExeNames, const std::vector<std::string> &ancestorExeNames) {
	for (size_t i = 0; i < vLaunchers.size(); i++) {
		std::string &val = vLaunchers.at(i);
		if (isAbsPath(val)) {
			if (std::find(absAncestorExeNames.begin(), absAncestorExeNames.end(), val) != absAncestorExeNames.end()) {
				return true;
			}
		} else {
			if (std::find(ancestorExeNames.begin(), ancestorExeNames.end(), val) != ancestorExeNames.end()) {
				return true;
			}
		}
	}
	return false;
}

bool ExcludeCheckIsOverlayEnabled(std::string absExeName_, std::string exeName_) {
	bool enableOverlay = true;

	std::string absExeName = slowercase(absExeName_);
	std::string exeName = slowercase(exeName_);

	ExcludeCheckEnsureInitialized();

	switch (oemExcludeMode) {
		case LauncherFilterExclusionMode: {
			ods("ExcludeCheck: using 'launcher filter' exclusion mode...");

			std::vector<std::string> absAncestorExeNames;
			std::vector<std::string> ancestorExeNames;
			if (!getAncestorChain(absAncestorExeNames, ancestorExeNames)) {
				// Unable to get ancestor chain. Process is allowed.
				ods("ExcludeCheck: Unable to find parent. Process allowed.");
				enableOverlay = true;
			}
			absAncestorExeNames = vlowercase(absAncestorExeNames);
			ancestorExeNames = vlowercase(ancestorExeNames);

			for (size_t i = 0; i < absAncestorExeNames.size(); i++) {
				std::string absAncestorExeName = absAncestorExeNames.at(i);
				ods("ExcludeCheck: Ancestor #%i is '%s'", i+1, absAncestorExeName.c_str());
			}

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
			// If any of the process's ancestors are whitelisted, allow the process through.
			// This allows us to whitelist Steam.exe, etc. -- and have the overlay
			// show up in all Steam titles.
			} else if (hasWhitelistedAncestor(absAncestorExeNames, ancestorExeNames)) {
				ods("ExcludeCheck: An ancestor of '%s' is whitelisted. Overlay enabled.", exeName.c_str());
				enableOverlay = true;
			// If nothing matched, do not show the overlay.
			} else {
				ods("ExcludeCheck: No matching overlay exclusion rule found. Overlay disabled.");
				enableOverlay = false;
			}
			break;
		}
		case WhitelistExclusionMode: {
			ods("ExcludeCheck: using 'whitelist' exclusion mode...");

			if (isWhitelistedExe(absExeName, exeName)) {
				ods("ExcludeCheck: '%s' is whitelisted. Overlay enabled.", absExeName.c_str());
				enableOverlay = true;
			} else {
				ods("ExcludeCheck: '%s' not whitelisted. Overlay disabled.", absExeName.c_str());
				enableOverlay = false;
			}
			break;
		}
		case BlacklistExclusionMode: {
			ods("ExcludeCheck: using 'blacklist' exclusion mode...");

			if (isBlacklistedExe(absExeName, exeName)) {
				ods("ExcludeCheck: '%s' is blacklisted. Overlay disabled.", absExeName.c_str());
				enableOverlay = false;
			} else {
				ods("ExcludeCheck: '%s' is not blacklisted. Overlay enabled.", absExeName.c_str());
				enableOverlay = true;
			}
			break;
		}
	}

	return enableOverlay;
}
