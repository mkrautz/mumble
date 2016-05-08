// Copyright 2005-2016 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "GlobalShortcut_win.h"

#include "MainWindow.h"
#include "OverlayClient.h"
#include "Global.h"

// 3rdparty/xinputcheck-src.
#include <xinputcheck.h>

#undef FAILED
#define FAILED(Status) (static_cast<HRESULT>(Status)<0)

#define DX_SAMPLE_BUFFER_SIZE 512

// from os_win.cpp
extern HWND MumbleHWNDForQWidget(QWidget *w);

static uint qHash(const GUID &a) {
	uint val = a.Data1 ^ a.Data2 ^ a.Data3;
	for (int i=0;i<8;i++)
		val += a.Data4[i];
	return val;
}

/**
 * Returns a platform specific GlobalShortcutEngine object.
 *
 * @see GlobalShortcutX
 * @see GlobalShortcutMac
 * @see GlobalShortcutWin
 */
GlobalShortcutEngine *GlobalShortcutEngine::platformInit() {
	return new GlobalShortcutWin();
}


GlobalShortcutWin::GlobalShortcutWin()
	: pDI(NULL)
#ifdef USE_GKEY
	, gkey(NULL)
#endif
#ifdef USE_XBOXINPUT
	, xboxinput(NULL)
	, nxboxinput(0)
#endif
	, uiHardwareDevices(0) {

	// Hidden setting to disable hooking
	bHook = g.qs->value(QLatin1String("winhooks"), true).toBool();

	GetKeyboardState(ucKeyState);

	moveToThread(this);
	start(QThread::LowestPriority);
}

GlobalShortcutWin::~GlobalShortcutWin() {
	quit();
	wait();
}

void GlobalShortcutWin::run() {
	if (FAILED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void **>(&pDI), NULL))) {
		qFatal("GlobalShortcutWin: Failed to create d8input");
		return;
	}

	// Print the user's LowLevelHooksTimeout registry key for debugging purposes.
	// On Windows 7 and greater, Windows will silently remove badly behaving hooks
	// without telling the application. Users can tweak the timeout themselves
	// with this registry key.
	HKEY key = NULL;
	DWORD type = 0;
	DWORD value = 0;
	DWORD len = sizeof(DWORD);
	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Control Panel\\Desktop", NULL, KEY_READ, &key) == ERROR_SUCCESS) {
		LONG err = RegQueryValueExA(key, "LowLevelHooksTimeout", NULL, &type, reinterpret_cast<LPBYTE>(&value), &len);
		if (err == ERROR_SUCCESS && type == REG_DWORD) {
			qWarning("GlobalShortcutWin: Found LowLevelHooksTimeout with value = 0x%x", value);
		} else if (err == ERROR_FILE_NOT_FOUND) {
			qWarning("GlobalShortcutWin: No LowLevelHooksTimeout registry key found.");
		} else {
			qWarning("GlobalShortcutWin: Error looking up LowLevelHooksTimeout. (Error: 0x%x, Type: 0x%x, Value: 0x%x)", err, type, value);
		}
	}

	// Wait for MainWindow's constructor to finish before we enumerate DirectInput devices.
	// We need to do this because adding a new device requires a Window handle. (SetCooperativeLevel())
	while (! g.mw)
		this->yieldCurrentThread();

	if (bHook) {
		HMODULE hSelf;
		GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (wchar_t *) &HookKeyboard, &hSelf);
		hhKeyboard = SetWindowsHookEx(WH_KEYBOARD_LL, HookKeyboard, hSelf, 0);
		hhMouse = SetWindowsHookEx(WH_MOUSE_LL, HookMouse, hSelf, 0);
	}

#ifdef USE_GKEY
	if (g.s.bEnableGKey) {
		gkey = new GKeyLibrary();
		qWarning("GlobalShortcutWin: GKeys initialized, isValid: %d", gkey->isValid());
	}
#endif

#ifdef USE_XBOXINPUT
	if (g.s.bEnableXboxInput) {
		xboxinput = new XboxInput();
		ZeroMemory(&xboxinputLastPacket, sizeof(xboxinputLastPacket));
		qWarning("GlobalShortcutWin: XboxInput initialized, isValid: %d", xboxinput->isValid());
	}
