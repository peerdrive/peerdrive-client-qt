/*
 * PeerDrive
 * Copyright (C) 2013  Jan Klötzke <jan DOT kloetzke AT freenet DOT de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <peerdrive-qt/peerdrive.h>
#include <peerdrive-qt/pdsd.h>
#include <peerdrive-qt/foldermodel.h>

#include <QtGui>
#include <QMessageBox>
#include <QProcess>
#include <QListWidget>

#include "window.h"
#include "syncrules.h"
#include "syncedit.h"
#include "progresswidget.h"

#include <QtDebug>

struct OpenInfo {
	PeerDrive::Link link;
	PeerDrive::Link referrer;
	QString executable;
};
Q_DECLARE_METATYPE(OpenInfo);

class SyncListItem : public QListWidgetItem
{
public:
	SyncListItem(const PeerDrive::DId &from, const PeerDrive::DId &to, QListWidget *parent)
		: QListWidgetItem(parent)
	{
		this->from = from;
		this->to = to;
	}

	PeerDrive::DId from;
	PeerDrive::DId to;
};

Window::Window()
{
	fstab = new PeerDrive::FSTab(true, this);
	if (!fstab->load())
		qDebug() << "Cannot load sys:/fstab";

	progressWatch = new PeerDrive::ProgressWatcher(this);
	connect(progressWatch, SIGNAL(started(unsigned int)),
		this, SLOT(progressStart(unsigned int)));
	connect(progressWatch, SIGNAL(finished(unsigned int)),
		this, SLOT(progressEnd(unsigned int)));

	syncRules = new SyncRules;
	if (!syncRules->load())
		qDebug() << "Cannot load sys:/syncrules";
	connect(syncRules, SIGNAL(modified()), this, SLOT(fillSyncRules()));

	PeerDrive::Mounts mounts;
	foreach (PeerDrive::Mounts::Store *s, mounts.regularStores())
		knownMounted << s->label;

	watch = new PeerDrive::LinkWatcher(this);
	connect(watch, SIGNAL(modified(Link)), this, SLOT(mountsModified()));
	connect(watch, SIGNAL(modified(Link)), this, SLOT(fillSyncRules()));
	watch->addWatch(PeerDrive::LinkWatcher::rootDoc);

	restoreAction = new QAction(tr("&Restore"), this);
	connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));
	quitAction = new QAction(tr("&Quit"), this);
	connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));

	model = new PeerDrive::FolderModel(this);
	connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)), this,
		SLOT(rowsInserted(QModelIndex,int,int)));
	connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)), this,
		SLOT(rowsRemoved(QModelIndex,int,int)));
	connect(model, SIGNAL(fetched(QModelIndex)), this, SLOT(fetched(QModelIndex)));

	trayIconMenu = new QMenu(this);
	connect(trayIconMenu, SIGNAL(aboutToShow()), this, SLOT(showTrayMenu()));
	connect(trayIconMenu, SIGNAL(aboutToHide()), this, SLOT(hideTrayMenu()));
	hideTrayMenu(); // prime menu

	trayIcon = new QSystemTrayIcon(this);
	trayIcon->setContextMenu(trayIconMenu);
	trayIcon->setIcon(QIcon(ICON_PATH "/peerdrive.png"));
	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
			this, SLOT(iconActivated(QSystemTrayIcon::ActivationReason)));
	trayIcon->show();

	// window content

	idleLabel = new QLabel(tr("PeerDrive is idle"));
	progressLayout = new QVBoxLayout;
	progressLayout->addWidget(idleLabel);
	QGroupBox *progressGroupBox = new QGroupBox(tr("Current activity"));
	progressGroupBox->setLayout(progressLayout);

	syncRulesList = new QListWidget;
	syncRulesList->setSortingEnabled(true);
	connect(syncRulesList, SIGNAL(itemSelectionChanged()), this, SLOT(syncItemChanged()));

	QVBoxLayout *syncButtons = new QVBoxLayout;
	addSync = new QPushButton("Add");
	editSync = new QPushButton("Edit");
	editSync->setEnabled(false);
	removeSync = new QPushButton("Remove");
	removeSync->setEnabled(false);
	connect(addSync, SIGNAL(clicked()), this, SLOT(addSyncRule()));
	connect(editSync, SIGNAL(clicked()), this, SLOT(editSyncRule()));
	connect(removeSync, SIGNAL(clicked()), this, SLOT(removeSyncRule()));

	QHBoxLayout *syncLayout = new QHBoxLayout;
	syncButtons->addWidget(addSync);
	syncButtons->addWidget(editSync);
	syncButtons->addWidget(removeSync);
	syncButtons->addStretch();

	syncLayout->addWidget(syncRulesList);
	syncLayout->addLayout(syncButtons);
	QGroupBox *syncGroupBox = new QGroupBox(tr("Synchronization rules"));
	syncGroupBox->setLayout(syncLayout);

	QVBoxLayout *rootLayout = new QVBoxLayout;
	rootLayout->addWidget(progressGroupBox);
	rootLayout->addWidget(syncGroupBox);
	setLayout(rootLayout);

	setWindowTitle(tr("PeerDrive"));
	setWindowIcon(QIcon(ICON_PATH "/peerdrive.png"));
	setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);

	fillSyncRules();
}

void Window::showTrayMenu()
{
	// clear all menues to save some memory...
	// TODO: reset the model after some timeout too to save additional memory,
	// especially when the applet is never closed and used much...
	trayIconMenu->clear();
	trayIconMenu->addSeparator();

	QList<QString> allLabels = fstab->knownLabels();
	PeerDrive::Mounts mounts;

	foreach (PeerDrive::Mounts::Store *s, mounts.regularStores())
		allLabels.removeAll(s->label);

	QString text;
	foreach(const QString &label, allLabels) {
		text = "Mount ";
		text += label;
		QAction *mount = trayIconMenu->addAction(text);
		mount->setData(label);
		connect(mount, SIGNAL(triggered()), this, SLOT(mount()));
	}

	trayIconMenu->addSeparator();
	trayIconMenu->addAction(restoreAction);
	trayIconMenu->addSeparator();
	trayIconMenu->addAction(quitAction);

	// add any available stores
	QModelIndex root;
	if (model->rowCount(root) > 0)
		rowsInserted(root, 0, model->rowCount(root)-1);
}

void Window::hideTrayMenu()
{
	// The menu is cleared every time. Re-establish the showFolderMenu slot.
	connect(trayIconMenu, SIGNAL(aboutToShow()), this, SLOT(showFolderMenu()));
}

/**
 * Called from the model when it has fetched new rows or by us when a sub-menu
 * has to be populated.
 */
