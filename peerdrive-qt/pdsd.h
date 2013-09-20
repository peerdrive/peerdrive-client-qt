/*
 * This file is part of the PeerDrive Qt4 library.
 * Copyright (C) 2013  Jan Klötzke <jan DOT kloetzke AT freenet DOT de>
 *
 * PeerDrive is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * PeerDrive is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PeerDrive. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _PDSD_H_
#define _PDSD_H_

#include <QList>
#include <QString>
#include <QMap>
#include <QByteArray>
#include <QSharedDataPointer>

#include "peerdrive.h"

class QMutex;

namespace PeerDrive {

class Folder {
public:
	Folder();
	Folder(const Link &link);
	~Folder();

	void setLink(const Link &link);
	Link link() const;
	bool load();
	bool save();

	static QList<Link> lookup(const QString &path);
	static Link lookupSingle(const QString &path);

	int size() const;
	bool add(const Link &link);
	bool remove(const Link &link);

private:
	Document file;
	Value content;
};

class FSTab : public QObject
{
	Q_OBJECT

public:
	FSTab(bool autoReload = false, QObject *parent = NULL);
	virtual ~FSTab();

	bool load();
	bool save();

	QList<QString> knownLabels() const;
	bool add(const QString &label, const QString &src,
		const QString &type = QString("file"),
		const QString &options = QString(""),
		const QString &credentials = QString(""));
	bool remove(const QString &label);

	QString src(const QString &label) const;
	bool setSrc(const QString &label, const QString &src);
	QString type(const QString &label) const;
	bool setType(const QString &label, const QString &type);
	QString options(const QString &label) const;
	bool setOptions(const QString &label, const QString &options);
	QString credentials(const QString &label) const;
	bool setCredentials(const QString &label, const QString &credentials);
	bool autoMounted(const QString &label) const;
	bool setAutoMounted(const QString &label, bool enable);

signals:
	void modified();

private slots:
	void fsTabModified(const Link & item);

private:
	bool reload;
	Document *file;
	Value fstab;
	LinkWatcher watch;
};

class Registry : public QObject
{
	Q_OBJECT

public:
	static Registry& instance();

	Value search(const QString &uti, const QString &key, bool recursive = true,
		const Value &defVal = Value()) const;
	bool conformes(const QString &uti, const QString &superClass) const;
	QStringList conformes(const QString &uti) const;
	QString title(const QString &uti) const;
	QString icon(const QString &uti) const;
	QStringList executables(const QString &uti) const;

private slots:
	void modified(const Link &item);

private:
	Registry();
	Registry(const Registry &) : QObject() { }
	~Registry() { }

	static QMutex mutex;
	static Registry* volatile singleton;

	Link link;
	Value registry;
	LinkWatcher watch;
};

}

#endif
