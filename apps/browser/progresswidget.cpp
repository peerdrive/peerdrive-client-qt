
#include <QTextDocument>
#include <QLabel>
#include <QProgressBar>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <peerdrive-qt/pdsd.h>

#include "progresswidget.h"

namespace {

	QString titleWithLinks(const PeerDrive::Link &link)
	{
		return "<a href=\"" + link.uri() + "\">" +
			Qt::escape(PeerDrive::readTitle(link, 32)) + "</a>";
	}

}


ProgressWidget::ProgressWidget(const QList<PeerDrive::Link> &sources,
                               const PeerDrive::Link &destination, QWidget *parent)
	: QWidget(parent)
	, task(NULL)
	, sources(sources)
	, destination(destination)
{
	qRegisterMetaType<PeerDrive::Error>("PeerDrive::Error");

	text = new QLabel(this);
	text->setWordWrap(true);
	text->setTextFormat(Qt::RichText);
	connect(text, SIGNAL(linkActivated(QString)), this, SLOT(linkActivated(QString)));

	progressBar = new QProgressBar(this);
	progressBar->setMinimum(0);
	progressBar->setMaximum(256);

	stopBtn = new QToolButton(this);
	stopBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
	stopBtn->setIcon(QIcon(ICON_PATH "/progress-stop.png"));
	stopBtn->setToolTip("Abort");
	connect(stopBtn, SIGNAL(clicked()), this, SLOT(stop()));

	redoBtn = new QToolButton(this);
	redoBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
	redoBtn->setIcon(QIcon(ICON_PATH "/progress-retry.png"));
	redoBtn->setToolTip("Retry");
	redoBtn->hide();
	connect(redoBtn, SIGNAL(clicked()), this, SLOT(start()));

	QHBoxLayout *statusLayout = new QHBoxLayout;
	statusLayout->addWidget(text);
	statusLayout->addWidget(stopBtn);
	statusLayout->addWidget(redoBtn);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(statusLayout);
	mainLayout->addWidget(progressBar);
	setLayout(mainLayout);

	toText = titleWithLinks(destination);
	itemText = titleWithLinks(sources.at(0));
	switch (sources.size()) {
	case 0:
	case 1:
		break;
	case 2:
		itemText += " and " + titleWithLinks(sources.at(1));
		break;
	default:
		itemText += QString(" and %1 other items").arg(sources.size()-1);
		break;
	}

	start();
}

ProgressWidget::~ProgressWidget()
{
	if (task)
		delete task;
}

void ProgressWidget::start()
{
	text->setText(QString("Replicating %1 to %2").arg(itemText).arg(toText));
	stopBtn->show();
	redoBtn->hide();

	task = new ReplicateTask(sources, destination);
	connect(task, SIGNAL(progress(int)), progressBar, SLOT(setValue(int)));
	connect(task, SIGNAL(finished(PeerDrive::Error)), this,
		SLOT(finished(PeerDrive::Error)));

	task->start();
}

void ProgressWidget::stop()
{
	// TODO
}

void ProgressWidget::finished(PeerDrive::Error result)
{
	if (result) {
		stopBtn->hide();
		redoBtn->show();
		text->setText(QString("Error %1 while replicating %2 to %3").arg(result)
			.arg(itemText).arg(toText));
	} else
		deleteLater();

	delete task;
	task = NULL;
}

void ProgressWidget::linkActivated(const QString &link)
{
	// TODO
}
