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

#include "peerdrive.h"
#include "pdsd.h"

#include <QIcon>
#include <QList>
#include <QSet>
#include <QVariant>
#include <QLocale>
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

void FolderModel::setRootItem(const Link &link)
{
	beginResetModel();
	d->setRootItem(link);
	endResetModel();
}

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
	return node->unknownChildren > 0;
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

int FolderModel::columnCount(const QModelIndex & /*parent*/) const
{
	return d->worker.columnCount;
}

static QVariant formatSize(qulonglong size)
{
	const char *units[] = { "Bytes", "KiB", "MiB", "GiB" };

	int unit = 0;
	unsigned int frac = 0;
	while (unit < 3 && size > (1 << 10)) {
		unit++;
		frac = size & 1023;
		size >>= 10;
	}

	if (frac)
		return QString("%1%2%3 %4").arg(size).arg(QLocale::system().decimalPoint())
			.arg(frac / 102).arg(units[unit]);
	else
		return QString("%1 %2").arg(size).arg(units[unit]);
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
	//qDebug() << "data" << index << role;
	if (!index.isValid())
		return QVariant();

	FolderModelPrivate::Node *node = d->node(index);

	switch (role) {
		case Qt::DisplayRole:
			if (index.column() >= node->columns.size())
				return QVariant();

			if (d->worker.columnSizeMask & (1UL << index.column()))
				return formatSize(node->columns.at(index.column()).toULongLong());
			else
				return node->columns.at(index.column());
		case Qt::DecorationRole:
			if (index.column() == 0)
				return QIcon(ICON_PATH "/" + Registry::instance().icon(node->type));
			else
				return QVariant();
		case LinkRole:
			return QVariant::fromValue(node->link);
		case TypeCodeRole:
			return node->type;
		default:
			return QVariant();
	}
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

void FolderModel::sort(int column, Qt::SortOrder order)
{
	if (column < 0 || column >= d->worker.columnCount)
		return;

	if (d->sortOrder == order && d->sortColumn == column)
		return;

	emit layoutAboutToBeChanged();

	// get nodes of active persistent indexes
	QModelIndexList oldList = persistentIndexList();
	QList<FolderModelPrivate::Node*> oldNodes;
	for (int i = 0; i < oldList.count(); ++i)
		oldNodes.append(d->node(oldList.at(i)));

	// do actual sort
	d->sortColumn = column;
	d->sortOrder = order;
	d->sort();

	// update persistent index list
	QModelIndexList newList;
	for (int i = 0; i < oldNodes.count(); ++i)
		newList.append(d->index(oldNodes.at(i), oldList.at(i).column()));
	changePersistentIndexList(oldList, newList);

	emit layoutChanged();
}

QStringList FolderModel::columns() const
{
	return d->worker.columnDefs();
}

void FolderModel::setColumns(const QStringList &cols)
{
	if (cols.size() == 0)
		return;

	beginResetModel();

	// update worker to fetch the right data
	d->worker.setColumns(cols);

	// clear all columns an re-fetch it
	QList<QVariant> empty;
	for (int i = 0; i < cols.size(); i++)
		empty << QVariant();

	d->setNodeColumns(d->root, empty);

	endResetModel();
}

void FolderModel::insertColumn(const QString &key, int i)
{
	if (i < 0 || i > d->worker.columnCount)
		return;

	d->worker.insertColumn(i, key);
	d->insertNodeColumn(d->root, i);
}

void FolderModel::removeColumn(int i)
{
	if (i < 0 || i >= d->worker.columnCount)
		return;

	d->worker.removeColumn(i);
	d->removeNodeColumn(d->root, i);
}

/*****************************************************************************/

FolderModelPrivate::FolderModelPrivate(FolderModel *parent)
	: QObject(parent)
{
	q = parent;
	sortColumn = 0;
	sortOrder = Qt::AscendingOrder;

	qRegisterMetaType< QList<FolderInfo> >("QList<FolderInfo>");

	connect(&worker, SIGNAL(updates(QList<FolderInfo>)),
		this, SLOT(gotItemInfos(QList<FolderInfo>)), Qt::QueuedConnection);
	connect(&watch, SIGNAL(modified(Link)), this, SLOT(itemChanged(Link)));
	connect(&watch, SIGNAL(distributionChanged(Link, Distribution)),
		this, SLOT(itemChanged(Link)));

	root = createNode(LinkWatcher::rootDoc, NULL);
	root->visible = true;
	root->fetchingChildren = true;
	worker.fetch(LinkWatcher::rootDoc);
}

FolderModelPrivate::~FolderModelPrivate()
{
	destroyNode(root);
}

void FolderModelPrivate::setRootItem(const Link &link)
{
	destroyNode(root);
	worker.flush();

	root = createNode(link, NULL);
	root->visible = true;
	root->fetchingChildren = true;
	worker.fetch(link);
}

class SortHelper
{
public:
	SortHelper(const QHash<Link, FolderModelPrivate::Node*> &childs, int col,
	           Qt::SortOrder ord)
		: children(childs), column(col), order(ord)
	{
	}

	bool operator()(const Link &l, const Link &r)
	{
		FolderModelPrivate::Node *left = children.value(l);
		FolderModelPrivate::Node *right = children.value(r);

		// folders first (except on MAC)
#ifndef Q_OS_MAC
		if (left->isFolder ^ right->isFolder)
			return left->isFolder;
#endif

		// FIXME: handle individual types for correct results
		bool result = left->columns.at(column).toString()
			< right->columns.at(column).toString();
		return order == Qt::AscendingOrder ? result : !result;
	}

private:
	const QHash<Link, FolderModelPrivate::Node*> &children;
	int column;
	Qt::SortOrder order;
};

void FolderModelPrivate::sort()
{
	sortNode(root);
}

void FolderModelPrivate::sortNode(Node *node)
{
	SortHelper helper(node->children, sortColumn, sortOrder);
	qStableSort(node->visibleChildren.begin(), node->visibleChildren.end(), helper);

	foreach(const Link &l, node->visibleChildren)
		sortNode(node->children.value(l));
}

FolderModelPrivate::Node *FolderModelPrivate::node(const QModelIndex &index) const
{
	if (!index.isValid())
		return root;
	else
		return static_cast<Node*>(index.internalPointer());
}

QModelIndex FolderModelPrivate::index(const Node *node, int col) const
{
	if (!node->parent || !node->visible)
		return QModelIndex();

	int row = node->parent->visibleChildren.indexOf(node->link);

	return q->createIndex(row, col, (void*)node);
}

void FolderModelPrivate::updateColumns(Node *node, const QList<QVariant> &infos)
{
	// FIXME: check if infos has the right number of columns
	// or better if columns have been changed in between (counter?)

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
		// TODO: should we trigger a sort?
	} else {
		QList<Link> tmp = node->parent->visibleChildren;
		tmp.append(node->link);
		SortHelper helper(node->parent->children, sortColumn, sortOrder);
		qStableSort(tmp.begin(), tmp.end(), helper);

		int row = tmp.indexOf(node->link);

		QModelIndex parent = index(node->parent);
		node->visible = true;
		q->beginInsertRows(parent, row, row);
		node->parent->visibleChildren = tmp;
		q->endInsertRows();
	}
}

