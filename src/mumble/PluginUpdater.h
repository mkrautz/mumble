// Copyright 2005-2016 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_PLUGINS_H_
#define MUMBLE_MUMBLE_PLUGINS_H_

#include <QtCore/QObject>
#include <QtCore/QUrl>

struct PluginFetchMeta;

class PluginUpdater : public QThread {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(PluginUpdater)
	protected:
		QList<PluginInfo *> qlPlugins;
		void clearPlugins();
		QMap<QString, PluginFetchMeta> qmPluginFetchMeta;
		QString qsSystemPlugins;
		QString qsUserPlugins;

		PluginUpdater(QObject *p = NULL);
		~PluginUpdater() Q_DECL_OVERRIDE;

	public slots:
		void checkUpdates();
		void fetchedUpdatePAPlugins(QByteArray, QUrl);
		void fetchedPAPluginDL(QByteArray, QUrl);
};

#endif