void Window::rowsInserted(const QModelIndex &parent, int start, int end)
{
	QMenu *menu = findMenu(parent);
	if (!menu)
		return;

	QAction *beforeAction = start >= menu->actions().size()
		? NULL : menu->actions().at(start);
	for (int i = start; i <= end; i++) {
		QModelIndex idx = model->index(i, 0, parent);

		QString title = model->data(idx, Qt::DisplayRole).toString();
		if (title.length() > 42)
			title = title.left(40) + "...";

		PeerDrive::Link link = model->data(idx, PeerDrive::FolderModel::LinkRole).value<PeerDrive::Link>();
		PeerDrive::RevInfo stat(link.rev());
		QStringList execList = PeerDrive::Registry::instance().executables(stat.type());

		QAction *action;
		if (model->rowCount(idx) > 0 || model->canFetchMore(idx)) {
			QMenu *subMenu = new QMenu(title, menu);
			subMenu->setEnabled(false);
			connect(subMenu, SIGNAL(aboutToShow()), this, SLOT(showFolderMenu()));
			action = subMenu->menuAction();

			if (model->canFetchMore(idx) && menu->isVisible())
				model->fetchMore(idx);

			if (execList.size() > 0) {
				subMenu->addSeparator();

				QAction *open = subMenu->addAction("Open");
				OpenInfo info;
				info.link = link;
				if (parent.isValid())
					info.referrer = model->data(parent, PeerDrive::FolderModel::LinkRole).value<PeerDrive::Link>();
				info.executable = execList.at(0);
				open->setData(QVariant::fromValue(info));
				connect(open, SIGNAL(triggered()), this, SLOT(openLink()));

				if (execList.size() > 1) {
					QMenu *execMenu = subMenu->addMenu("Open with");
					foreach (const QString &e, execList) {
						open = execMenu->addAction(e);
						info.executable = e;
						open->setData(QVariant::fromValue(info));
						connect(open, SIGNAL(triggered()), this, SLOT(openLink()));
					}
				}
			}

			if (!parent.isValid()) {
				QAction *unmount = subMenu->addAction("Unmount");
				unmount->setIcon(QIcon(ICON_PATH "/unmount.png"));
				unmount->setData(QVariant::fromValue(link.store()));
				connect(unmount, SIGNAL(triggered()), this, SLOT(unmount()));
			}

			subMenu->addSeparator();
			QAction *properties = subMenu->addAction("Properties");
			properties->setData(QVariant::fromValue(link));
			connect(properties, SIGNAL(triggered()), this, SLOT(openLinkProperties()));
		} else {
			action = new QAction(title, menu);
			if (execList.size() > 0) {
				OpenInfo info;
				info.link = link;
				if (parent.isValid())
					info.referrer = model->data(parent, PeerDrive::FolderModel::LinkRole).value<PeerDrive::Link>();
				info.executable = execList.at(0);
				action->setData(QVariant::fromValue(info));
				connect(action, SIGNAL(triggered()), this, SLOT(openLink()));
			}
		}

		QIcon icon = model->data(idx, Qt::DecorationRole).value<QIcon>();
		if (!icon.isNull())
			action->setIcon(icon);

		menu->insertAction(beforeAction, action);
	}
}

