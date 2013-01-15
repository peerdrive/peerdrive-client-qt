
#include <peerdrive-qt/peerdrive.h>
#include <peerdrive-qt/pdsd.h>
#include "utils.h"

QString readTitle(const PeerDrive::Link &link)
{
	QString name = "<Unknown>";
	try {
		PeerDrive::Document file(link);
		if (!file.peek())
			return name;

		QByteArray tmp;
		if (file.readAll(PeerDrive::Part::META, tmp) <= 0)
			return name;

		PeerDrive::Value meta = PeerDrive::Value::fromByteArray(tmp, link.store());
		name = meta["org.peerdrive.annotation"]["title"].asString();
	} catch (PeerDrive::ValueError&) {
	}

	if (name.length() > 42)
		name = name.left(40) + "...";
	return name;
}

