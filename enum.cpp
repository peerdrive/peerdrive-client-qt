
#include <iostream>
#include <QCoreApplication>
#include "peerdrive.h"

static void printStore(const PMounts::Store *store, bool automatic)
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
	PMounts stores;

	foreach(PMounts::Store *store, stores.allStores())
		printStore(store, false);
}

int main(int argc, char** argv)
{
	QCoreApplication app(argc, argv);
	printStores();
	return 0;
}
