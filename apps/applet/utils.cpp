/*
 * PeerDrive
 * Copyright (C) 2013  Jan Klötzke <jan DOT kloetzke AT freenet DOT de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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