#endif

	QTimer * timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(timeTicked()));
	timer->start(20);

	setPriority(QThread::TimeCriticalPriority);

	exec();

#ifdef USE_GKEY
	delete gkey;
#endif

#ifdef USE_XBOXINPUT
	delete xboxinput;
#endif

	if (bHook) {
		UnhookWindowsHookEx(hhKeyboard);
		UnhookWindowsHookEx(hhMouse);
	}

	foreach(InputDevice *id, qhInputDevices) {
		if (id->pDID) {
			id->pDID->Unacquire();
			id->pDID->Release();
		}
		delete id;
	}
	pDI->Release();
}

LRESULT CALLBACK GlobalShortcutWin::HookKeyboard(int nCode, WPARAM wParam, LPARAM lParam) {
	GlobalShortcutWin *gsw=static_cast<GlobalShortcutWin *>(engine);
	KBDLLHOOKSTRUCT *key=reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
	BYTE *ucKeyState = gsw->ucKeyState;

#ifndef QT_NO_DEBUG
	static int safety = 0;

	if ((++safety < 100) && (nCode >= 0)) {
#else
	if (nCode >= 0) {
#endif
		UINT msg = wParam;
		WPARAM w = key->vkCode;
		LPARAM l = 1 | (key->scanCode << 16);
		if (key->flags & LLKHF_EXTENDED)
			l |= 0x1000000;
		if (wParam == WM_KEYUP)
			l |= 0xC0000000;

		bool nomsg = false;

		switch (w) {
			case VK_LCONTROL:
			case VK_RCONTROL:
				if ((msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN))
					ucKeyState[w] |= 0x80;
				else {
					ucKeyState[w] &= 0x7f;

					if ((ucKeyState[VK_LCONTROL] & 0x80) || (ucKeyState[VK_RCONTROL] & 0x80)) {
						nomsg = true;
						break;
					}
				}

				w = VK_CONTROL;
				break;
			case VK_LSHIFT:
			case VK_RSHIFT:
				if ((msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN))
					ucKeyState[w] |= 0x80;
				else {
					ucKeyState[w] &= 0x7f;

					if ((ucKeyState[VK_LSHIFT] & 0x80) || (ucKeyState[VK_RSHIFT] & 0x80)) {
						nomsg = true;
						break;
					}
				}

				w = VK_SHIFT;
				break;
			case VK_LMENU:
			case VK_RMENU:
				if ((msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN))
					ucKeyState[w] |= 0x80;
				else {
					ucKeyState[w] &= 0x7f;

					if ((ucKeyState[VK_LMENU] & 0x80) || (ucKeyState[VK_RMENU] & 0x80)) {
						nomsg = true;
						break;
					}
				}

				w = VK_MENU;
				break;
			default:
				break;
		}

		if ((msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN)) {
			if (ucKeyState[w] & 0x80)
				l |= 0x40000000;
			ucKeyState[w] |= 0x80;
		} else if (((msg == WM_KEYUP) || (msg == WM_SYSKEYUP)) && !nomsg) {
			ucKeyState[w] &= 0x7f;
		}

		QList<QVariant> ql;
		unsigned int keyid = static_cast<unsigned int>((key->scanCode << 8) | 0x4);
		if (key->flags & LLKHF_EXTENDED)
			keyid |= 0x8000U;
		ql << keyid;
		ql << QVariant(QUuid(GUID_SysKeyboard));
		bool suppress = gsw->handleButton(ql, !(key->flags & LLKHF_UP));

		if (! suppress && g.ocIntercept) {
			// In full-GUI-overlay always suppress
			suppress = true;

			HWND hwnd = MumbleHWNDForQWidget(&g.ocIntercept->qgv);

			GUITHREADINFO gti;
			ZeroMemory(&gti, sizeof(gti));
			gti.cbSize = sizeof(gti);
			::GetGUIThreadInfo(::GetWindowThreadProcessId(hwnd, NULL), &gti);

			if (gti.hwndFocus)
				hwnd = gti.hwndFocus;

			if (! nomsg)
				::PostMessage(hwnd, msg, w, l);
		}

		if (suppress)
			return 1;

	}
	return CallNextHookEx(gsw->hhKeyboard, nCode, wParam, lParam);
}

LRESULT CALLBACK GlobalShortcutWin::HookMouse(int nCode, WPARAM wParam, LPARAM lParam) {
	GlobalShortcutWin *gsw=static_cast<GlobalShortcutWin *>(engine);
	MSLLHOOKSTRUCT *mouse=reinterpret_cast<MSLLHOOKSTRUCT *>(lParam);
	BYTE *ucKeyState = gsw->ucKeyState;

	if (nCode >= 0) {
		bool suppress = false;
		UINT msg = wParam;

		switch (msg) {
			case WM_LBUTTONDOWN:
				ucKeyState[VK_LBUTTON] |= 0x80;
				if (gsw->tDoubleClick.restart() < (QApplication::doubleClickInterval() * 1000ULL))
					msg = WM_LBUTTONDBLCLK;
				break;
			case WM_LBUTTONUP:
				ucKeyState[VK_LBUTTON] &= 0x7f;
				break;
			case WM_RBUTTONDOWN:
				ucKeyState[VK_RBUTTON] |= 0x80;
				break;
			case WM_RBUTTONUP:
				ucKeyState[VK_RBUTTON] &= 0x7f;
				break;
			case WM_MBUTTONDOWN:
				ucKeyState[VK_MBUTTON] |= 0x80;
				break;
			case WM_MBUTTONUP:
				ucKeyState[VK_MBUTTON] &= 0x7f;
				break;
			case WM_XBUTTONDOWN:
				if ((mouse->mouseData >> 16) == XBUTTON1)
					ucKeyState[VK_XBUTTON1] |= 0x80;
				else if ((mouse->mouseData >> 16) == XBUTTON2)
					ucKeyState[VK_XBUTTON2] |= 0x80;
				break;
			case WM_XBUTTONUP:
				if ((mouse->mouseData >> 16) == XBUTTON1)
					ucKeyState[VK_XBUTTON1] &= 0x7f;
				else if ((mouse->mouseData >> 16) == XBUTTON2)
					ucKeyState[VK_XBUTTON2] &= 0x7f;
				break;
			default:
				break;
		}

		if (g.ocIntercept) {
			// In full-GUI-overlay always suppress
			suppress = true;

			POINT p;
			GetCursorPos(&p);

			int dx = mouse->pt.x - p.x;
			int dy = mouse->pt.y - p.y;

			g.ocIntercept->iMouseX = qBound<int>(0, g.ocIntercept->iMouseX + dx, g.ocIntercept->uiWidth - 1);
			g.ocIntercept->iMouseY = qBound<int>(0, g.ocIntercept->iMouseY + dy, g.ocIntercept->uiHeight - 1);

			WPARAM w = 0;
			LPARAM l = (static_cast<short>(g.ocIntercept->iMouseX) & 0xFFFF) | ((g.ocIntercept->iMouseY << 16) & 0xFFFF0000);

			if (ucKeyState[VK_CONTROL] & 0x80)
				w |= MK_CONTROL;
			if (ucKeyState[VK_LBUTTON] & 0x80)
				w |= MK_LBUTTON;
			if (ucKeyState[VK_MBUTTON] & 0x80)
				w |= MK_MBUTTON;
			if (ucKeyState[VK_RBUTTON] & 0x80)
				w |= MK_RBUTTON;
			if (ucKeyState[VK_SHIFT] & 0x80)
				w |= MK_SHIFT;
			if (ucKeyState[VK_XBUTTON1] & 0x80)
				w |= MK_XBUTTON1;
			if (ucKeyState[VK_XBUTTON2] & 0x80)
				w |= MK_XBUTTON2;

			w |= (mouse->mouseData & 0xFFFF0000);

			HWND hwnd = MumbleHWNDForQWidget(&g.ocIntercept->qgv);

			GUITHREADINFO gti;
			ZeroMemory(&gti, sizeof(gti));
			gti.cbSize = sizeof(gti);
			::GetGUIThreadInfo(::GetWindowThreadProcessId(hwnd, NULL), &gti);

			if (gti.hwndCapture)
				hwnd = gti.hwndCapture;

			::PostMessage(hwnd, msg, w, l);

			QMetaObject::invokeMethod(g.ocIntercept, "updateMouse", Qt::QueuedConnection);
		}

		bool down = false;
		unsigned int btn = 0;
		switch (msg) {
			case WM_LBUTTONDOWN:
				down = true;
			case WM_LBUTTONUP:
				btn = 3;
				break;
			case WM_RBUTTONDOWN:
				down = true;
			case WM_RBUTTONUP:
				btn = 4;
				break;
			case WM_MBUTTONDOWN:
				down = true;
			case WM_MBUTTONUP:
				btn = 5;
				break;
			case WM_XBUTTONDOWN:
				down = true;
			case WM_XBUTTONUP:
				btn = 5 + (mouse->mouseData >> 16);
			default:
				break;
		}
		if (btn) {
			QList<QVariant> ql;
			ql << static_cast<unsigned int>((btn << 8) | 0x4);
			ql << QVariant(QUuid(GUID_SysMouse));
			bool wantsuppress = gsw->handleButton(ql, down);
			// Do not suppress LBUTTONUP though (so suppression can be deactvated via mouse).
			if (! suppress)
				suppress = wantsuppress && (btn != 3);
		}
		if (suppress)
			return 1;
	}
	return CallNextHookEx(gsw->hhMouse, nCode, wParam, lParam);
}

BOOL CALLBACK GlobalShortcutWin::EnumDeviceObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef) {
	InputDevice *id=static_cast<InputDevice *>(pvRef);
	QString name = QString::fromUtf16(reinterpret_cast<const ushort *>(lpddoi->tszName));
	id->qhNames[lpddoi->dwType] = name;

	return DIENUM_CONTINUE;
}

