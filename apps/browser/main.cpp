#include <QtGui>

#include "peerdrive-qt/foldermodel.h"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	PeerDrive::FolderModel model;

	QTreeView view;
	view.setModel(&model);
	view.setWindowTitle(QObject::tr("Simple Tree Model"));
	view.show();

	return app.exec();
}
