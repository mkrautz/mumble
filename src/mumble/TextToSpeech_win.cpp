// Copyright 2005-2017 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "TextToSpeech.h"

//#include <servprov.h>
//#include <sapi.h>

#undef FAILED
#define FAILED(Status) (static_cast<HRESULT>(Status)<0)

class TextToSpeechPrivate {
	public:
		TextToSpeechPrivate();
		~TextToSpeechPrivate();
		void say(const QString &text);
		void setVolume(int v);
};

TextToSpeechPrivate::TextToSpeechPrivate() {
#ifdef _MSC_VER
	pVoice = NULL;

	HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&pVoice);
	if (FAILED(hr))
		qWarning("TextToSpeechPrivate: Failed to allocate TTS Voice");
#endif
}

TextToSpeechPrivate::~TextToSpeechPrivate() {
#ifdef _MSC_VER
	if (pVoice)
		pVoice->Release();
#endif
}

void TextToSpeechPrivate::say(const QString &text) {
#ifdef _MSC_VER
	if (pVoice) {
		pVoice->Speak((const wchar_t *) text.utf16(), SPF_ASYNC, NULL);
	}
#endif
}

void TextToSpeechPrivate::setVolume(int volume) {
#ifdef _MSC_VER
	if (pVoice)
		pVoice->SetVolume(volume);
#endif
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
