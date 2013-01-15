
#include <peerdrive-qt/peerdrive.h>
#include <peerdrive-qt/pdsd.h>
#include "progresswidget.h"
#include "utils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

#include <QtDebug>

ProgressWidget::ProgressWidget(PeerDrive::ProgressWatcher *watcher,
	unsigned int tag, QSystemTrayIcon *tray, QWidget *parent)
	: QWidget(parent)
{
	this->watcher = watcher;
	this->tag = tag;
	this->tray = tray;
	this->oldState = -1;

	connect(watcher, SIGNAL(changed(unsigned int)),
		this, SLOT(progressChange(unsigned int)));
	connect(watcher, SIGNAL(finished(unsigned int)),
		this, SLOT(progressEnd(unsigned int)));

	text = new QLabel(this);
	text->setWordWrap(true);
	text->setTextFormat(Qt::RichText);
	connect(text, SIGNAL(linkActivated(QString)), this, SLOT(linkActivated(QString)));
	progressBar = new QProgressBar(this);
	progressBar->setMinimum(0);
	progressBar->setMaximum(0);
	pauseBtn = new QToolButton(this);
	pauseBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
	pauseBtn->setIcon(QIcon(ICON_PATH "/progress-pause.png"));
	pauseBtn->setToolTip("Pause");
	connect(pauseBtn, SIGNAL(clicked()), this, SLOT(pause()));
	skipBtn = new QToolButton(this);
	skipBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
	skipBtn->setIcon(QIcon(ICON_PATH "/progress-skip.png"));
	skipBtn->setToolTip("Skip");
	skipBtn->hide();
	connect(skipBtn, SIGNAL(clicked()), this, SLOT(skip()));

	QHBoxLayout *statusLayout = new QHBoxLayout;
	statusLayout->addWidget(text);
	statusLayout->addWidget(pauseBtn);
	statusLayout->addWidget(skipBtn);
	if (watcher->type(tag) != PeerDrive::ProgressWatcher::Synchronization) {
		QToolButton *stopBtn = new QToolButton(this);
		stopBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
		stopBtn->setIcon(QIcon(ICON_PATH "/progress-stop.png"));
		stopBtn->setToolTip("Abort");
		connect(stopBtn, SIGNAL(clicked()), this, SLOT(stop()));
		statusLayout->addWidget(stopBtn);
	}

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(statusLayout);
	mainLayout->addWidget(progressBar);
	setLayout(mainLayout);

	rawFromText = readTitle(watcher->source(tag));
	fromText = "<a href=\"" + watcher->source(tag).uri() + "\">" +
		Qt::escape(rawFromText) + "</a>";
	rawToText = readTitle(watcher->destination(tag));
	toText = "<a href=\"" + watcher->destination(tag).uri() + "\">" +
		Qt::escape(rawToText) + "</a>";
	rawItemText = readTitle(watcher->item(tag));
	itemText = "<a href=\"" + watcher->item(tag).uri() + "\">" +
		Qt::escape(rawItemText) + "</a>";

	progressChange(tag);
}

void ProgressWidget::progressChange(unsigned int tag)
{
	if (tag != this->tag)
		return;

	int val = watcher->progress(tag);
	if (val > 0)
		progressBar->setMaximum(255);
	progressBar->setValue(val);

	if (oldState == watcher->state(tag))
		return;
	oldState = watcher->state(tag);

	bool isSync = watcher->type(tag) == PeerDrive::ProgressWatcher::Synchronization;
	bool isErr = watcher->state(tag) == PeerDrive::ProgressWatcher::Error;
	progressBar->setEnabled(!isErr);
	skipBtn->setVisible(isErr);

	QString msg, rawMsg;

	switch (oldState) {
		case PeerDrive::ProgressWatcher::Running:
			if (isSync)
				msg = "Synchronizing %1 and %2";
			else
				msg = "Replicating %3 from %1 to %2";

			pauseBtn->setIcon(QIcon(ICON_PATH "/progress-pause.png"));
			pauseBtn->setToolTip("Pause");
			break;

		case PeerDrive::ProgressWatcher::Paused:
			if (isSync)
				msg = "Paused synchronization of %1 and %2";
			else
				msg = "Paused replication of %3 from %1 to %2";

			pauseBtn->setIcon(QIcon(ICON_PATH "/progress-start.png"));
			pauseBtn->setToolTip("Resume");
			break;

		case PeerDrive::ProgressWatcher::Error:
			if (isSync) {
				msg = "Error %1 at %2 while synchronizing %3 and %4";
				rawMsg = "Error %1 at '%2' while synchronizing '%3' and '%4'";
			} else {
				msg = "Error %1 at %2 while replicating %5 from %3 to %4";
				rawMsg = "Error %1 at '%2' while replicating '%5' from '%3' to '%4'";
			}

			msg = msg.arg(watcher->error(tag)).arg("<a href=\"" +
				watcher->errorItem(tag).uri() + "\">" +
				Qt::escape(readTitle(watcher->errorItem(tag))) +
				"</a>");

			pauseBtn->setIcon(QIcon(ICON_PATH "/progress-retry.png"));
			pauseBtn->setToolTip("Retry");

			break;
	}

	msg = msg.arg(fromText).arg(toText);
	if (!isSync)
		msg = msg.arg(itemText);

	if (oldState == PeerDrive::ProgressWatcher::Error) {
		rawMsg = rawMsg.arg(watcher->error(tag))
			.arg(readTitle(watcher->errorItem(tag)))
			.arg(rawFromText).arg(rawToText);
		if (!isSync)
			rawMsg = rawMsg.arg(rawItemText);
		tray->showMessage("Error", rawMsg, QSystemTrayIcon::Warning);
	}
	text->setText(msg);
}

void ProgressWidget::progressEnd(unsigned int tag)
{
	if (tag == this->tag)
		deleteLater();
}

void ProgressWidget::pause()
{
	if (oldState == PeerDrive::ProgressWatcher::Running)
		PeerDrive::ProgressWatcher::pause(tag);
	else
		PeerDrive::ProgressWatcher::resume(tag);
}

void ProgressWidget::stop()
{
	PeerDrive::ProgressWatcher::stop(tag);
}

void ProgressWidget::skip()
{
	PeerDrive::ProgressWatcher::resume(tag, true);
}

void ProgressWidget::linkActivated(const QString &linkStr)
{
	PeerDrive::Link link(linkStr);

	if (!link.update())
		return;

	PeerDrive::RevInfo stat(link.rev());
	QStringList execList = PeerDrive::Registry::instance().executables(stat.type());
	if (execList.isEmpty())
		return;

	QString executable = execList.at(0);

	bool couldStart = QProcess::startDetached(executable, QStringList() << link.uri());
	if (!couldStart) {
		QString msg = QString("Could not start '%1'").arg(executable);
		QMessageBox::critical(this, "Open item", msg);
	}
}

