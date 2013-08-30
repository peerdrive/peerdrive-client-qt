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

#include <peerdrive-qt/peerdrive.h>
#include <peerdrive-qt/pdsd.h>

#include "syncrules.h"

#include <QtDebug>

SyncRules::SyncRules()
{
	connect(&watch, SIGNAL(modified(Link)), this, SLOT(_modified(Link)));
	changed = false;
}

SyncRules::~SyncRules()
{
}

bool SyncRules::load()
{
	rules = PeerDrive::Value(PeerDrive::Value::LIST);

	if (!link.isValid()) {
		link = PeerDrive::Folder::lookupSingle("sys:syncrules");
		if (!link.isValid())
			return false;

		watch.addWatch(link);
	}

	PeerDrive::Document file(link);
	if (!file.peek())
		return false;

	try {
		rules = file.get("/org.peerdrive.syncrules");
		changed = false;
	} catch (PeerDrive::ValueError&) {
		return false;
	}

	return true;
}

bool SyncRules::save() const
{
	if (!changed)
		return true;

	PeerDrive::Document file(link);
	if (!file.update())
		return false;

	if (!file.set("/org.peerdrive.syncrules", rules))
		return false;

	if (!file.commit())
		return false;

	file.close();
	const_cast<PeerDrive::Link&>(link) = file.link();
	const_cast<bool&>(changed) = false;

	return true;
}

int SyncRules::size() const
{
	return rules.size();
}

int SyncRules::index(const PeerDrive::DId &from, const PeerDrive::DId &to) const
{
	QString fromStr = from.toByteArray().toHex();
	QString toStr = to.toByteArray().toHex();

	try {
		for (int i = 0; i < rules.size(); i++) {
			if (rules[i]["from"].asString() != fromStr)
				continue;
			if (rules[i]["to"].asString() != toStr)
				continue;
			return i;
		}
	} catch (PeerDrive::ValueError&) {
	}

	return -1;
}

PeerDrive::DId SyncRules::from(int i) const
{
	try {
		return PeerDrive::DId(QByteArray::fromHex(rules[i]["from"].asString().toLatin1()));
	} catch (PeerDrive::ValueError&) {
		return PeerDrive::DId();
	}
}

PeerDrive::DId SyncRules::to(int i) const
{
	try {
		return PeerDrive::DId(QByteArray::fromHex(rules[i]["to"].asString().toLatin1()));
	} catch (PeerDrive::ValueError&) {
		return PeerDrive::DId();
	}
}

SyncRules::Mode SyncRules::mode(int i) const
{
	try {
		QString mode = rules[i]["mode"].asString();
		if (mode == "ff")
			return FastForward;
		else if (mode == "latest")
			return Latest;
		else if (mode == "merge")
			return Merge;
		else
			return None;
	} catch (PeerDrive::ValueError&) {
		return None;
	}
}

SyncRules::Mode SyncRules::mode(const PeerDrive::DId &from, const PeerDrive::DId &to) const
{
	int i = index(from, to);
	return i >= 0 ? mode(i) : None;
}

void SyncRules::setMode(const PeerDrive::DId &from, const PeerDrive::DId &to, Mode mode)
{
	int i = index(from, to);

	if (mode == None) {
		if (i >= 0) {
			rules.remove(i);
			changed = true;
		}
	} else {
		if (i < 0) {
			PeerDrive::Value entry;
			entry["from"] = QString(from.toByteArray().toHex());
			entry["to"] = QString(to.toByteArray().toHex());

			i = rules.size();
			rules.append(entry);
		}

		switch (mode) {
			case FastForward:
				rules[i]["mode"] = QString("ff");
				break;
			case Latest:
				rules[i]["mode"] = QString("latest");
				break;
			case Merge:
				rules[i]["mode"] = QString("merge");
				break;
			default:
				// not reached
				break;
		}

		changed = true;
	}
}

QString SyncRules::description(int i) const
{
	try {
		return rules[i].get("descr", PeerDrive::Value(QString())).asString();
	} catch (PeerDrive::ValueError&) {
		return QString();
	}
}

QString SyncRules::description(const PeerDrive::DId &from, const PeerDrive::DId &to) const
{
	int i = index(from, to);
	return i >= 0 ? description(i) : QString();
}

void SyncRules::setDescription(const PeerDrive::DId &from, const PeerDrive::DId &to, QString descr)
{
	int i = index(from, to);
	if (i >= 0) {
		rules[i]["descr"] = descr;
		changed = true;
	}
}

void SyncRules::_modified(const PeerDrive::Link &item)
{
	link = item;
	load();
	emit modified();
}

