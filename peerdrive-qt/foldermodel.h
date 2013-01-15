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
#ifndef PEERDRIVE_FOLDERMODEL_H
#define PEERDRIVE_FOLDERMODEL_H

#include <QAbstractItemModel>

namespace PeerDrive {

class FolderModelPrivate;
class Link;

class FolderModel : public QAbstractItemModel
{
    Q_OBJECT

public:
	enum ItemDataRole {
		LinkRole = Qt::UserRole,
	};

	FolderModel(QObject *parent = 0);
	~FolderModel();

	void setRootItem(const Link &link);
	Link rootItem() const;

	//QStringList columns() const;
	//void setColumns(const QStringList &cols);

	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
	QModelIndex index(const Link &item, int column = 0) const;
	QModelIndex parent(const QModelIndex &child) const;
	bool hasChildren(const QModelIndex &parent = QModelIndex()) const;
	bool canFetchMore(const QModelIndex &parent) const;
	void fetchMore(const QModelIndex &parent);

	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
	//bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	Qt::ItemFlags flags(const QModelIndex &index) const;

	//void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);

signals:
	void fetched(const QModelIndex &parent);

private:
	FolderModelPrivate *d;

	friend class FolderModelPrivate;
};

}

#endif
