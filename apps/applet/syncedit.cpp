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
#include "syncedit.h"
#include "syncrules.h"
#include "utils.h"

#include <QVariant>
#include <QMessageBox>

#include <QtDebug>

// We cannot use QComboBox::findData() because it doesn't work for custom
// QVariant types.
static int findComboBoxDId(QComboBox *box, const PeerDrive::DId &doc)
{
	for (int i = 0; i < box->count(); i++) {
		if (box->itemData(i).value<PeerDrive::DId>() == doc)
			return i;
	}

	return -1;
}

SyncEdit::SyncEdit(SyncRules *rules, QWidget *parent)
	: QDialog(parent)
{
	this->rules = rules;

	setupUi(this);
	setWindowTitle(tr("Add synchronization rule"));
	connect(qApp, SIGNAL(focusChanged(QWidget *, QWidget*)), this, SLOT(focusChanged()));
	manualGroup->setVisible(false);

	// get all available stores
	PeerDrive::Mounts mounts;
	foreach (PeerDrive::Mounts::Store *s, mounts.regularStores()) {
		QString title = s->label + ": " + readTitle(Link(s->sid, s->sid));
		masterStore->addItem(title, QVariant::fromValue(s->sid));
		slaveStore->addItem(title, QVariant::fromValue(s->sid));
	}

	// try to select a combination wher no sync is defined in any direction
	// otherwise select one where no single sync is defined
	foreach (PeerDrive::Mounts::Store *s, mounts.regularStores()) {
		foreach (PeerDrive::Mounts::Store *d, mounts.regularStores()) {
			if (s == d)
				continue;
			if (rules->mode(s->sid, d->sid) != SyncRules::None)
				continue;
			masterStore->setCurrentIndex(findComboBoxDId(masterStore, s->sid));
			slaveStore->setCurrentIndex(findComboBoxDId(slaveStore, d->sid));

			if (rules->mode(d->sid, s->sid) == SyncRules::None)
				return;
		}
	}
}

SyncEdit::SyncEdit(SyncRules *rules, const PeerDrive::DId &from,
                   const PeerDrive::DId &to, QWidget *parent)
	: QDialog(parent)
{
	this->rules = rules;

	setupUi(this);
	setWindowTitle(tr("Change synchronization rule"));
	connect(qApp, SIGNAL(focusChanged(QWidget *, QWidget*)), this, SLOT(focusChanged()));
	manualGroup->setVisible(false);

	QString fromTitle = "<Unmounted store>";
	QString toTitle   = "<Unmounted store>";
	PeerDrive::Mounts mounts;
	foreach (PeerDrive::Mounts::Store *s, mounts.regularStores()) {
		if (s->sid == from)
			fromTitle = s->label + ": " + readTitle(Link(s->sid, s->sid));
		else if (s->sid == to)
			toTitle = s->label + ": " + readTitle(Link(s->sid, s->sid));
	}

	masterStore->addItem(fromTitle, QVariant::fromValue(from));
	slaveStore->addItem(toTitle, QVariant::fromValue(to));

	masterStore->setCurrentIndex(0);
	slaveStore->setCurrentIndex(0);
}

SyncEdit::~SyncEdit()
{
}

void SyncEdit::storesSelected()
{
	if (masterStore->currentIndex() == -1 || slaveStore->currentIndex() == -1)
		return;

	PeerDrive::DId from = masterStore->itemData(masterStore->currentIndex()).value<PeerDrive::DId>();
	PeerDrive::DId to = slaveStore->itemData(slaveStore->currentIndex()).value<PeerDrive::DId>();
	if (from == to) {
		profileFrame->setEnabled(false);
		noneProfile->setChecked(true);
		description->setText("You must select two different stores!");
		buttonBox->setStandardButtons(QDialogButtonBox::Cancel);
		return;
	} else {
		profileFrame->setEnabled(true);
		buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	}

	SyncRules::Mode fwdMode = rules->mode(from, to);
	SyncRules::Mode revMode = rules->mode(to, from);

	if (fwdMode == SyncRules::None && revMode == SyncRules::None)
		noneProfile->setChecked(true);
	else if (fwdMode == SyncRules::Merge && revMode == SyncRules::Merge)
		biDirProfile->setChecked(true);
	else if (fwdMode == SyncRules::FastForward && revMode == SyncRules::None)
		backupProfile->setChecked(true);
	else if (fwdMode == SyncRules::Merge && revMode == SyncRules::None)
		followProfile->setChecked(true);
	else
		manualProfile->setChecked(true);

	forwardType->setCurrentIndex(fwdMode);
	backwardType->setCurrentIndex(revMode);
}

void SyncEdit::focusChanged()
{
	QWidget *current = QApplication::focusWidget();

	if (current == noneProfile) {
		description->setText("noneProfile");
	} else if (current == biDirProfile) {
		description->setText("biDirProfile");
	} else if (current == backupProfile) {
		description->setText("backupProfile");
	} else if (current == followProfile) {
		description->setText("followProfile");
	} else if (current == manualProfile) {
		description->setText("manualProfile");
	} else if (current == forwardType) {
		description->setText("forwardType");
	} else if (current == backwardType) {
		description->setText("backwardType");
	}
}

void SyncEdit::accept()
{
	SyncRules::Mode fwdMode = SyncRules::None;
	SyncRules::Mode revMode = SyncRules::None;
	QString descr;

	if (biDirProfile->isChecked()) {
		fwdMode = revMode = SyncRules::Merge;
		descr = "Synchronize '%1' and '%2'";
	} else if (backupProfile->isChecked()) {
		fwdMode = SyncRules::FastForward;
		descr = "Backup '%1' to '%2'";
	} else if (followProfile->isChecked()) {
		revMode = SyncRules::Merge;
		descr = "Follow changes of '%2' in '%1'";
	} else if (manualProfile->isChecked()) {
		fwdMode = static_cast<SyncRules::Mode>(forwardType->currentIndex());
		revMode = static_cast<SyncRules::Mode>(backwardType->currentIndex());
		descr = "Custom syncronization of '%1' and '%2'"; // TODO
	}

	descr = descr.arg(masterStore->itemText(masterStore->currentIndex()))
		.arg(slaveStore->itemText(slaveStore->currentIndex()));

	PeerDrive::DId from = masterStore->itemData(masterStore->currentIndex()).value<PeerDrive::DId>();
	PeerDrive::DId to = slaveStore->itemData(slaveStore->currentIndex()).value<PeerDrive::DId>();
	rules->setMode(from, to, fwdMode);
	rules->setDescription(from, to, descr);
	rules->setMode(to, from, revMode);
	rules->setDescription(to, from, descr);

	if (!rules->save())
		QMessageBox::critical(this, tr("Synchronization rules"), tr("Could not save rules!"));

	QDialog::accept();
}

