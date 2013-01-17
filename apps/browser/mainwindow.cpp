
#include <peerdrive-qt/foldermodel.h>
#include <peerdrive-qt/pdsd.h>
#include <QtGui>

#include "mainwindow.h"

MainWindow::MainWindow()
{
	QAction *exitAct = new QAction(tr("E&xit"), this);
	exitAct->setShortcuts(QKeySequence::Quit);
	connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));

	QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addSeparator();
	fileMenu->addAction(exitAct);

	columnsMenu = menuBar()->addMenu(tr("&Columns"));
	connect(columnsMenu, SIGNAL(aboutToShow()), this, SLOT(showColumnsMenu()));

	model = new PeerDrive::FolderModel(this);
	connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)), this,
		SLOT(modelRowsInserted(QModelIndex,int,int)));
	connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this,
		SLOT(modelDataChanged(QModelIndex,QModelIndex)));
	connect(model, SIGNAL(modelReset()), this, SLOT(modelReset()));

	treeView = new QTreeView;
	treeView->setModel(model);
	treeView->sortByColumn(0, Qt::AscendingOrder);
	treeView->setSortingEnabled(true);
	connect(treeView, SIGNAL(activated(QModelIndex)), this,
		SLOT(itemActivated(QModelIndex)));

	setCentralWidget(treeView);
	setWindowTitle(QObject::tr("Simple Tree Model"));
	setUnifiedTitleAndToolBarOnMac(true);
}

MainWindow::~MainWindow()
{
}

void MainWindow::modelRowsInserted(const QModelIndex &parent, int start, int end)
{
	for (int i = start; i <= end; i++)
		allTypeCodes << model->data(model->index(i, 0, parent),
			PeerDrive::FolderModel::TypeCodeRole).toString();
}

void MainWindow::modelDataChanged(const QModelIndex &topLeft, const QModelIndex & /*bottomRight*/)
{
	allTypeCodes << model->data(topLeft, PeerDrive::FolderModel::TypeCodeRole).toString();
}

void MainWindow::modelReset()
{
	allTypeCodes.clear();
}

static void addSuperClasses(const QString &uti, QSet<QString> &types)
{
	foreach(const QString &super, PeerDrive::Registry::instance().conformes(uti)) {
		types << super;
		addSuperClasses(super, types);
	}
}

void MainWindow::showColumnsMenu()
{
	columnsMenu->clear();

	// also show types that belong to active columns but currently no active
	// items
	QSet<QString> activeTypes = allTypeCodes;
	QStringList columns = model->columns();
	foreach(const QString &t, columns) {
		QStringList l = t.split(":");
		if (l.at(0) != "")
			activeTypes << l.at(1);
	}

	addColumnEntry(columnsMenu, columns, ":size", "Size");
	addColumnEntry(columnsMenu, columns, ":mtime", "Modification time");
	addColumnEntry(columnsMenu, columns, ":type", "Type code");
	addColumnEntry(columnsMenu, columns, ":creator", "Creator code");
	addColumnEntry(columnsMenu, columns, ":comment", "Comment");
	columnsMenu->addSeparator();

	// add superclasses to the set
	foreach(const QString &uti, activeTypes)
		addSuperClasses(uti, activeTypes);

	// find all meta data keys sorted by type code
	foreach(const QString &uti, activeTypes) {
		PeerDrive::Value metaList = PeerDrive::Registry::instance().search(uti,
			"meta", false, PeerDrive::Value(PeerDrive::Value::LIST));

		if (metaList.size() == 0)
			continue;

		QMenu *subMenu = columnsMenu->addMenu(
			PeerDrive::Registry::instance().title(uti));

		for (int i = 0; i < metaList.size(); i++) {
			const PeerDrive::Value &entry = metaList[i];
			const PeerDrive::Value &rawPath = entry["key"];

			QString display = entry["display"].asString();

			QString key = uti + ":";
			for (int j = 0; j < rawPath.size(); j++) {
				if (j > 0)
					key += "/";
				key += rawPath[j].asString();
			}

			addColumnEntry(subMenu, columns, key, display);
		}
	}
}

void MainWindow::addColumnEntry(QMenu *menu, const QStringList &columns,
	const QString &key, const QString &display)
{
	bool visible = columns.contains(key);

	QAction *action = menu->addAction(display);
	action->setCheckable(true);
	action->setChecked(visible);
	action->setData(key);

	if (visible)
		connect(action, SIGNAL(triggered()), this, SLOT(removeColumn()));
	else
		connect(action, SIGNAL(triggered()), this, SLOT(insertColumn()));
}

void MainWindow::insertColumn()
{
	QAction *action = static_cast<QAction*>(QObject::sender());
	model->insertColumn(action->data().toString(), model->columnCount());
}

void MainWindow::removeColumn()
{
	QAction *action = static_cast<QAction*>(QObject::sender());
	int column = model->columns().indexOf(action->data().toString());
	if (column < 0)
		return;

	model->removeColumn(column);
}

void MainWindow::itemActivated(const QModelIndex &index)
{
	PeerDrive::Link item = model->data(index, PeerDrive::FolderModel::LinkRole)
		.value<PeerDrive::Link>();
	model->setRootItem(item);
}