BOOL GlobalShortcutWin::EnumDevicesCB(LPCDIDEVICEINSTANCE pdidi, LPVOID pContext) {
	GlobalShortcutWin *cbgsw=static_cast<GlobalShortcutWin *>(pContext);
	HRESULT hr;

	QString name = QString::fromUtf16(reinterpret_cast<const ushort *>(pdidi->tszProductName));
	QString sname = QString::fromUtf16(reinterpret_cast<const ushort *>(pdidi->tszInstanceName));

	InputDevice *id = new InputDevice;

	id->pDID = NULL;

	id->name = name;

	id->guid = pdidi->guidInstance;
	id->vguid = QVariant(QUuid(id->guid).toString());

	id->guidproduct = pdidi->guidProduct;
	id->vguidproduct = QVariant(QUuid(id->guidproduct).toString());

	// Is it an XInput device? Skip it.
	//
	// This check is not restricted to USE_XBOXINPUT because
	// Windows 10 (10586.122, ~March 2016) has issues with
	// using XInput devices via DirectInput.
	//
	// See issues mumble-voip/mumble#2104 and mumble-voip/mumble#2147
	// for more information.
	if (XInputCheck_IsGuidProductXInputDevice(&id->guidproduct)) {
		cbgsw->nxboxinput += 1;

		qWarning("GlobalShortcutWin: excluded XInput device '%s' (%s) from DirectInput", qPrintable(id->name), qPrintable(id->vguid.toString()));
		delete id;
		return DIENUM_CONTINUE;
	}

	// Check for PIDVID at the end of the GUID, as
	// per http://stackoverflow.com/q/25622780.
	BYTE pidvid[8] = { 0, 0, 'P', 'I', 'D', 'V', 'I', 'D' };
	if (memcmp(id->guidproduct.Data4, pidvid, 8) == 0) {
		uint16_t vendor_id = id->guidproduct.Data1 & 0xffff;
		uint16_t product_id = (id->guidproduct.Data1 >> 16) & 0xffff;

		id->vendor_id = vendor_id;
		id->product_id = product_id;
	} else {
		id->vendor_id = 0x00;
		id->product_id = 0x00;
	}

	// Reject devices if they are blacklisted.
	//
	// Device Name: ODAC-revB
	// Vendor/Product ID: 0x262A, 0x1048
	// https://github.com/mumble-voip/mumble/issues/1977
	//
	// Device Name: Aune T1 MK2 - HID-compliant consumer control device
	// Vendor/Product ID: 0x262A, 0x1168
	// https://github.com/mumble-voip/mumble/issues/1880
	//
	// For now, we simply disable the 0x262A vendor ID.
	//
	// 0x26A is SAVITECH Corp.
	// http://www.savitech-ic.com/, or
	// http://www.saviaudio.com/product.html
	// (via https://usb-ids.gowdy.us/read/UD/262a)
	//
	// In the future, if there are more devices in the
	// blacklist, we need a more structured aproach.
	{
		if (id->vendor_id == 0x262A) {
			qWarning("GlobalShortcutWin: rejected blacklisted device %s (GUID: %s, PGUID: %s, VID: 0x%.4x, PID: 0x%.4x)",
			         qPrintable(id->name), qPrintable(id->vguid.toString()), qPrintable(id->vguidproduct.toString()),
			         id->vendor_id, id->product_id);
			delete id;
			return DIENUM_CONTINUE;
		}
	}

	foreach(InputDevice *dev, cbgsw->qhInputDevices) {
		if (dev->guid == id->guid) {
			delete id;
			return DIENUM_CONTINUE;
		}
	}

	if (FAILED(hr = cbgsw->pDI->CreateDevice(pdidi->guidInstance, &id->pDID, NULL)))
		qFatal("GlobalShortcutWin: CreateDevice: %lx", hr);

	if (FAILED(hr = id->pDID->EnumObjects(EnumDeviceObjectsCallback, static_cast<void *>(id), DIDFT_BUTTON)))
		qFatal("GlobalShortcutWin: EnumObjects: %lx", hr);

	if (id->qhNames.count() > 0) {
		QList<DWORD> types = id->qhNames.keys();
		qSort(types);

		int nbuttons = types.count();
		STACKVAR(DIOBJECTDATAFORMAT, rgodf, nbuttons);
		DIDATAFORMAT df;
		ZeroMemory(&df, sizeof(df));
		df.dwSize = sizeof(df);
		df.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
		df.dwFlags=DIDF_ABSAXIS;
		df.dwDataSize = (nbuttons + 3) & (~0x3);
		df.dwNumObjs = nbuttons;
		df.rgodf = rgodf;
		for (int i=0;i<nbuttons;i++) {
			ZeroMemory(& rgodf[i], sizeof(DIOBJECTDATAFORMAT));
			DWORD dwType = types[i];
			DWORD dwOfs = i;
			rgodf[i].dwOfs = dwOfs;
			rgodf[i].dwType = dwType;
			id->qhOfsToType[dwOfs] = dwType;
			id->qhTypeToOfs[dwType] = dwOfs;
		}

		if (FAILED(hr = id->pDID->SetCooperativeLevel(MumbleHWNDForQWidget(g.mw), DISCL_NONEXCLUSIVE|DISCL_BACKGROUND)))
			qFatal("GlobalShortcutWin: SetCooperativeLevel: %lx", hr);

		if (FAILED(hr = id->pDID->SetDataFormat(&df)))
			qFatal("GlobalShortcutWin: SetDataFormat: %lx", hr);

		DIPROPDWORD dipdw;

		dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
		dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		dipdw.diph.dwObj        = 0;
		dipdw.diph.dwHow        = DIPH_DEVICE;
		dipdw.dwData            = DX_SAMPLE_BUFFER_SIZE;

		if (FAILED(hr = id->pDID->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph)))
			qFatal("GlobalShortcutWin: SetProperty: %lx", hr);

		qWarning("Adding device %s %s %s:%d", qPrintable(QUuid(id->guid).toString()),qPrintable(name),qPrintable(sname),id->qhNames.count());
		cbgsw->qhInputDevices[id->guid] = id;
	} else {
		id->pDID->Release();
		delete id;
	}

	return DIENUM_CONTINUE;
}

