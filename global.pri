include(paths.pri)

INCLUDEPATH += $$PWD
DEPENDPATH += ../..
LIBS += -L$$PWD/peerdrive-qt -lpeerdrive-qt
DEFINES += ICON_PATH=\\\"$$ICON_PATH\\\"

QMAKE_CXXFLAGS += -ffunction-sections
QMAKE_CXXFLAGS += -fdata-sections
QMAKE_LFLAGS += -Wl,-gc-sections

QMAKE_LFLAGS_DEBUG += -Wl,-rpath=$$PWD/peerdrive-qt/
