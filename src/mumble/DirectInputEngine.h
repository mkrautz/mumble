// Copyright 2005-2017 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_DIRECTINPUT_H_
#define MUMBLE_MUMBLE_DIRECTINPUT_H_

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QVariant>
#include <QtCore/QThread>

struct InputDevice {
	LPDIRECTINPUTDEVICE8 pDID;

	QString name;	

	GUID guid;
	QVariant vguid;

	GUID guidproduct;
	QVariant vguidproduct;

	uint16_t vendor_id;
	uint16_t product_id;

	// dwType to name
	QHash<DWORD, QString> qhNames;

	// Map dwType to dwOfs in our structure
	QHash<DWORD, DWORD> qhTypeToOfs;

	// Map dwOfs in our structure to dwType
	QHash<DWORD, DWORD> qhOfsToType;

	// Buttons active since last reset
	QSet<DWORD> activeMap;
};


class DirectInputEngine : public QThread {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(DirectInputEngine)
	
	public:
		LPDIRECTINPUT8 pDI;
		QHash<GUID, InputDevice *> qhInputDevices;
		unsigned int uiHardwareDevices;
		/// nxboxinput holds the number of XInput devices
		/// available on the system. It is filled out by
		/// our EnumDevices callback.
		int nxboxinput;

		static BOOL CALLBACK EnumDevicesCB(LPCDIDEVICEINSTANCE, LPVOID);
		static BOOL CALLBACK EnumDeviceObjectsCallback(LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef);

		void run() Q_DECL_OVERRIDE;

		DirectInputEngine();
		~DirectInputEngine() Q_DECL_OVERRIDE;

		QString buttonName(const QVariant &);
		void handleButton(const QVariant &, bool);

	public slots:
		void timeTicked();
};

#endif