void GlobalShortcutWin::timeTicked() {
	if (g.mw->uiNewHardware != uiHardwareDevices) {
		uiHardwareDevices = g.mw->uiNewHardware;

		XInputCheck_ClearDeviceCache();
		nxboxinput = 0;

		pDI->EnumDevices(DI8DEVCLASS_ALL, EnumDevicesCB, static_cast<void *>(this), DIEDFL_ATTACHEDONLY);
	}

	if (bNeedRemap)
		remap();

	foreach(InputDevice *id, qhInputDevices) {
		DIDEVICEOBJECTDATA rgdod[DX_SAMPLE_BUFFER_SIZE];
		DWORD   dwItems = DX_SAMPLE_BUFFER_SIZE;
		HRESULT hr;

		hr = id->pDID->Acquire();

		switch (hr) {
			case DI_OK:
			case S_FALSE:
				break;
			case DIERR_UNPLUGGED:
			case DIERR_GENERIC:
				qWarning("Removing device %s", qPrintable(QUuid(id->guid).toString()));
				id->pDID->Release();
				qhInputDevices.remove(id->guid);
				delete id;
				return;
			case DIERR_OTHERAPPHASPRIO:
				continue;
			default:
				break;
		}

		{
			QElapsedTimer timer;
			timer.start();

			id->pDID->Poll();

			// If a call to Poll takes more than
			// a second, warn the user that they
			// might have a misbehaving device.
			if (timer.elapsed() > 1000) {
				qWarning("GlobalShortcut_win: Poll() for device %s took %li msec. This is abnormal, the device is possibly misbehaving...", qPrintable(QUuid(id->guid).toString()), static_cast<long>(timer.elapsed()));
			}
		}

		hr = id->pDID->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), rgdod, &dwItems, 0);
		if (FAILED(hr))
			continue;

		if (dwItems <= 0)
			continue;

		for (DWORD j=0; j<dwItems; j++) {
			QList<QVariant> ql;

			quint32 uiType = id->qhOfsToType.value(rgdod[j].dwOfs);
			ql << uiType;
			ql << id->vguid;
			handleButton(ql, rgdod[j].dwData & 0x80);
		}
	}

