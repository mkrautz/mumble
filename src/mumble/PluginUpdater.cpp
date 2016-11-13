// Copyright 2005-2016 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "mumble_pch.hpp"

#include "Plugins.h"

#include "Global.h"
#include "Log.h"
#include "MainWindow.h"
#include "Message.h"
#include "ServerHandler.h"
#include "../../plugins/mumble_plugin.h"
#include "WebFetch.h"
#include "MumbleApplication.h"

struct PluginFetchMeta {
	QString hash;
	QString path;
	
	PluginFetchMeta(const QString &hash_ = QString(), const QString &path_ = QString())
		: hash(hash_)
		, path(path_) { /* Empty */ }
};

PluginUpdater::PluginUpdater(QObject *p) : QObject(p) {
	QMetaObject::connectSlotsByName(this);

#ifdef QT_NO_DEBUG
#ifndef PLUGIN_PATH
	qsSystemPlugins=QString::fromLatin1("%1/plugins").arg(MumbleApplication::instance()->applicationVersionRootPath());
#ifdef Q_OS_MAC
	qsSystemPlugins=QString::fromLatin1("%1/../Plugins").arg(qApp->applicationDirPath());
#endif
#else
	qsSystemPlugins=QLatin1String(MUMTEXT(PLUGIN_PATH));
#endif

	qsUserPlugins = g.qdBasePath.absolutePath() + QLatin1String("/Plugins");
#else
	qsSystemPlugins = QString::fromLatin1("%1/plugins").arg(MumbleApplication::instance()->applicationVersionRootPath());
	qsUserPlugins = QString();
#endif
}

PluginUpdater::~PluginUpdater() {
	clearPlugins();
}

void PluginUpdater::clearPlugins() {
	qWarning("clearPlugins UNIMPL");
}

void PluginUpdater::checkUpdates() {
	QUrl url;
	url.setPath(QLatin1String("/v1/pa-plugins"));

	QList<QPair<QString, QString> > queryItems;
	queryItems << qMakePair(QString::fromUtf8("ver"), QString::fromUtf8(QUrl::toPercentEncoding(QString::fromUtf8(MUMBLE_RELEASE))));
#if defined(Q_OS_WIN)
# if defined(Q_OS_WIN64)
	queryItems << qMakePair(QString::fromUtf8("os"), QString::fromUtf8("WinX64"));
# else
	queryItems << qMakePair(QString::fromUtf8("os"), QString::fromUtf8("Win32"));
# endif
	queryItems << qMakePair(QString::fromUtf8("abi"), QString::fromUtf8(MUMTEXT(_MSC_VER)));
#elif defined(Q_OS_MAC)
# if defined(USE_MAC_UNIVERSAL)
	queryItems << qMakePair(QString::fromUtf8("os"), QString::fromUtf8("MacOSX-Universal"));
# else
	queryItems << qMakePair(QString::fromUtf8("os"), QString::fromUtf8("MacOSX"));
# endif
#else
	queryItems << qMakePair(QString::fromUtf8("os"), QString::fromUtf8("Unix"));
#endif


#ifdef QT_NO_DEBUG
#if QT_VERSION >= 0x050000
	QUrlQuery query;
	query.setQueryItems(queryItems);
	url.setQuery(query);
#else
	for (int i = 0; i < queryItems.size(); i++) {
		const QPair<QString, QString> &queryPair = queryItems.at(i);
		url.addQueryItem(queryPair.first, queryPair.second);
	}
#endif
	WebFetch::fetch(QLatin1String("update"), url, this, SLOT(fetchedUpdatePAPlugins(QByteArray,QUrl)));
#else
	g.mw->msgBox(tr("Skipping plugin update in debug mode."));
#endif
}

