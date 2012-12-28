include(../../global.pri)

TEMPLATE = app
CONFIG += console
QT = core

TARGET = peerdrive

SOURCES += optparse.cpp
SOURCES += peerdrive.cpp
SOURCES += mount.cpp
SOURCES += umount.cpp