#ifdef USE_GKEY
	if (g.s.bEnableGKey && gkey->isValid()) {
		for (int button = GKEY_MIN_MOUSE_BUTTON; button <= GKEY_MAX_MOUSE_BUTTON; button++) {
			QList<QVariant> ql;
			ql << button;
			ql << GKeyLibrary::quMouse;
			handleButton(ql, gkey->isMouseButtonPressed(button));
		}
		for (int mode = GKEY_MIN_KEYBOARD_MODE; mode <= GKEY_MAX_KEYBOARD_MODE; mode++) {
			for (int key = GKEY_MIN_KEYBOARD_BUTTON; key <= GKEY_MAX_KEYBOARD_BUTTON; key++) {
				QList<QVariant> ql;
				// Store the key and mode in one int
				// bit 0..15: mode, bit 16..31: key
				ql << (key | (mode << 16));
				ql << GKeyLibrary::quKeyboard;
				handleButton(ql, gkey->isKeyboardGkeyPressed(key, mode));
			}
		}
	}
#endif

#ifdef USE_XBOXINPUT
	if (g.s.bEnableXboxInput && xboxinput->isValid() && nxboxinput > 0) {
		XboxInputState state;
		for (uint32_t i = 0; i < XBOXINPUT_MAX_DEVICES; i++) {
			if (xboxinput->GetState(i, &state) == 0) {
				// Skip the result of GetState() if the packet number hasn't changed,
				// or if we're at the first packet.
				if (xboxinputLastPacket[i] != 0 && state.packetNumber == xboxinputLastPacket[i]) {
					continue;
				}

				// The buttons field of XboxInputState contains a bit
				// for each button on the Xbox controller. The official
				// headers enumerate the bits via XINPUT_GAMEPAD_*.
				// The official mapping uses all 16-bits, but leaves
				// bit 10 and 11 (counting from 0) undocumented.
				//
				// It turns out that bit 10 is the guide button,
				// which can be queried using the non-public
				// XInputGetStateEx() function.
				//
				// Our mapping uses the bit number as a button index.
				// So 0x1 -> 0, 0x2 -> 1, 0x4 -> 2, and so on...
				//
				// However, since the buttons field is only a 16-bit value,
				// and we also want to use the left and right triggers as
				// buttons, we assign them the button indexes 16 and 17.
				uint32_t buttonMask = state.buttons;
				for (uint32_t j = 0; j < 18; j++) {
					QList<QVariant> ql;

					bool pressed = false;
					if (j >= 16) {
						if (j == 16) { // LeftTrigger
							pressed = state.leftTrigger > XBOXINPUT_TRIGGER_THRESHOLD;
						} else if (j == 17) { // RightTrigger
							pressed = state.rightTrigger > XBOXINPUT_TRIGGER_THRESHOLD;
						}
					} else {
						uint32_t currentButtonMask = (1 << j);
						pressed = (buttonMask & currentButtonMask) != 0;
					}

					uint32_t type = (i << 24) | j;
					ql << static_cast<uint>(type);
					ql << XboxInput::s_XboxInputGuid;
					handleButton(ql, pressed);
				}

				xboxinputLastPacket[i] = state.packetNumber;
			}
		}
	}
