#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSet>

class DndFolderModel;
class QAction;
class QMenu;
class QTreeView;
class QModelIndex;

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow();
	~MainWindow();

private slots:
	void modelRowsInserted(const QModelIndex &parent, int start, int end);
	void modelDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
	void modelReset();
	void showColumnsMenu();
	void insertColumn();
	void removeColumn();
	void itemActivated(const QModelIndex &index);

private:
	void addColumnEntry(QMenu *menu, const QStringList &columns, const QString &key,
		const QString &display);

	DndFolderModel *model;
	QTreeView *treeView;
	QMenu *columnsMenu;
	QSet<QString> allTypeCodes;
};

#endif
