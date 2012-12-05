/*
 * PeerDrive
 * Copyright (C) 2012  Jan Klötzke <jan DOT kloetzke AT freenet DOT de>
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

#include <QStringList>
#include <QtDebug>
#include <iostream>

#include <peerdrive-qt/peerdrive.h>
#include <peerdrive-qt/pdsd.h>

#include "optparse.h"
#include "commands.h"

using namespace PeerDrive;

static void printStore(const Mounts::Store *store, bool automatic)
{
	std::cout << "'" << qPrintable(store->src) << "' as '"
		<< qPrintable(store->label) << "' type '"
		<< qPrintable(store->type) << "'";

	if (!store->options.isEmpty())
		std::cout << " (" << qPrintable(store->options) << ")";

	if (automatic)
		std::cout << " [auto]";

	std::cout << "\n";
}

static void printStores(void)
{
	Mounts stores;
	FSTab fstab;

	if (!fstab.load()) {
		std::cerr << "error: could not load 'sys:fstab'!\n";
		return;
	}

	foreach(Mounts::Store *store, stores.allStores())
		printStore(store, fstab.autoMounted(store->label));
}

int cmd_mount(const QStringList &cmdLine)
{
	QStringList args = cmdLine.mid(2);
	OptionParser parser("usage: peerdrive mount [options] [[<src>] <label>]");

	parser.addOption("type", QStringList() << "-t" << "--type")
		.setHelp("Store type (default: 'file')");

	parser.addOption("options", QStringList() << "-o" << "--options")
		.setHelp("Comma separated string of mount options");

	parser.addOption("credentials", QStringList() << "-c" << "--credentials")
		.setHelp("Credentials to mount store");

	parser.addOption("persist", QStringList() << "-p" << "--persist", false)
		.setActionStoreTrue()
		.setHelp("Save mount label permanently in fstab");

	parser.addOption("auto", QStringList() << "-a" << "--auto", false)
		.setActionStoreTrue()
		.setHelp("Mount store automatically on startup (depends on '-p')");

	parser.addOption("verbose", QStringList() << "-v" << "--verbose", false)
		.setActionStoreTrue()
		.setHelp("Verbose output");

	switch (parser.parseArgs(args)) {
		case OptionParser::Aborted:
			return 0;
		case OptionParser::Error:
			return 1;
		default:
			break;
	}

	if (parser.arguments().size() > 2) {
		parser.printError("too many arguments");
		return 1;
	}
	if (parser.options()["auto"].toBool() && !parser.options()["persist"].toBool()) {
		parser.printError("'--auto' depends on '--persist'");
		return 1;
	}
	if (parser.options()["persist"].toBool() && parser.arguments().size() < 2) {
		parser.printError("missing arguments");
		return 1;
	}

	printStores();
	return 0;
}
