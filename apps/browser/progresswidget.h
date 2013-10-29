#ifndef PROGRESSWIDGET_H
#define PROGRESSWIDGET_H

#include <QList>
#include <QString>
#include <QWidget>
#include <peerdrive-qt/peerdrive.h>

#include "replicatetask.h"

class QLabel;
class QProgressBar;
class QToolButton;

class ProgressWidget : public QWidget
{
	Q_OBJECT

public:
	ProgressWidget(const QList<PeerDrive::Link> &sources,
	               const PeerDrive::Link &destination, QWidget *parent = NULL);
	~ProgressWidget();

private slots:
	void start();
	void stop();
	void finished(PeerDrive::Error result);
	void linkActivated(const QString &link);

private:
	ReplicateTask *task;
	QList<PeerDrive::Link> sources;
	PeerDrive::Link destination;
	QLabel *text;
	QProgressBar *progressBar;
	QToolButton *stopBtn;
	QToolButton *redoBtn;
	QString itemText, toText;
};

#endif
