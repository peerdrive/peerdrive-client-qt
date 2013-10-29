
#include <QMimeData>
#include <QStringList>
#include "dndfoldermodel.h"

DndFolderModel::DndFolderModel(QObject *parent)
	: FolderModel(parent)
{
}

Qt::DropActions DndFolderModel::supportedDropActions() const
{
	return Qt::CopyAction;
}

Qt::ItemFlags DndFolderModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags flags = PeerDrive::FolderModel::flags(index);

	if (data(index, PeerDrive::FolderModel::IsFolderRole).toBool())
		flags |= Qt::ItemIsDropEnabled;

	return flags;
}

bool DndFolderModel::dropMimeData(const QMimeData *mime, Qt::DropAction action,
	int /*row*/, int /*column*/, const QModelIndex &parent)
{
	if (action == Qt::IgnoreAction)
		return true;
	if (!parent.isValid())
		return false;

	PeerDrive::Link dst(data(parent, PeerDrive::FolderModel::LinkRole)
		.value<PeerDrive::Link>());

	if (mime->hasFormat("application/x-peerdrive-links")) {
		QStringList list(QString(mime->data("application/x-peerdrive-links")).split('\n'));

		QList<PeerDrive::Link> sources;
		sources.reserve(list.size());
		foreach (const QString &url, list)
			sources.append(PeerDrive::Link(url));

		emit requestReplicate(sources, dst);
		return true;
	}

	return false;
}

