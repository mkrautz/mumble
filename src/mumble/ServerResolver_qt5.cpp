// Copyright 2005-2016 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "ServerResolver.h"
#include "NetworkConfig.h"

ServerResolver::ServerResolver(QObject *parent)
	: QObject(parent)
	, m_origPort(0)
	, m_index(0) {
}

void ServerResolver::resolve(QString hostname, quint16 port) {
	m_origHostname = hostname;
	m_origPort = port;
	m_targets.clear();

	if (NetworkConfig::EnableDNSSRVRecords()) {
		QDnsLookup *resolver = new QDnsLookup(this);
		connect(resolver, SIGNAL(finished()), this, SLOT(srvResolved()));
		resolver->setType(QDnsLookup::SRV);

		resolver->setName(QLatin1String("_mumble._tcp.") + hostname);
		resolver->lookup();
	} else {
		emit resolved();
	}
}

void ServerResolver::srvResolved() {
	QDnsLookup *resolver = qobject_cast<QDnsLookup *>(sender());

	if (resolver->error() == QDnsLookup::NoError) {
		QList<QDnsServiceRecord> records = resolver->serviceRecords();
		foreach (const QDnsServiceRecord &record, records) {
			m_targets << QPair<QString, quint16>(record.target(), record.port());
		}
	} else {
		m_targets << QPair<QString, quint16>(m_origHostname, m_origPort);
	}

	delete resolver;

	emit resolved();
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
