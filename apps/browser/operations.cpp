
#include <QVBoxLayout>
#include "operations.h"
#include "progresswidget.h"

Operations::Operations(QWidget *parent)
	: QWidget(parent)
{
	progressLayout = new QVBoxLayout;
	progressLayout->addStretch();
	setLayout(progressLayout);
}

Operations::~Operations()
{
}

void Operations::replicate(const QList<PeerDrive::Link> &sources,
                           const PeerDrive::Link &destination)
{
	ProgressWidget *w = new ProgressWidget(sources, destination);
	progressLayout->insertWidget(0, w);
}

