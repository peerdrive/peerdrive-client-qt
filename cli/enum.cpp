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

#include <iostream>

#include <QCoreApplication>
#include <QtDebug>

#include "peerdrive.h"
#include "pdsd.h"

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

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	printStores();
	return 0;
}