#endif
}

QString GlobalShortcutWin::buttonName(const QVariant &v) {
	GlobalShortcutWin *gsw = static_cast<GlobalShortcutWin *>(GlobalShortcutEngine::engine);

	const QList<QVariant> &sublist = v.toList();
	if (sublist.count() != 2)
		return QString();

	bool ok = false;
	DWORD type = sublist.at(0).toUInt(&ok);
	QUuid guid(sublist.at(1).toString());

	if (guid.isNull() || (!ok))
		return QString();

	QString device=guid.toString();
	QString name=QLatin1String("Unknown");

#ifdef USE_GKEY
	if (g.s.bEnableGKey && gkey->isValid()) {
		bool isGKey = false;
		if (guid == GKeyLibrary::quMouse) {
			isGKey = true;
			name = gkey->getMouseButtonString(type);
		} else if (guid == GKeyLibrary::quKeyboard) {
			isGKey = true;
			name = gkey->getKeyboardGkeyString(type & 0xFFFF, type >> 16);
		}
		if (isGKey) {
			device = QLatin1String("GKey:");
			return device + name; // Example output: "Gkey:G6/M1"
		}
	}
#endif

#ifdef USE_XBOXINPUT
	if (g.s.bEnableXboxInput && xboxinput->isValid() && guid == XboxInput::s_XboxInputGuid) {
		uint32_t idx = (type >> 24) & 0xff;
		uint32_t button = (type & 0x00ffffffff);

		// Translate from our own button index mapping to
		// the actual Xbox controller button names.
		// For a description of the mapping, see the state
		// querying code in GlobalShortcutWin::timeTicked().
		switch (button) {
			case 0:
				return QString::fromLatin1("Xbox%1:Up").arg(idx + 1);
			case 1:
				return QString::fromLatin1("Xbox%1:Down").arg(idx + 1);
			case 2:
				return QString::fromLatin1("Xbox%1:Left").arg(idx + 1);
			case 3:
				return QString::fromLatin1("Xbox%1:Right").arg(idx + 1);
			case 4:
				return QString::fromLatin1("Xbox%1:Start").arg(idx + 1);
			case 5:
				return QString::fromLatin1("Xbox%1:Back").arg(idx + 1);
			case 6:
				return QString::fromLatin1("Xbox%1:LeftThumb").arg(idx + 1);
			case 7:
				return QString::fromLatin1("Xbox%1:RightThumb").arg(idx + 1);
			case 8:
				return QString::fromLatin1("Xbox%1:LeftShoulder").arg(idx + 1);
			case 9:
				return QString::fromLatin1("Xbox%1:RightShoulder").arg(idx + 1);
			case 10:
				return QString::fromLatin1("Xbox%1:Guide").arg(idx + 1);
			case 11:
				return QString::fromLatin1("Xbox%1:11").arg(idx + 1);
			case 12:
				return QString::fromLatin1("Xbox%1:A").arg(idx + 1);
			case 13:
				return QString::fromLatin1("Xbox%1:B").arg(idx + 1);
			case 14:
				return QString::fromLatin1("Xbox%1:X").arg(idx + 1);
			case 15:
				return QString::fromLatin1("Xbox%1:Y").arg(idx + 1);
			case 16:
				return QString::fromLatin1("Xbox%1:LeftTrigger").arg(idx + 1);
			case 17:
				return QString::fromLatin1("Xbox%1:RightTrigger").arg(idx + 1);
		}
	}
#endif

	InputDevice *id = gsw->qhInputDevices.value(guid);
	if (guid == GUID_SysMouse)
		device=QLatin1String("M:");
	else if (guid == GUID_SysKeyboard)
		device=QLatin1String("K:");
	else if (id)
		device=id->name+QLatin1String(":");
	if (id) {
		name=id->qhNames.value(type);
	}
	return device+name;
}

bool GlobalShortcutWin::canSuppress() {
	return bHook;
}

void GlobalShortcutWin::prepareInput() {
	SetKeyboardState(ucKeyState);
}
