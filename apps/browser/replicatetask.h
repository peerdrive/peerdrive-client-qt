#ifndef REPLICATETASK_H
#define REPLICATETASK_H

#include <QList>
#include <QMap>
#include <QThread>
#include <peerdrive-qt/peerdrive.h>

class ReplicateTask : public QThread
{
	Q_OBJECT

public:
	ReplicateTask(const QList<PeerDrive::Link> &sources, const PeerDrive::Link &folder);
	~ReplicateTask();

signals:
	void progress(int progress);
	void finished(PeerDrive::Error result);

protected:
	void run();

private slots:
	void finishedItem(const PeerDrive::Link &item);
	void progressStarted(unsigned int tag);
	void progressChanged(unsigned int tag);
	void progressFinished(unsigned int tag);

private:
	void calculateProgress();

	QList<PeerDrive::Link> sources;
	PeerDrive::Link destination;
	QMap<PeerDrive::Link, int> progressMap;
	int lastProgress;
	QMap<unsigned int, PeerDrive::Link> tags;
	PeerDrive::ProgressWatcher watch;
};

#endif
