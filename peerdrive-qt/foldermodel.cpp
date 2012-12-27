/*
 * This file is part of the PeerDrive Qt4 library.
 * Copyright (C) 2012  Jan Klötzke <jan DOT kloetzke AT freenet DOT de>
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

#include "peerdrive.h"
#include "pdsd.h"

#include <QList>
#include <QVariant>
#include <QSet>
#include <stdexcept>

#include "foldermodel.h"
#include "foldermodel_internal.h"

#include <QtDebug>

using namespace PeerDrive;

/*****************************************************************************/



/*****************************************************************************/

FolderModel::FolderModel(QObject *parent)
	: QAbstractItemModel(parent)
{
	d = new FolderModelPrivate(this);
}

FolderModel::~FolderModel()
{
	delete d;
}

//void FolderModel::setRootItem(const Link &link)
//{
//}

QModelIndex FolderModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent)) {
		//qDebug() << "index" << row << column << parent << "denied";
		return QModelIndex();
	}

	FolderModelPrivate::Node *parentNode = d->node(parent);
	FolderModelPrivate::Node *childItem = parentNode->child(row);
	//qDebug() << "index" << row << column << parent;
	return createIndex(row, column, childItem);
}

//QModelIndex FolderModel::index(const Link &item, int column) const
//{
//}

QModelIndex FolderModel::parent(const QModelIndex &child) const
{
	FolderModelPrivate::Node *node = d->node(child);
	if (node->parent) {
		//qDebug() << "parent" << child << d->index(node->parent);
		return d->index(node->parent);
	}else{
		//qDebug() << "parent" << child << QModelIndex();
		return QModelIndex();
	}
}

bool FolderModel::hasChildren(const QModelIndex &parent) const
{
	if (parent.column() > 0)
		return false;

	FolderModelPrivate::Node *node = d->node(parent);
	//qDebug() << "hasChildren" << parent << !node->visibleChildren.isEmpty();
	return !node->children.isEmpty();//true;//!node->visibleChildren.isEmpty();
}

bool FolderModel::canFetchMore(const QModelIndex &parent) const
{
    if (!d->root || parent.column() > 0) {
		//qDebug() << "canFetchMore bail out";
        return false;
	}

	FolderModelPrivate::Node *node = d->node(parent);
	//qDebug() << "canFetchMore " << parent << !node->fetchingChildren;
	return (!node->fetchingChildren);
}

void FolderModel::fetchMore(const QModelIndex &parent)
{
	//qDebug() << "fetchMore";

    if (!d->root || parent.column() > 0)
        return;

    FolderModelPrivate::Node *node = d->node(parent);
    if (node->fetchingChildren)
        return;

    node->fetchingChildren = true;
	foreach (Link child, node->children.keys())
	    d->worker.fetch(child);
}

int FolderModel::rowCount(const QModelIndex &parent) const
{
	if (parent.column() > 0)
		return 0;

	FolderModelPrivate::Node *node = d->node(parent);
	//qDebug() << "rowCount" << parent << node->visibleChildren.size();
	return node->visibleChildren.size();
}

int FolderModel::columnCount(const QModelIndex &parent) const
{
	return d->worker.columnCount;
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
	//qDebug() << "data" << index << role;
	if (!index.isValid())
		return QVariant();

	if (role != Qt::DisplayRole)
		return QVariant();

	FolderModelPrivate::Node *node = d->node(index);

	if (index.column() >= node->columns.size())
		return QVariant();

	return node->columns.at(index.column());
}

//bool FolderModel::setData(const QModelIndex &index, const QVariant &value, int role)
//{
//}

QVariant FolderModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return d->worker.columnHeader(section);

	return QVariant();
}

Qt::ItemFlags FolderModel::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return 0;

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

//void FolderModel::sort(int column, Qt::SortOrder order)
//{
//}

//QStringList FolderModel::columns() const
//{
//}
//
//void FolderModel::setColumns(const QStringList &cols)
//{
//	QWriteLocker lock(&d->worker.columnsLock);
//
//	// clear all columns
//}

/*****************************************************************************/

FolderModelPrivate::FolderModelPrivate(FolderModel *parent)
	: QObject(parent)
{
	q = parent;

	//qDebug() << qRegisterMetaType<Fol>("Link");
	qDebug() << qRegisterMetaType< QList<FolderInfo> >("QList<FolderInfo>");

	connect(&worker, SIGNAL(updates(QList<FolderInfo>)),
		this, SLOT(gotItemInfos(QList<FolderInfo>)), Qt::QueuedConnection);

	root = new Node(Link(), NULL);
	root->visible = true;
	root->fetchingChildren = true;

	Mounts mounts;
	foreach (Mounts::Store *s, mounts.regularStores()) {
		Link l = Link(s->sid, s->sid);
		Node *newNode = new Node(l, root);
		nodes.insertMulti(l, newNode);
		root->children[l] = newNode;
		worker.fetch(l);
	}
}

