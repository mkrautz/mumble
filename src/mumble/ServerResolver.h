// Copyright 2005-2016 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_SERVERRESOLVER_H_
#define MUMBLE_MUMBLE_SERVERRESOLVER_H_

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QPair>

class ServerResolver : public QObject {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(ServerResolver)
	public:
		ServerResolver(QObject *parent = NULL);

		void resolve(QString hostname, quint16 port);
		bool isEmpty() const;
		QPair<QString, quint16> pop();

	signals:
		/// Resolved is fired once the ServerResolver
		/// has resolved the server address.
		void resolved();

	protected:
		QString m_origHostname;
		quint16 m_origPort;
		int m_index;
		QList<QPair<QString, quint16> > m_targets;
	public slots:
		void srvResolved();
};

#endif