// Copyright 2005-2017 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "DirectInputEngine.h"

#include "MainWindow.h"
#include "Global.h"

// 3rdparty/xinputcheck-src.
#include <xinputcheck.h>

#define DX_SAMPLE_BUFFER_SIZE 512

// from os_win.cpp
extern HWND mumble_mw_hwnd;

DirectInputEngine::DirectInputEngine()
	: pDI(NULL)
	, uiHardwareDevices(0) {

	moveToThread(this);
	start(QThread::LowestPriority);
}

DirectInputEngine::~DirectInputEngine() {
	quit();
	wait();
}

void DirectInputEngine::run() {
	if (FAILED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, reinterpret_cast<void **>(&pDI), NULL))) {
		qFatal("DirectInputEngine: Failed to create d8input");
		return;
	}

	// Wait for MainWindow's constructor to finish before we enumerate DirectInput devices.
	// We need to do this because adding a new device requires a Window handle. (SetCooperativeLevel())
	while (! g.mw)
		this->yieldCurrentThread();

	QTimer *timer = new QTimer;
	connect(timer, SIGNAL(timeout()), this, SLOT(timeTicked()));
	timer->start(20);

	setPriority(QThread::TimeCriticalPriority);

	exec();

	delete timer;

	foreach(InputDevice *id, qhInputDevices) {
		if (id->pDID) {
			id->pDID->Unacquire();
			id->pDID->Release();
		}
		delete id;
	}
	pDI->Release();
}

BOOL CALLBACK DirectInputEngine::EnumDeviceObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef) {
	InputDevice *id = static_cast<InputDevice *>(pvRef);
	QString name = QString::fromUtf16(reinterpret_cast<const ushort *>(lpddoi->tszName));
	id->qhNames[lpddoi->dwType] = name;

	if (g.s.bDirectInputVerboseLogging) {
		qWarning("DirectInputEngine: EnumObjects: device %s %s object 0x%.8lx %s",
		         qPrintable(QUuid(id->guid).toString()),
		         qPrintable(id->name),
		         static_cast<unsigned long>(lpddoi->dwType),
		         qPrintable(name));
	}

	return DIENUM_CONTINUE;
}

BOOL DirectInputEngine::EnumDevicesCB(LPCDIDEVICEINSTANCE pdidi, LPVOID pContext) {
	DirectInputEngine *die = static_cast<DirectInputEngine *>(pContext);
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
		die->nxboxinput += 1;

		qWarning("DirectInputEngine: excluded XInput device '%s' (guid %s guid product %s) from DirectInput",
		         qPrintable(id->name),
		         qPrintable(id->vguid.toString()),
		         qPrintable(id->vguidproduct.toString()));
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
			qWarning("DirectInputEngine: rejected blacklisted device %s (GUID: %s, PGUID: %s, VID: 0x%.4x, PID: 0x%.4x, TYPE: 0x%.8lx)",
			         qPrintable(id->name),
			         qPrintable(id->vguid.toString()),
			         qPrintable(id->vguidproduct.toString()),
			         id->vendor_id,
			         id->product_id,
			         static_cast<unsigned long>(pdidi->dwDevType));
			delete id;
			return DIENUM_CONTINUE;
		}
	}

	foreach(InputDevice *dev, die->qhInputDevices) {
		if (dev->guid == id->guid) {
			delete id;
			return DIENUM_CONTINUE;
		}
	}

	if (FAILED(hr = die->pDI->CreateDevice(pdidi->guidInstance, &id->pDID, NULL)))
		qFatal("DirectInputEngine: CreateDevice: %lx", hr);

	if (FAILED(hr = id->pDID->EnumObjects(EnumDeviceObjectsCallback, static_cast<void *>(id), DIDFT_BUTTON)))
		qFatal("DirectInputEngine: EnumObjects: %lx", hr);

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

		if (FAILED(hr = id->pDID->SetCooperativeLevel(mumble_mw_hwnd, DISCL_NONEXCLUSIVE|DISCL_BACKGROUND)))
			qFatal("DirectInputEngine: SetCooperativeLevel: %lx", hr);

		if (FAILED(hr = id->pDID->SetDataFormat(&df)))
			qFatal("DirectInputEngine: SetDataFormat: %lx", hr);

		DIPROPDWORD dipdw;

		dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
		dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		dipdw.diph.dwObj        = 0;
		dipdw.diph.dwHow        = DIPH_DEVICE;
		dipdw.dwData            = DX_SAMPLE_BUFFER_SIZE;

		if (FAILED(hr = id->pDID->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph)))
			qFatal("DirectInputEngine: SetProperty: %lx", hr);

		qWarning("Adding device %s %s %s:%d type 0x%.8lx guid product %s",
		         qPrintable(QUuid(id->guid).toString()),
		         qPrintable(name),
		         qPrintable(sname),
		         id->qhNames.count(),
		         static_cast<unsigned long>(pdidi->dwDevType),
		         qPrintable(id->vguidproduct.toString()));

		die->qhInputDevices[id->guid] = id;
	} else {
		id->pDID->Release();
		delete id;
	}

	return DIENUM_CONTINUE;
}

void DirectInputEngine::timeTicked() {
	if (g.mw->uiNewHardware != uiHardwareDevices) {
		uiHardwareDevices = g.mw->uiNewHardware;

		XInputCheck_ClearDeviceCache();
		nxboxinput = 0;

		pDI->EnumDevices(DI8DEVCLASS_ALL, EnumDevicesCB, static_cast<void *>(this), DIEDFL_ATTACHEDONLY);
	}

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
				qWarning("DirectInputEngine: Removing device %s", qPrintable(QUuid(id->guid).toString()));
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
				qWarning("DirectInputEngine: Poll() for device %s took %li msec. This is abnormal, the device is possibly misbehaving...", qPrintable(QUuid(id->guid).toString()), static_cast<long>(timer.elapsed()));
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
}

QString DirectInputEngine::buttonName(const QVariant &v) {
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

	InputDevice *id = qhInputDevices.value(guid);
	if (guid == GUID_SysMouse)
		device=QLatin1String("M:");
	else if (guid == GUID_SysKeyboard)
		device=QLatin1String("K:");
	else if (id)
		device=id->name+QLatin1String(":");
	if (id) {
		QString result = id->qhNames.value(type);
		if (!result.isEmpty()) {
			name = result;
		}
	}
	return device+name;
}

void DirectInputEngine::handleButton(const QVariant &, bool) {
}
