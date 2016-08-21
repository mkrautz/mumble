// Copyright 2005-2016 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "TextToSpeech.h"

#include <servprov.h>

#define SPF_ASYNC 1

extern "C" {
	extern const IID IID_ISpVoice;
	extern const CLSID CLSID_SpVoice;
}

MIDL_INTERFACE("6C44DF74-72B9-4992-A1EC-EF996E0422D4")
ISpVoice : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE Speak(LPCWSTR pwcs, DWORD dwFlags, ULONG *pulStreamNumber) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetVolume(USHORT usVolume) = 0;
};

#undef FAILED
#define FAILED(Status) (static_cast<HRESULT>(Status)<0)

class TextToSpeechPrivate {
	public:
		ISpVoice * pVoice;
		TextToSpeechPrivate();
		~TextToSpeechPrivate();
		void say(const QString &text);
		void setVolume(int v);
};

TextToSpeechPrivate::TextToSpeechPrivate() {
	pVoice = NULL;

	HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, reinterpret_cast<void **>(&pVoice));
	if (FAILED(hr))
		qWarning("TextToSpeechPrivate: Failed to allocate TTS Voice");
}

TextToSpeechPrivate::~TextToSpeechPrivate() {
	if (pVoice)
		pVoice->Release();
}

void TextToSpeechPrivate::say(const QString &text) {
	if (pVoice) {
		pVoice->Speak((const wchar_t *) text.utf16(), SPF_ASYNC, NULL);
	}
}

void TextToSpeechPrivate::setVolume(int volume) {
	if (pVoice)
		pVoice->SetVolume(volume);
}

TextToSpeech::TextToSpeech(QObject *p) : QObject(p) {
	enabled = true;
	d = new TextToSpeechPrivate();
}

TextToSpeech::~TextToSpeech() {
	delete d;
}

void TextToSpeech::say(const QString &text) {
	if (enabled)
		d->say(text);
}

void TextToSpeech::setEnabled(bool e) {
	enabled = e;
}

void TextToSpeech::setVolume(int volume) {
	d->setVolume(volume);
}

bool TextToSpeech::isEnabled() const {
	return enabled;
}
