#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <QList>
#include <QWidget>
#include <peerdrive-qt/peerdrive.h>

class QVBoxLayout;

class Operations : public QWidget
{
	Q_OBJECT

public:
	Operations(QWidget *parent = NULL);
	~Operations();

public slots:
	void replicate(const QList<PeerDrive::Link> &sources, const PeerDrive::Link &destination);

private:
	QVBoxLayout *progressLayout;
};

#endif
