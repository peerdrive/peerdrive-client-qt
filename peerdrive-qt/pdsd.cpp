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

#include <QtEndian>
#include <QStringList>
#include <stdexcept>

#include "pdsd.h"
#include "peerdrive.h"
#include "peerdrive_internal.h"

namespace PeerDrive {

QList<Link> Folder::lookup(const QString &path)
{
	WalkPathReq req;
	WalkPathCnf cnf;

	req.set_path(path.toUtf8().constData());
	int err = Connection::defaultRPC<WalkPathReq, WalkPathCnf>(WALK_PATH_MSG,
		req, cnf);
	if (err)
		return QList<Link>();

	QList<Link> result;
	for (int i = 0; i < cnf.items_size(); i++) {
		const WalkPathCnf_Item &item = cnf.items(i);

		DId store(item.store());
		if (item.has_doc())
			result.append(Link(store, DId(item.doc())));
		else if (item.has_rev())
			result.append(Link(store, RId(item.rev())));
	}

	return result;
}

Link Folder::lookupSingle(const QString &path)
{
	QList<Link> res = lookup(path);

	if (res.size() != 1)
		return Link();
	else
		return res.at(0);
}

/****************************************************************************/

FSTab::FSTab(bool autoReload, QObject *parent) : QObject(parent)
{
	reload = autoReload;
	file = NULL;
	QObject::connect(&watch, SIGNAL(modified(Link)), this, SLOT(fsTabModified(Link)));
}

FSTab::~FSTab()
{
	if (file)
		delete file;
}

void FSTab::fsTabModified(const Link & item)
{
	if (item.store() != file->link().store())
		return;

	if (reload)
		load();

	emit modified();
}

bool FSTab::load()
{
	if (!file) {
		// try to lookup fstab
		Link link = Folder::lookupSingle("sys:fstab");
		if (!link.isValid())
			return false;

		watch.addWatch(link);
		file = new Document(link);
	} else {
		// reset to HEAD
		Link link = file->link();
		if (!link.update())
			return false;
		file->setLink(link);
	}

	if (!file->peek())
		return false;

	try {
		fstab = file->get("/org.peerdrive.fstab");
	} catch (ValueError&) {
		file->close();
		return false;
	}

	file->close();
	return true;
}

bool FSTab::save()
{
	if (!file || !file->update())
		return false;

	if (!file->set("/org.peerdrive.fstab", fstab)) {
		file->close();
		return false;
	}

	if (!file->commit()) {
		file->close();
		return false;
	}

	file->close();
	return true;
}

QList<QString> FSTab::knownLabels() const
{
	return fstab.keys();
}

bool FSTab::add(const QString &label, const QString &src, const QString &type,
                const QString &options, const QString &credentials)
{
	if (fstab.contains(label))
		return false;

	fstab[label]["src"] = src;
	if (type != "file")
		fstab[label]["type"] = type;
	if (!options.isEmpty())
		fstab[label]["options"] = options;
	if (!credentials.isEmpty())
		fstab[label]["credentials"] = options;

	return true;
}

bool FSTab::remove(const QString &label)
{
	if (fstab.contains(label))
		return false;

	fstab.remove(label);
	return true;
}

QString FSTab::src(const QString &label) const
{
	if (!fstab.contains(label))
		return QString();

	return fstab.get(label).get("src").asString();
}

QString FSTab::type(const QString &label) const
{
	if (!fstab.contains(label))
		return QString();

	return fstab.get(label).get("type", QString("file")).asString();
}

QString FSTab::options(const QString &label) const
{
	if (!fstab.contains(label))
		return QString();

	return fstab.get(label).get("options", QString("")).asString();
}

QString FSTab::credentials(const QString &label) const
{
	if (!fstab.contains(label))
		return QString();

	return fstab.get(label).get("credentials", QString("")).asString();
}

bool FSTab::autoMounted(const QString &label) const
{
	if (!fstab.contains(label))
		return false;

	return fstab.get(label).get("auto", false).asBool();
}

bool FSTab::setAutoMounted(const QString &label, bool enable)
{
	if (!fstab.contains(label))
		return false;

	fstab[label]["auto"] = enable;
	return true;
}

/****************************************************************************/

QMutex Registry::mutex;
Registry* volatile Registry::singleton = NULL;

Registry& Registry::instance()
{
	if (singleton)
		return *singleton;

	QMutexLocker locker(&mutex);

	if (!singleton)
		singleton = new Registry();

	return *singleton;
}

Registry::Registry()
	: QObject()
{
	QObject::connect(&watch, SIGNAL(modified(Link)), this, SLOT(modified(Link)));

	link = Folder::lookupSingle("sys:registry");
	if (!link.isValid())
		throw std::runtime_error("Registry not found");

	watch.addWatch(link);

	modified(link);
}

void Registry::modified(const Link &item)
{
	if (item.store() != link.store())
		return;

	Document file(item);
	if (!file.peek()) {
		qDebug() << "PeerDrive::Registry: cannot read registry!";
		return;
	}

	try {
		registry = file.get("/org.peerdrive.registry");
	} catch (ValueError&) {
			qDebug() << "PeerDrive::Registry: invalid data!";
	}

	file.close();
}

Value Registry::search(const QString &uti, const QString &key, bool recursive,
                       const Value &defVal) const
{
	if (!registry.contains(uti))
		return defVal;

	const Value &item = registry[uti];
	if (item.contains(key))
		return item[key];
	else if (!recursive)
		return defVal;
	else if (!item.contains("conforming"))
		return defVal;

	// try to search recursive
	const Value &conforming = item["conforming"];
	for (int i = 0; i < conforming.size(); i++) {
		Value result = search(conforming[i].asString(), key);
		if (result.type() != Value::NUL)
			return result;
	}

	// nothing found
	return defVal;
}

bool Registry::conformes(const QString &uti, const QString &superClass) const
{
	if (uti == superClass)
		return true;

	if (!registry.contains(uti))
		return false;

	Value conforming = registry[uti].get("conforming", Value(Value::LIST));
	for (int i = 0; i < conforming.size(); i++)
		if (conformes(conforming[i].asString(), superClass))
			return true;

	return false;
}

QStringList Registry::conformes(const QString &uti) const
{
	if (!registry.contains(uti))
		return QStringList();

	QStringList result;
	Value conforming = registry[uti].get("conforming", Value(Value::LIST));
	for (int i = 0; i < conforming.size(); i++)
		result.append(conforming[i].asString());

	return result;
}

QStringList Registry::executables(const QString &uti) const
{
	if (!registry.contains(uti))
		return QStringList();

	// list of executables for uti
	Value exec = registry[uti].get("exec", Value(Value::LIST));
	QStringList result;
	for (int i = 0; i < exec.size(); i++)
		result << exec[i].asString();

	// extend with all superclasses
	Value conforming = registry[uti].get("conforming", Value(Value::LIST));
	for (int i = 0; i < conforming.size(); i++)
		result.append(executables(conforming[i].asString()));

	result.removeDuplicates();
	return result;
}

QString Registry::icon(const QString &uti) const
{
	return search(uti, "icon", true, Value(QString("uti/unknown.png"))).asString();
}

QString Registry::title(const QString &uti) const
{
	return search(uti, "display", true, Value(QString("unknown"))).asString();
}

}
