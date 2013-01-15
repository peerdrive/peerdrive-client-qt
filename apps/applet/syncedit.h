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


