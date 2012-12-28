#ifndef SYNCRULES_H
#define SYNCRULES_H

#include <peerdrive-qt/peerdrive.h>
#include <peerdrive-qt/pdsd.h>
#include <QObject>

using PeerDrive::Link;

class SyncRules : public QObject
{
	Q_OBJECT

public:
	enum Mode {
		None,
		FastForward,
		Latest,
		Merge
	};

	struct Rule {
		PeerDrive::DId from;
		PeerDrive::DId to;
		Mode mode;
	};

	SyncRules();
	~SyncRules();

	bool load();
	bool save() const;

	int size() const;
	int index(const PeerDrive::DId &from, const PeerDrive::DId &to) const;

	PeerDrive::DId from(int i) const;
	PeerDrive::DId to(int i) const;

	Mode mode(int i) const;
	Mode mode(const PeerDrive::DId &from, const PeerDrive::DId &to) const;
	void setMode(const PeerDrive::DId &from, const PeerDrive::DId &to, Mode mode);

	QString description(int i) const;
	QString description(const PeerDrive::DId &from, const PeerDrive::DId &to) const;
	void setDescription(const PeerDrive::DId &from, const PeerDrive::DId &to, QString descr);

signals:
	void modified();

private slots:
	void _modified(const Link &item);

private:
	bool changed;
	PeerDrive::Link link;
	PeerDrive::Value rules;
	PeerDrive::LinkWatcher watch;
};

#endif