void Window::rowsRemoved(const QModelIndex &parent, int start, int end)
{
	QMenu *menu = findMenu(parent);
	if (!menu)
		return;
	if (!menu->isEnabled())
		return;

	for (int i = start; i <= end; i++)
		menu->removeAction(menu->actions().at(start)); // TODO: memory leak?
}

void Window::fetched(const QModelIndex &parent)
{
	QMenu *menu = findMenu(parent);
	if (!menu)
		return;

	menu->setEnabled(true);
}

void Window::showFolderMenu()
{
	QMenu *menu = dynamic_cast<QMenu*>(QObject::sender());
	QModelIndex idx = findIndex(menu);

	/*
	 * All menues to be shown are already populated. As we get visible now,
	 * check that all sub-menues are populated or the model is told to fetch
	 * these sub-entries.
	 */
	for (int i = 0; i < model->rowCount(idx); i++) {
		QModelIndex child = model->index(i, 0, idx);
		if (model->canFetchMore(child)) {
			// This entry is disabled and will be enabled once the model has
			// fetched all sub-entries
			model->fetchMore(child);
		} else if (model->rowCount(child) > 0) {
			// The model has already sub-entries. Populate the sub-menu...
			rowsInserted(child, 0, model->rowCount(child)-1);
			menu->actions().at(i)->menu()->setEnabled(true);
		}
	}

	// do everything above only once per menu
	disconnect(menu, SIGNAL(aboutToShow()), this, SLOT(showFolderMenu()));
}

QMenu *Window::findMenu(const QModelIndex &index)
{
	if (!index.isValid())
		return trayIconMenu;

	QMenu *parentMenu = findMenu(index.parent());
	if (!parentMenu)
		return NULL;

	QList<QAction*> actions = parentMenu->actions();
	return index.row() >= actions.size() ? NULL : actions.at(index.row())->menu();
}

QModelIndex Window::findIndex(QMenu *menu)
{
	if (menu == trayIconMenu)
		return QModelIndex();

	QMenu *parentMenu = dynamic_cast<QMenu*>(menu->parent());
	QModelIndex parent = findIndex(parentMenu);
	int row = parentMenu->actions().indexOf(menu->menuAction());

	return model->index(row, 0, parent);
}

void Window::setVisible(bool visible)
{
	restoreAction->setEnabled(!visible);
	QDialog::setVisible(visible);
}

void Window::closeEvent(QCloseEvent *event)
{
	hide();
	event->ignore();
}

void Window::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
	switch (reason) {
		case QSystemTrayIcon::Trigger:
		case QSystemTrayIcon::DoubleClick:
			setVisible(!isVisible());
			break;
		default:
			break;
	}
}

void Window::openLink()
{
	QAction *action = dynamic_cast<QAction*>(QObject::sender());
	OpenInfo info = action->data().value<OpenInfo>();
	QStringList args;

	if (info.referrer.isValid()) {
		args << "--referrer";
		args << info.referrer.uri();
	}
	args << info.link.uri();

	bool couldStart = QProcess::startDetached(info.executable, args);
	if (!couldStart) {
		QString msg = QString("Could not start '%1'").arg(info.executable);
		QMessageBox::critical(this, "Open item", msg);
	}
}

void Window::openLinkProperties()
{
	QAction *action = dynamic_cast<QAction*>(QObject::sender());
	PeerDrive::Link link = action->data().value<PeerDrive::Link>();
	qDebug() << "prop" << link.uri();
}