void PluginUpdater::fetchedUpdatePAPlugins(QByteArray data, QUrl) {
	if (data.isNull())
		return;

	bool rescan = false;
	qmPluginFetchMeta.clear();
	QDomDocument doc;
	doc.setContent(data);

	QDomElement root=doc.documentElement();
	QDomNode n = root.firstChild();
	while (!n.isNull()) {
		QDomElement e = n.toElement();
		if (!e.isNull()) {
			if (e.tagName() == QLatin1String("plugin")) {
				QString name = QFileInfo(e.attribute(QLatin1String("name"))).fileName();
				QString hash = e.attribute(QLatin1String("hash"));
				QString path = e.attribute(QLatin1String("path"));
				qmPluginFetchMeta.insert(name, PluginFetchMeta(hash, path));
			}
		}
		n = n.nextSibling();
	}

	QDir qd(qsSystemPlugins, QString(), QDir::Name, QDir::Files | QDir::Readable);
	QDir qdu(qsUserPlugins, QString(), QDir::Name, QDir::Files | QDir::Readable);

	QFileInfoList libs = qd.entryInfoList();
	foreach(const QFileInfo &libinfo, libs) {
		QString libname = libinfo.absoluteFilePath();
		QString filename = libinfo.fileName();
		PluginFetchMeta pfm = qmPluginFetchMeta.value(filename);
		QString wanthash = pfm.hash;
		if (! wanthash.isNull() && QLibrary::isLibrary(libname)) {
			QFile f(libname);
			if (wanthash.isEmpty()) {
				// Outdated plugin
				if (f.exists()) {
					clearPlugins();
					f.remove();
					rescan=true;
				}
			} else if (f.open(QIODevice::ReadOnly)) {
				QString h = QLatin1String(sha1(f.readAll()).toHex());
				f.close();
				if (h == wanthash) {
					if (qd != qdu) {
						QFile qfuser(qsUserPlugins + QString::fromLatin1("/") + filename);
						if (qfuser.exists()) {
							clearPlugins();
							qfuser.remove();
							rescan=true;
						}
					}
					// Mark for removal from userplugins
					qmPluginFetchMeta.insert(filename, PluginFetchMeta());
				}
			}
		}
	}

	if (qd != qdu) {
		libs = qdu.entryInfoList();
		foreach(const QFileInfo &libinfo, libs) {
			QString libname = libinfo.absoluteFilePath();
			QString filename = libinfo.fileName();
			PluginFetchMeta pfm = qmPluginFetchMeta.value(filename);
			QString wanthash = pfm.hash;
			if (! wanthash.isNull() && QLibrary::isLibrary(libname)) {
				QFile f(libname);
				if (wanthash.isEmpty()) {
					// Outdated plugin
					if (f.exists()) {
						clearPlugins();
						f.remove();
						rescan=true;
					}
				} else if (f.open(QIODevice::ReadOnly)) {
					QString h = QLatin1String(sha1(f.readAll()).toHex());
					f.close();
					if (h == wanthash) {
						qmPluginFetchMeta.remove(filename);
					}
				}
			}
		}
	}
	QMap<QString, PluginFetchMeta>::const_iterator i;
	for (i = qmPluginFetchMeta.constBegin(); i != qmPluginFetchMeta.constEnd(); ++i) {
		PluginFetchMeta pfm = i.value();
		if (! pfm.hash.isEmpty()) {
			QUrl pluginDownloadUrl;
			if (pfm.path.isEmpty()) {
				pluginDownloadUrl.setPath(QString::fromLatin1("%1").arg(i.key()));
			} else {
				pluginDownloadUrl.setPath(pfm.path);
			}

			WebFetch::fetch(QLatin1String("pa-plugin-dl"), pluginDownloadUrl, this, SLOT(fetchedPAPluginDL(QByteArray,QUrl)));
		}
	}

	if (rescan)
		rescanPlugins();
}

void PluginUpdater::fetchedPAPluginDL(QByteArray data, QUrl url) {
	if (data.isNull())
		return;

	bool rescan = false;

	const QString &urlPath = url.path();
	QString fname = QFileInfo(urlPath).fileName();
	if (qmPluginFetchMeta.contains(fname)) {
		PluginFetchMeta pfm = qmPluginFetchMeta.value(fname);
		if (pfm.hash == QLatin1String(sha1(data).toHex())) {
			bool verified = true;
#ifdef Q_OS_WIN
			verified = false;
			QString tempname;
			std::wstring tempnative;
			{
				QTemporaryFile temp(QDir::tempPath() + QLatin1String("/plugin_XXXXXX.dll"));
				if (temp.open()) {
					tempname = temp.fileName();
					tempnative = QDir::toNativeSeparators(tempname).toStdWString();
					temp.write(data);
					temp.setAutoRemove(false);
				}
			}
			if (! tempname.isNull()) {
				WINTRUST_FILE_INFO file;
				ZeroMemory(&file, sizeof(file));
				file.cbStruct = sizeof(file);
				file.pcwszFilePath = tempnative.c_str();

				WINTRUST_DATA data;
				ZeroMemory(&data, sizeof(data));
				data.cbStruct = sizeof(data);
				data.dwUIChoice = WTD_UI_NONE;
				data.fdwRevocationChecks = WTD_REVOKE_NONE;
				data.dwUnionChoice = WTD_CHOICE_FILE;
				data.pFile = &file;
				data.dwProvFlags = WTD_SAFER_FLAG | WTD_USE_DEFAULT_OSVER_CHECK;
				data.dwUIContext = WTD_UICONTEXT_INSTALL;

				static GUID guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

				LONG ts = WinVerifyTrust(0, &guid , &data);

				QFile deltemp(tempname);
				deltemp.remove();
				verified = (ts == 0);
			}
#endif
			if (verified) {
				clearPlugins();

				QFile f;
				f.setFileName(qsSystemPlugins + QLatin1String("/") + fname);
				if (f.open(QIODevice::WriteOnly)) {
					f.write(data);
					f.close();
					g.mw->msgBox(tr("Downloaded new or updated plugin to %1.").arg(Qt::escape(f.fileName())));
				} else {
					f.setFileName(qsUserPlugins + QLatin1String("/") + fname);
					if (f.open(QIODevice::WriteOnly)) {
						f.write(data);
						f.close();
						g.mw->msgBox(tr("Downloaded new or updated plugin to %1.").arg(Qt::escape(f.fileName())));
					} else {
						g.mw->msgBox(tr("Failed to install new plugin to %1.").arg(Qt::escape(f.fileName())));
					}
				}

				rescan=true;
			}
		}
	}

	if (rescan)
		rescanPlugins();
}