void FolderModelPrivate::removeChild(Node *node, const Link &link)
{
	Node *oldNode = node->children.value(link);
	QModelIndex parent = index(node);

	if (!oldNode->fetched) {
		node->unknownChildren--;
		if (node->unknownChildren == 0)
			emit q->fetched(parent);
	}

	if (oldNode->visible) {
		int row = node->visibleChildren.indexOf(link);
		q->beginRemoveRows(parent, row, row);
		node->visibleChildren.removeAll(link);
		q->endRemoveRows();
	}

	node->children.remove(link);
	destroyNode(oldNode);
}

FolderModelPrivate::Node *FolderModelPrivate::createNode(const Link &link, Node *parent)
{
	Node *newNode = new Node(link, parent);

	if (!nodes.contains(link))
		watch.addWatch(link);

	nodes.insertMulti(link, newNode);
	return newNode;
}

void FolderModelPrivate::destroyNode(Node *node)
{
	foreach (Node *child, node->children.values())
		destroyNode(child);

	nodes.remove(node->link, node);
	if (!nodes.contains(node->link))
		watch.removeWatch(node->link);

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
	if (!node->fetched) {
		node->fetched = true;
		if (node->parent) {
			// regular node has been fetched
			node->parent->unknownChildren--;
			if (node->parent->unknownChildren == 0) {
				QModelIndex parentIdx = index(node->parent);
				emit q->fetched(parentIdx);
			}
		} else if (!info.exists || info.childs.isEmpty()) {
			// empty or non-existing root node
			QModelIndex idx = index(node);
			emit q->fetched(idx);
		}
	}

	// does it still exists on the file system?
	if (info.exists) {

		QSet<Link> curChilds = QSet<Link>::fromList(info.childs);
		QSet<Link> oldChilds = QSet<Link>::fromList(node->children.keys());

		// check for new children
		QSet<Link> newChilds = curChilds - oldChilds;
		for (QSet<Link>::const_iterator i = newChilds.constBegin(); i != newChilds.constEnd(); i++) {
			Node *newNode = createNode(*i, node);
			node->children[*i] = newNode;
			node->unknownChildren++;
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
		node->type = info.type;
		node->isFolder = info.isFolder;
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

void FolderModelPrivate::setNodeColumns(Node *node, const QList<QVariant> &empty)
{
	node->columns = empty;
	worker.fetch(node->link);

	foreach (Node *child, node->children.values())
		setNodeColumns(child, empty);
}

void FolderModelPrivate::insertNodeColumn(Node *node, int i)
{
	if (!node->visible)
		return;

	node->columns.insert(i, QVariant());
	worker.fetch(node->link);

	q->beginInsertColumns(index(node), i, i);
	foreach (Node *child, node->children.values())
		insertNodeColumn(child, i);
	q->endInsertColumns();
}

void FolderModelPrivate::removeNodeColumn(Node *node, int i)
{
	if (!node->visible)
		return;

	node->columns.removeAt(i);
	worker.fetch(node->link);

	q->beginRemoveColumns(index(node), i, i);
	foreach (Node *child, node->children.values())
		removeNodeColumn(child, i);
	q->endRemoveColumns();
}

/*****************************************************************************/

ColumnInfo::~ColumnInfo()
{
}

bool ColumnInfo::isSize() const
{
	return false;
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

bool StatColumnInfo::isSize() const
{
	return extractor == extractSize;
}

QVariant StatColumnInfo::extract(const RevInfo &stat, const Value &/*metaData*/) const
{
	return extractor(stat);
}

QVariant StatColumnInfo::extractSize(const RevInfo &stat)
{
	return stat.size();
}

QVariant StatColumnInfo::extractMTime(const RevInfo &stat)
{
	return stat.mtime();
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

			// FIXME: something goes wrong here if we assign 'item' directly
			// reliably detectable with valgrind
			Value item2 = item[path.at(i)];
			item = item2;
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

	columns << new MetaColumnInfo("public.item:title");
	columnCount = 1;
	columnSizeMask = 0;
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

void FolderGatherer::flush()
{
	QMutexLocker locker(&mutex);

	items.clear();
	infos.clear();
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

		if (item.isValid()) {
			if (item == LinkWatcher::rootDoc)
				getRootInfos();
			else
				getItemInfos(item);
		}
	}
}

void FolderGatherer::getItemInfos(const Link &item)
{
	DId store(item.store());
	FolderInfo info;
	RevInfo stat(item.rev(), QList<DId>() << store);
	Document file(item);

	info.link = item;
	info.exists = stat.exists();
	if (info.exists && file.peek()) {
		info.type = stat.type();
		try {
			QByteArray tmp;
			Value meta;

			info.isFolder = Registry::instance().conformes(file.type(),
				"org.peerdrive.folder");

			LinkInfo linkInfo(item.rev(), QList<DId>() << store);
			foreach(const DId &doc, linkInfo.docLinks())
				info.childs.append(Link(store, doc));
			foreach(const RId &rev, linkInfo.revLinks())
				info.childs.append(Link(store, rev));

			meta = file.get("/org.peerdrive.annotation");

			QReadLocker lock(&columnsLock);
			for (int i = 0; i < columns.size(); i++)
				info.columns.append(columns.at(i)->extract(stat, meta));
		} catch (ValueError&) {
		}

		file.close();
	}

	infos.append(info);
	//QThread::msleep(150);
}

void FolderGatherer::getRootInfos()
{
	FolderInfo info;

	info.link = LinkWatcher::rootDoc;
	info.exists = true;
	info.type = "org.peerdrive.store";
	info.isFolder = true;

	Mounts mounts;
	foreach (Mounts::Store *s, mounts.regularStores()) {
		Link l = Link(s->sid, s->sid);
		info.childs.append(l);
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

QStringList FolderGatherer::columnDefs() const
{
	QReadLocker lock(const_cast<QReadWriteLock*>(&columnsLock));

	QStringList result;
	for (int i = 0; i < columns.size(); i++)
		result << columns.at(i)->key;

	return result;
}

void FolderGatherer::setColumns(const QStringList &cols)
{
	QWriteLocker lock(&columnsLock);

	for (int i = 0; i < columns.size(); i++)
		delete columns.at(i);
	columns.clear();

	columnCount = 0;
	columnSizeMask = 0;

	for (int i = 0; i < cols.size(); i++) {
		QString key = cols.at(i);
		QStringList splittedKey = key.split(":");
		if (splittedKey.size() != 2)
			continue;

		ColumnInfo *col = splittedKey.at(0) == ""
			? static_cast<ColumnInfo*>(new StatColumnInfo(key))
			: static_cast<ColumnInfo*>(new MetaColumnInfo(key));
		columns.append(col);

		columnCount++;
		if (col->isSize())
			columnSizeMask |= 1 << i;
	}
}

void FolderGatherer::insertColumn(int i, const QString &key)
{
	QWriteLocker lock(&columnsLock);

	QStringList splittedKey = key.split(":");
	ColumnInfo *col = splittedKey.at(0) == ""
		? static_cast<ColumnInfo*>(new StatColumnInfo(key))
		: static_cast<ColumnInfo*>(new MetaColumnInfo(key));
	columns.insert(i, col);

	columnCount++;
	if (col->isSize())
		columnSizeMask |= 1 << i;
}

void FolderGatherer::removeColumn(int i)
{
	QWriteLocker lock(&columnsLock);

	delete columns.takeAt(i);
	columnCount--;

	columnSizeMask = 0;
	for (int j = 0; j < columnCount; j++)
		if (columns.at(j)->isSize())
			columnSizeMask |= 1 << j;
}
