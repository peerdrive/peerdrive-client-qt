TEMPLATE = app
CONFIG += console
QT = core

INCLUDEPATH += ../peerdrive-qt
LIBS += -L../peerdrive-qt -lpeerdrive-qt
SOURCES += enum.cpp
TARGET = peerdrive
