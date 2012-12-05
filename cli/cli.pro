TEMPLATE = app
CONFIG += console
QT = core

INCLUDEPATH += ..
LIBS += -L../peerdrive-qt -lpeerdrive-qt
TARGET = peerdrive

QMAKE_CXXFLAGS += -ffunction-sections
QMAKE_CXXFLAGS += -fdata-sections
QMAKE_LFLAGS += -Wl,-gc-sections

SOURCES += optparse.cpp
SOURCES += peerdrive.cpp
SOURCES += mount.cpp

