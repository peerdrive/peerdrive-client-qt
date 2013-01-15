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

#ifndef SYNCEDIT_H
#define SYNCEDIT_H

#include <QDialog>
#include "ui_syncedit.h"

namespace PeerDrive {
	class DId;
}
class SyncRules;

class SyncEdit : public QDialog, private Ui::SyncEdit
{
	Q_OBJECT

public:
	SyncEdit(SyncRules *rules, QWidget *parent);
	SyncEdit(SyncRules *rules, const PeerDrive::DId &from,
		const PeerDrive::DId &to, QWidget *parent);
	~SyncEdit();

private slots:
	void focusChanged();
	void storesSelected();
	void accept();

private:
	SyncRules *rules;
};

#endif


