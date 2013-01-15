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

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QtDebug>
#include <iostream>

#include "commands.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

struct cmd {
	QString name;
	int (*fun)(const QStringList &args);
};

static struct cmd commands[] = {
	{ "mount", cmd_mount },
	{ "umount", cmd_umount },
};

static const char *help =
	"PeerDrive command line utility\n"
	"\n"
	"USAGE: peerdrive [-h|--help] [--version] COMMAND [ARGS]\n"
	"\n"
	"The most commonly used peerdrive commands are:\n"
	//"    export     Export files/folders from PeerDrive\n"
	//"    import     Import files/folders into PeerDrive\n"
	//"    ls         List PeerDrive documents\n"
	"    mount      List and/or mount stores\n"
	//"    replicate  Replicate documents\n"
	"    umount     Unmount stores\n"
	//"    unlink     Remove document from a folder\n"
	"\n"
	"Use 'peerdrive COMMAND --help' for more information on a specific command.\n";

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	QStringList arguments(QCoreApplication::arguments());

	if (arguments.size() < 2) {
		std::cerr << help;
		return 1;
	}

	if (arguments.at(1) == "--help" || arguments.at(1) == "-h") {
		std::cout << help;
		return 0;
	} else if (arguments.at(1) == "--version") {
		std::cout << "peerdrive cli version ?\n";
		return 0;
	} else {
		QString cmdName = arguments.at(1);

		for (unsigned int i = 0; i < ARRAY_SIZE(commands); i++)
			if (commands[i].name == cmdName)
				return commands[i].fun(arguments);
	}

	std::cerr << help;
	return 1;
}