FolderModelPrivate::~FolderModelPrivate()
{
	destroyNode(root);
}

FolderModelPrivate::Node *FolderModelPrivate::node(const QModelIndex &index) const
{
	if (!index.isValid())
		return root;
	else
		return static_cast<Node*>(index.internalPointer());
}

QModelIndex FolderModelPrivate::index(const Node *node) const
{
	if (!node->parent || !node->visible)
		return QModelIndex();

	int row = node->parent->visibleChildren.indexOf(node->link);

	return q->createIndex(row, 0, (void*)node);
}

void FolderModelPrivate::updateColumns(Node *node, const QList<QVariant> &infos)
{
	// the root node is not visible (only its childs)
	if (!node->parent)
		return;

	// update node properties
	node->columns = infos;
	if (node->visible) {
		int row = node->parent->visibleChildren.indexOf(node->link);
		QModelIndex topLeft = q->createIndex(row, 0, node);
		QModelIndex bottomRight = q->createIndex(row, infos.size()-1, node);
		emit q->dataChanged(topLeft, bottomRight);
	} else {
		int row = node->parent->visibleChildren.count();
		QModelIndex parent = index(node->parent);
		node->visible = true;
		q->beginInsertRows(parent, row, row);
		node->parent->visibleChildren.append(node->link);
		q->endInsertRows();
	}
}

void FolderModelPrivate::removeChild(Node *node, const Link &link)
{
	Node *oldNode = node->children.value(link);
	if (oldNode->visible) {
		int row = node->visibleChildren.indexOf(link);
		QModelIndex parent = index(node);
		q->beginRemoveRows(parent, row, row);
		node->visibleChildren.removeAll(link);
		q->endRemoveRows();
	}

	node->children.remove(link);
	destroyNode(oldNode);
}

void FolderModelPrivate::destroyNode(Node *node)
{
	foreach (Node *child, node->children.values())
		destroyNode(child);

	nodes.remove(node->link, node);
	delete node;
}

void FolderModelPrivate::gotItemInfos(const QList<FolderInfo> &infos)
{
	foreach (const FolderInfo &info, infos)
		foreach (Node *node, nodes.values(info.link))
			updateNode(node, info);
}

void FolderModelPrivate::updateNode(Node *node, const FolderInfo &info)
{
	// does it still exists on the file system?
	if (info.exists) {

		QSet<Link> curChilds = QSet<Link>::fromList(info.childs);
		QSet<Link> oldChilds = QSet<Link>::fromList(node->children.keys());

		// check for new children
		QSet<Link> newChilds = curChilds - oldChilds;
		for (QSet<Link>::const_iterator i = newChilds.constBegin(); i != newChilds.constEnd(); i++) {
			Node *newNode = new Node(*i, node);
			nodes.insertMulti(*i, newNode);
			node->children[*i] = newNode;
			if (node->fetchingChildren)
				worker.fetch(*i);
		}

		// check for removed children
		QSet<Link> remChilds = oldChilds - curChilds;
		for (QSet<Link>::const_iterator i = remChilds.constBegin();
		     i != remChilds.constEnd(); i++)
			removeChild(node, *i);

		// put the update at the end because the view will check for our childs
		// via hasChildren()
		updateColumns(node, info.columns);
	} else if (node->visible && node->parent) {
		// visible node vanished
		int row = node->parent->visibleChildren.indexOf(node->link);
		QModelIndex parentIdx = index(node->parent);
		q->beginRemoveRows(parentIdx, row, row);
		node->parent->visibleChildren.removeAll(node->link);
		q->endRemoveRows();

		node->visible = false;
	}
}

void FolderModelPrivate::itemChanged(const Link &item)
{
	worker.fetch(item);
}

/*****************************************************************************/

ColumnInfo::~ColumnInfo()
{
}

StatColumnInfo::StatColumnInfo(const QString &key)
{
	this->key = key;

	if (key == ":size") {
		name = "Size";
		extractor = extractSize;
	} else if (key == ":mtime") {
		name = "Modification time";
		extractor = extractMTime;
	} else if (key == ":type") {
		name = "Type code";
		extractor = extractType;
	} else if (key == ":creator") {
		name = "Creator code";
		extractor = extractCreator;
	} else if (key == ":comment") {
		name = "Comment";
		extractor = extractComment;
	} else {
		throw std::runtime_error("Invalid StatColumnInfo key");
	}
}

StatColumnInfo::~StatColumnInfo()
{
}

bool StatColumnInfo::editable() const
{
	return false;
}

QVariant StatColumnInfo::extract(const RevInfo &stat, const Value &/*metaData*/) const
{
	return extractor(stat);
}

