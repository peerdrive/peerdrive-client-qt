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
#include <iostream>

#include "optparse.h"
#include "commands.h"

using namespace PeerDrive;

int cmd_umount(const QStringList &cmdLine)
{
	QStringList args = cmdLine.mid(2);
	OptionParser parser("usage: peerdrive umount [options] <label>");

	parser.addOption("remove", QStringList() << "-r" << "--remove", false)
		.setActionStoreTrue()
		.setHelp("Remove mount label from fstab");

	switch (parser.parseArgs(args)) {
		case OptionParser::Aborted:
			return 0;
		case OptionParser::Error:
			return 1;
		default:
			break;
	}

	if (parser.arguments().size() != 1) {
		parser.printError("wrong number of arguments");
		return 1;
	}

	QString label = parser.arguments().at(0);
	bool remove = parser.options()["remove"].toBool();

	Mounts mounts;
	Mounts::Store *store = mounts.fromLabel(label);
	if (!store) {
		std::cerr << qPrintable(QString("Label '%1' not mounted\n").arg(label));
		if (!remove)
			return 1;
	}

	int err = Mounts::unmount(store->sid);
	if (err) {
		std::cerr << "Unmount failed: " << err << "\n";
		return 2;
	}

	if (remove) {
		FSTab fstab;
		if (!fstab.load()) {
			std::cerr << "error: could not load 'sys:fstab'!\n";
			return 2;
		}

		if (!fstab.knownLabels().contains(label)) {
			std::cerr << qPrintable(QString("Remove failed: label '%1' not "
				"found in fstab\n").arg(label));
			return 1;
		}

		fstab.remove(label);
		if (!fstab.save()) {
			std::cerr << "Fstab update failed!\n";
			return 2;
		}
	}

	return 0;
}
