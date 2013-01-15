#ifndef PROGRESSWIDGET_H
#define PROGRESSWIDGET_H

#include <QWidget>

namespace PeerDrive {
	class ProgressWatcher;
}

class QLabel;
class QProgressBar;
class QToolButton;
class QSystemTrayIcon;

class ProgressWidget : public QWidget
{
	Q_OBJECT

public:
	ProgressWidget(PeerDrive::ProgressWatcher *watcher, unsigned int tag,
		QSystemTrayIcon *tray, QWidget *parent);

private slots:
	void progressChange(unsigned int tag);
	void progressEnd(unsigned int tag);
	void pause();
	void stop();
	void skip();
	void linkActivated(const QString &link);

private:
	unsigned int tag;
	PeerDrive::ProgressWatcher *watcher;
	QSystemTrayIcon *tray;
	int oldState;

	QLabel *text;
	QProgressBar *progressBar;
	QToolButton *pauseBtn;
	QToolButton *skipBtn;
	QString fromText, rawFromText;
	QString toText, rawToText;
	QString itemText, rawItemText;
};

#endif
