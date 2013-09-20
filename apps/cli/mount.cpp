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

#include <QSet>
#include <QStringList>
#include <iostream>

#include <peerdrive-qt/peerdrive.h>
#include <peerdrive-qt/pdsd.h>

#include "optparse.h"
#include "commands.h"

using namespace PeerDrive;

static std::string string_to_hex(const std::string& input)
{
    static const char* const lut = "0123456789abcdef";
    size_t len = input.length();

    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i) {
        const unsigned char c = (unsigned char)input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }

    return output;
}

static void printStore(const Mounts::Store *store, bool automatic, bool showSId,
                       const QString &credentials = QString())
{
	std::cout << "'" << qPrintable(store->src) << "' as '"
		<< qPrintable(store->label) << "'";
	if (showSId)
		std::cout << " / " << string_to_hex(store->sid.toStdString());
	std::cout << " type '"
		<< qPrintable(store->type) << "'";

	if (!credentials.isEmpty())
		std::cout << " with '" << qPrintable(credentials) << "'";

	if (!store->options.isEmpty())
		std::cout << " (" << qPrintable(store->options) << ")";

	if (automatic)
		std::cout << " [auto]";

	std::cout << "\n";
}

static void printStores(FSTab &fstab, int verbosity)
{
	Mounts mounts;

	QList<Mounts::Store*> stores = verbosity >= 1 ? mounts.allStores()
		: mounts.regularStores();
	foreach (Mounts::Store *store, stores)
		printStore(store, fstab.autoMounted(store->label), verbosity >= 2);

	if (verbosity >= 1) {
		QSet<QString> unmounted = QSet<QString>::fromList(fstab.knownLabels());
		foreach (Mounts::Store *s, mounts.allStores())
			unmounted -= s->label;

		std::cout << "\nUnmounted stores:\n";
		Mounts::Store store;
		foreach (QString label, unmounted) {
			store.src = fstab.src(label);
			store.type = fstab.type(label);
			store.label = label;
			store.options = fstab.options(label);
			QString credentials = fstab.credentials(label);
			printStore(&store, fstab.autoMounted(label), false, credentials);
		}
	}
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
		.setActionCounter()
		.setHelp("Increase output verbosity");

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

	FSTab fstab;
	if (!fstab.load()) {
		std::cerr << "error: could not load 'sys:fstab'!\n";
		return 2;
	}

	int verbosity = parser.options()["verbose"].toInt();

	if (parser.arguments().size() == 0) {
		printStores(fstab, verbosity);
		return 0;
	}

	QString type = "file";
	QString options;
	QString credentials;
	QString label;
	QString src;

	if (parser.arguments().size() == 1) {
		label = parser.arguments().at(0);
		if (!fstab.knownLabels().contains(label)) {
			std::cerr << "Label '" << qPrintable(label)
				<< "' not found in fstab!\n";
			return 1;
		}

		src = fstab.src(label);
		type = fstab.type(label);
		options = fstab.options(label);
		credentials = fstab.credentials(label);
	} else {
		src = parser.arguments().at(0);
		label = parser.arguments().at(1);
	}

	// command line overrides fstab/defaults
	if (parser.options().contains("type"))
		type = parser.options()["type"].toString();
	if (parser.options().contains("options"))
		options = parser.options()["options"].toString();
	if (parser.options().contains("credentials"))
		credentials = parser.options()["credentials"].toString();

	// already in fstab?
	bool persist = parser.options()["persist"].toBool();
	if (persist && fstab.knownLabels().contains(label)) {
		std::cerr << "Label '" << qPrintable(label)
			<< "' already defined in fstab!\n";
		return 1;
	}

	// try to mount
	if (verbosity >= 1) {
		QString msg = QString("Mounting '%1' as '%2' type '%3'").arg(src, label,
			type);
		if (!options.isEmpty())
			msg += " (" + options + ")";
		std::cout << qPrintable(msg) << "...\n";
	}

	Error err;
	DId sid = Mounts::mount(&err, src, label, type, options, credentials);
	if (err) {
		std::cerr << "Mount failed: " << err << "\n";
		return 2;
	}

	// add to fstab if requested
	if (persist) {
		fstab.add(label, src, type, options, credentials);
		if (parser.options()["auto"].toBool())
			fstab.setAutoMounted(label, true);

		if (!fstab.save()) {
			std::cerr << "Fstab update failed!\n";
			return 2;
		}
	}

	if (verbosity >= 1) {
		std::cout << "Mounted '" << qPrintable(label) << "'";
		if (verbosity >= 2)
			std::cout << " / "<< string_to_hex(sid.toStdString());
		std::cout << "\n";
	}

	return 0;
}
