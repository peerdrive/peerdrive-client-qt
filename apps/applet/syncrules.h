/*
 * PeerDrive
 * Copyright (C) 2013  Jan Klötzke <jan DOT kloetzke AT freenet DOT de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SYNCRULES_H
#define SYNCRULES_H

#include <peerdrive-qt/peerdrive.h>
#include <peerdrive-qt/pdsd.h>
#include <QObject>

using PeerDrive::Link;

class SyncRules : public QObject
{
	Q_OBJECT

public:
	enum Mode {
		None,
		FastForward,
		Latest,
		Merge
	};

	struct Rule {
		PeerDrive::DId from;
		PeerDrive::DId to;
		Mode mode;
	};

	SyncRules();
	~SyncRules();

	bool load();
	bool save() const;

	int size() const;
	int index(const PeerDrive::DId &from, const PeerDrive::DId &to) const;

	PeerDrive::DId from(int i) const;
	PeerDrive::DId to(int i) const;

	Mode mode(int i) const;
	Mode mode(const PeerDrive::DId &from, const PeerDrive::DId &to) const;
	void setMode(const PeerDrive::DId &from, const PeerDrive::DId &to, Mode mode);

	QString description(int i) const;
	QString description(const PeerDrive::DId &from, const PeerDrive::DId &to) const;
	void setDescription(const PeerDrive::DId &from, const PeerDrive::DId &to, QString descr);

signals:
	void modified();

private slots:
	void _modified(const Link &item);

private:
	bool changed;
	PeerDrive::Link link;
	PeerDrive::Value rules;
	PeerDrive::LinkWatcher watch;
};

#endif
