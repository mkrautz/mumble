// Copyright 2005-2016 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "ServerResolver.h"

ServerResolver::ServerResolver(QObject *parent)
	: QObject(parent)
	, m_origPort(0)
	, m_index(0) {
}

void ServerResolver::resolve(QString hostname, quint16 port) {
	m_origHostname = hostname;
	m_origPort = port;
	m_targets.clear();

	m_targets << QPair<QString, quint16>(m_origHostname, m_origPort);

	emit resolved();
}

void ServerResolver::srvResolved () {
}

QPair<QString, quint16> ServerResolver::pop() {
	return QPair<QString, quint16>
}

bool ServerResolver::isEmpty() const {
	return m_targets.isEmpty();
}

QPair<QString, quint16> ServerResolver::pop() {
	if (m_targets.isEmpty()) {
		return QPair<QString, quint16>(QString(), 0);
	}

	QPair<QString, quint16> target = m_targets[0];
	m_targets.removeFirst();
	return target;
}