void Window::mount()
{
	QAction *action = dynamic_cast<QAction*>(QObject::sender());
	QString label = action->data().toString();

	if (!fstab->knownLabels().contains(label))
		return;

	int res = 0;
	PeerDrive::Mounts::mount(&res, fstab->src(label), label, fstab->type(label),
		fstab->options(label), fstab->credentials(label)).toByteArray();
	if (res) {
		QString msg = QString("Mount operation failed: %1").arg(res);
		QMessageBox::warning(this, label, msg);
	}
}

void Window::unmount()
{
	QAction *action = dynamic_cast<QAction*>(QObject::sender());
	PeerDrive::DId store = action->data().value<PeerDrive::DId>();

	int res = PeerDrive::Mounts::unmount(store);
	if (res) {
		QString msg = QString("Unmount operation failed: %1").arg(res);
		QMessageBox::warning(this, "Unmount", msg);
	}
}

void Window::mountsModified()
{
	QSet<QString> nowMounted;

	PeerDrive::Mounts mounts;
	foreach (PeerDrive::Mounts::Store *s, mounts.regularStores())
		nowMounted << s->label;

	foreach (const QString &label, knownMounted - nowMounted) {
		QString msg = QString("Store '%1' has been unmounted").arg(label);
		trayIcon->showMessage("Unmount", msg);
	}

	foreach (const QString &label, nowMounted - knownMounted) {
		QString msg = QString("Store '%1' has been mounted").arg(label);
		trayIcon->showMessage("Mount", msg);
	}

	knownMounted = nowMounted;
}

void Window::fillSyncRules()
{
	bool canAddRule = false;

	syncRulesList->clear();

	PeerDrive::Mounts mounts;
	QSet<PeerDrive::DId> mounted;
	foreach (PeerDrive::Mounts::Store *s, mounts.regularStores()) {
		mounted << s->sid;
		if (!canAddRule)
			foreach (PeerDrive::Mounts::Store *d, mounts.regularStores())
				if (s != d && syncRules->mode(s->sid, d->sid) == SyncRules::None)
					canAddRule = true;
	}

	addSync->setEnabled(canAddRule);

	for (int i = 0; i < syncRules->size(); i++) {
		PeerDrive::DId from = syncRules->from(i);
		PeerDrive::DId to = syncRules->to(i);

		// skip if reverse rule exists (they are combined as far as the user is
		// concerned)
		if (syncRules->index(to, from) > i)
			continue;

		SyncListItem *item = new SyncListItem(from, to, syncRulesList);
		item->setText(syncRules->description(i));
		//if (mounted.contains(from) && mounted.contains(to))
		//	item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		//else
		//	item->setFlags(Qt::ItemIsSelectable);
	}
}

void Window::syncItemChanged()
{
	bool selected = !syncRulesList->selectedItems().isEmpty();

	editSync->setEnabled(selected);
	removeSync->setEnabled(selected);
}

void Window::addSyncRule()
{
	SyncEdit *editor = new SyncEdit(syncRules, this);
	editor->exec();
}

void Window::editSyncRule()
{
	SyncListItem *item = static_cast<SyncListItem*>(syncRulesList->currentItem());

	SyncEdit *editor = new SyncEdit(syncRules, item->from, item->to, this);
	editor->exec();
}

void Window::removeSyncRule()
{
	SyncListItem *item = static_cast<SyncListItem*>(syncRulesList->currentItem());

	QString q = QString(tr("Are you sure you want to remove the rule '%1'?"))
		.arg(item->text());
	QMessageBox::StandardButton choice = QMessageBox::question(this,
		tr("Remove synchronization rule"), q, QMessageBox::Yes | QMessageBox::No,
		QMessageBox::No);

	if (choice == QMessageBox::Yes) {
		syncRules->setMode(item->from, item->to, SyncRules::None);
		syncRules->setMode(item->to, item->from, SyncRules::None);

		if (!syncRules->save())
			QMessageBox::critical(this, tr("Synchronization rules"),
				tr("Could not save rules!"));
	}
}

void Window::progressStart(unsigned int tag)
{
	idleLabel->hide();
	ProgressWidget *w = new ProgressWidget(progressWatch, tag, trayIcon, this);
	progressLayout->addWidget(w);
}

void Window::progressEnd(unsigned int /*tag*/)
{
	if (progressWatch->tags().isEmpty())
		idleLabel->show();
}

