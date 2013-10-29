
#include <peerdrive-qt/pdsd.h>
#include "replicatetask.h"

ReplicateTask::ReplicateTask(const QList<PeerDrive::Link> &sources,
                             const PeerDrive::Link &folder)
	: sources(sources)
	, destination(folder)
	, lastProgress(0)
{
	foreach(const PeerDrive::Link &item, sources)
		progressMap[item] = 0;

	connect(&watch, SIGNAL(started(unsigned int)), this,
		SLOT(progressStarted(unsigned int)));
	connect(&watch, SIGNAL(changed(unsigned int)), this,
		SLOT(progressChanged(unsigned int)));
	connect(&watch, SIGNAL(finished(unsigned int)), this,
		SLOT(progressFinished(unsigned int)));
}

ReplicateTask::~ReplicateTask()
{
	wait();
}

void ReplicateTask::run()
{
	PeerDrive::Error err = PeerDrive::ErrNoError;
	PeerDrive::Folder folder(destination);

	foreach(const PeerDrive::Link &item, sources) {
		PeerDrive::Replicator rep;

		if (!rep.replicate(item, destination.store())) {
			err = rep.error();
			break;
		}

		int i = 3;
		do {
			if (!folder.load()) {
				err = folder.error();
				break;
			}

			if (folder.add(item)) {
				if (!folder.save())
					err = folder.error();
			}
		} while (err == PeerDrive::ErrConflict && i-- > 0);
		if (err)
			break;
	}

	emit finished(err);
}

void ReplicateTask::finishedItem(const PeerDrive::Link &item)
{
	progressMap[item] = 256;
	calculateProgress();
}

void ReplicateTask::progressStarted(unsigned int tag)
{
	// one of our items?
	foreach (const PeerDrive::Link &item, sources) {
		if (watch.item(tag) != item)
			continue;
		if (watch.destination(tag).store() != destination.store())
			continue;

		tags[tag] = item;
		break;
	}
}

void ReplicateTask::progressChanged(unsigned int tag)
{
	if (!tags.contains(tag))
		return;

	progressMap[tags[tag]] = watch.progress(tag);
	calculateProgress();
}

void ReplicateTask::progressFinished(unsigned int tag)
{
	if (!tags.contains(tag))
		return;

	progressMap[tags[tag]] = 256;
	tags.remove(tag);
	calculateProgress();
}

void ReplicateTask::calculateProgress()
{
	int overall = 0;

	foreach(int p, progressMap)
		overall += p;

	overall /= progressMap.size();

	if (overall != lastProgress) {
		lastProgress = overall;
		emit progress(overall);
	}
}


