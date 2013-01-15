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

#ifndef WINDOW_H
#define WINDOW_H

#include <QSystemTrayIcon>
#include <QDialog>
#include <QSet>

class QAction;
class QMenu;
class QModelIndex;
class SyncRules;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class QVBoxLayout;

namespace PeerDrive {
	class FolderModel;
	class FSTab;
	class LinkWatcher;
	class ProgressWatcher;
}

class Window : public QDialog
{
	Q_OBJECT

public:
	Window();

	void setVisible(bool visible);

protected:
	void closeEvent(QCloseEvent *event);

private slots:
	void iconActivated(QSystemTrayIcon::ActivationReason reason);
	void showTrayMenu();
	void hideTrayMenu();
	void rowsInserted(const QModelIndex &parent, int start, int end);
	void rowsRemoved(const QModelIndex &parent, int start, int end);
	void showFolderMenu();
	void fetched(const QModelIndex &parent);
	void openLink();
	void openLinkProperties();
	void mount();
	void unmount();
	void mountsModified();
	void fillSyncRules();
	void syncItemChanged();
	void addSyncRule();
	void editSyncRule();
	void removeSyncRule();
	void progressStart(unsigned int tag);
	void progressEnd(unsigned int tag);

private:
	QMenu *findMenu(const QModelIndex &index);
	QModelIndex findIndex(QMenu *menu);

	PeerDrive::FolderModel *model;
	QAction *restoreAction;
	QAction *quitAction;

	QSystemTrayIcon *trayIcon;
	QMenu *trayIconMenu;
	QListWidget *syncRulesList;
	QPushButton *addSync;
	QPushButton *editSync;
	QPushButton *removeSync;
	QLabel *idleLabel;
	QVBoxLayout *progressLayout;

	PeerDrive::FSTab *fstab;
	PeerDrive::LinkWatcher *watch;
	PeerDrive::ProgressWatcher *progressWatch;
	QSet<QString> knownMounted;
	SyncRules *syncRules;
};

#endif

