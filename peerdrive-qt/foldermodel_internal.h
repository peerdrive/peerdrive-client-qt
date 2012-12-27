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
#ifndef PEERDRIVE_FOLDERMODEL_INTERNAL_H
#define PEERDRIVE_FOLDERMODEL_INTERNAL_H

#include "peerdrive.h"
#include "pdsd.h"

#include <QHash>
#include <QList>
#include <QMultiHash>
#include <QMutex>
#include <QReadWriteLock>
#include <QStack>
#include <QStringList>
#include <QThread>
#include <QTime>
#include <QVariant>
#include <QWaitCondition>

class QModelIndex;

namespace PeerDrive {

class FolderModel;

class ColumnInfo {
public:
	virtual ~ColumnInfo();
	virtual bool editable() const = 0;
	virtual QVariant extract(const RevInfo &stat, const Value &metaData) const = 0;

	QString name;
	QString key;
};

class StatColumnInfo : public ColumnInfo {
public:
	StatColumnInfo(const QString &key);
	~StatColumnInfo();
	bool editable() const;
	QVariant extract(const RevInfo &stat, const Value &metaData) const;
private:
	QVariant (*extractor)(const RevInfo &stat);
	static QVariant extractSize(const RevInfo &stat);
	static QVariant extractMTime(const RevInfo &stat);
	static QVariant extractType(const RevInfo &stat);
	static QVariant extractCreator(const RevInfo &stat);
	static QVariant extractComment(const RevInfo &stat);
};

class MetaColumnInfo : public ColumnInfo {
public:
	MetaColumnInfo(const QString &key);
	~MetaColumnInfo();
	bool editable() const;
	QVariant extract(const RevInfo &stat, const Value &metaData) const;
private:
	QStringList path;
	enum Type {
		String,
		Unsupported
	};
	Type type;
};

struct FolderInfo
{
	Link link;
	bool exists;
	QList<QVariant> columns;
	QList<Link> childs;
};

class FolderGatherer : public QThread
{
	Q_OBJECT

public:
	FolderGatherer(QObject *parent = 0);
	~FolderGatherer();

	void fetch(const Link &item);

	int columnCount;
	QString columnHeader(int col) const;

signals:
	void updates(const QList<FolderInfo> &infos);

protected:
    void run();

private:
	void getItemInfos(const Link &link);
	void dispatch(bool force);

    QMutex mutex;
    QWaitCondition condition;
    volatile bool abort;
    QStack<Link> items;
	QList<FolderInfo> infos;
	QTime timer;

	QReadWriteLock columnsLock;
	QList<ColumnInfo*> columns;
};

class FolderModelPrivate : public QObject
{
	Q_OBJECT

public:
	class Node {
	public:
		Node(const Link &link, Node *parent)
			: link(link), parent(parent), visible(false), fetchingChildren(false)
		{
		}

		Node* child(int row)
		{
			return children.value(visibleChildren.at(row));
		}

		Link link;
		Node *parent;
		QList<Link> visibleChildren;
		QHash<Link, Node*> children;
		bool visible;
		bool fetchingChildren;
		QList<QVariant> columns;
	};

	FolderModelPrivate(FolderModel *parent);
	~FolderModelPrivate();

	Node *node(const QModelIndex &index) const;
	QModelIndex index(const Node *node) const;

	FolderModel *q;
	QMultiHash<Link, Node*> nodes;
	Node *root;
	FolderGatherer worker;

private:
	void updateNode(Node *node, const FolderInfo &info);
	void updateColumns(Node *node, const QList<QVariant> &infos);
	void removeChild(Node *node, const Link &link);
	void destroyNode(Node *node);

private slots:
	void gotItemInfos(const QList<FolderInfo> &infos);
	void itemChanged(const Link &item);
};

}

#endif