QVariant StatColumnInfo::extractSize(const RevInfo &stat)
{
	// TODO: KiB, MiB, GiB
	return QString::number(stat.size());
}

QVariant StatColumnInfo::extractMTime(const RevInfo &stat)
{
	return stat.mtime().toString(Qt::DefaultLocaleShortDate);
}

QVariant StatColumnInfo::extractType(const RevInfo &stat)
{
	return stat.type();
}

QVariant StatColumnInfo::extractCreator(const RevInfo &stat)
{
	return stat.creator();
}

QVariant StatColumnInfo::extractComment(const RevInfo &stat)
{
	return stat.comment();
}


MetaColumnInfo::MetaColumnInfo(const QString &key)
{
	this->key = key;
	this->type = Unsupported;

	try {
		QStringList splittedKey = key.split(":");
		if (splittedKey.size() != 2)
			return;

		Value metaList = Registry::instance().search(splittedKey.at(0), "meta",
			false, Value(Value::LIST));
		QString keyPath = splittedKey.at(1);

		for (int i = 0; i < metaList.size(); i++) {
			const Value &entry = metaList[i];
			const Value &rawPath = entry["key"];

			for (int j = 0; j < rawPath.size(); j++)
				path << rawPath[j].asString();

			if (path.join("/") == keyPath) {
				name = entry["display"].asString();
				QString typeStr = entry["type"].asString();
				if (typeStr == "string")
					type = String;
				else
					type = Unsupported;
				break;
			}

			path.clear();
		}
	} catch (ValueError&) {
	}
}

MetaColumnInfo::~MetaColumnInfo()
{
}

bool MetaColumnInfo::editable() const
{
	return type == String;
}

QVariant MetaColumnInfo::extract(const RevInfo &, const Value &metaData) const
{
	if (type == Unsupported)
		return QVariant();

	try {
		Value item = metaData;
		for (int i = 0; i < path.size(); i++) {
			if (!item.contains(path.at(i)))
				return QVariant();
			item = item[path.at(i)];
		}

		switch (type) {
			case String:
				return item.asString();
			default:
				break;
		}
	} catch (ValueError&) {
	}

	return QVariant();
}

/*****************************************************************************/

FolderGatherer::FolderGatherer(QObject *parent)
	: QThread(parent)
{
	abort = false;
	start(LowPriority);

	columns << new MetaColumnInfo("public.item:org.peerdrive.annotation/title");
	columnCount = 1;
}

FolderGatherer::~FolderGatherer()
{
	QMutexLocker locker(&mutex);
	abort = true;
	condition.wakeOne();
	locker.unlock();
	wait();

	for (int i = 0; i < columns.size(); i++)
		delete columns.at(i);
}

void FolderGatherer::fetch(const Link &item)
{
	QMutexLocker locker(&mutex);

	items.push(item);
	condition.wakeAll();
}

void FolderGatherer::run()
{
	forever {
		QMutexLocker locker(&mutex);

		if (abort)
			return;

		if (items.isEmpty()) {
			dispatch(true);
			condition.wait(&mutex);
			timer.start();
		} else
			dispatch(false);

		if (abort)
			return;

		Link item;
		if (!items.isEmpty())
			item = items.pop();

		locker.unlock();

		if (item.isValid())
			getItemInfos(item);
	}
}

void FolderGatherer::getItemInfos(const Link &item)
{
	FolderInfo info;
	RevInfo stat(item.rev(), QList<DId>() << item.store());
	Document file(item);

	info.link = item;
	info.exists = stat.exists();
	if (info.exists && file.peek()) {
		try {
			QByteArray tmp;
			Value meta;

			// Hmm, extend RevInfo with DocLinks and RevLinks?
			if (Registry::instance().conformes(file.type(), "org.peerdrive.folder")
				&& file.readAll(Part::PDSD, tmp) > 0) {

				Value folder = Value::fromByteArray(tmp, item.store());
				for (int i = 0; i < folder.size(); i++) {
					Link l = folder[i][""].asLink();
					l.update();
					if (!l.isValid())
						continue;

					info.childs.append(l);
				}
			}

			if (file.readAll(Part::META, tmp) > 0)
				meta = Value::fromByteArray(tmp, item.store());

			QReadLocker lock(&columnsLock);
			for (int i = 0; i < columns.size(); i++)
				info.columns.append(columns.at(i)->extract(stat, meta));
		} catch (ValueError&) {
		}

		file.close();
	}

	infos.append(info);
}

void FolderGatherer::dispatch(bool force)
{
	if (!infos.isEmpty() && (force || timer.elapsed() > 100)) {
		emit updates(infos);
		infos.clear();
		timer.start();
	}
}

QString FolderGatherer::columnHeader(int col) const
{
	QReadLocker lock(const_cast<QReadWriteLock*>(&columnsLock));
	return columns[col]->name;
}
