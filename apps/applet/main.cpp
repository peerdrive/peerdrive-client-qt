 #include <QtGui>

 #include "window.h"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	QApplication::setQuitOnLastWindowClosed(false);

	Window window;
	return app.exec();
}
