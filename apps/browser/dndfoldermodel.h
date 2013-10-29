#ifndef DNDFOLDERMODEL_H
#define DNDFOLDERMODEL_H

#include <QList>
#include <peerdrive-qt/foldermodel.h>
#include <peerdrive-qt/peerdrive.h>

class DndFolderModel : public PeerDrive::FolderModel
{
    Q_OBJECT

public:
	DndFolderModel(QObject *parent = 0);

	// DnD stuff
	Qt::DropActions supportedDropActions() const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool dropMimeData(const QMimeData *mime, Qt::DropAction action, int row,
		int column, const QModelIndex &parent);

signals:
	void requestReplicate(const QList<PeerDrive::Link> &sources,
		const PeerDrive::Link &destination);
};

Q_DECLARE_METATYPE(QList<PeerDrive::Link>);

#endif
