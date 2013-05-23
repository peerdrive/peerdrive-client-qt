include(paths.pri)

INCLUDEPATH += $$PWD
DEPENDPATH += ../..
LIBS += -L$$PWD/peerdrive-qt -lpeerdrive-qt
DEFINES += ICON_PATH=\\\"$$ICON_PATH\\\"

QMAKE_CXXFLAGS += -ffunction-sections
QMAKE_CXXFLAGS += -fdata-sections

!macx:QMAKE_LFLAGS += -Wl,-gc-sections
macx: QMAKE_LFLAGS += -dead_strip -dead_strip_dylibs

!macx:QMAKE_LFLAGS_DEBUG += -Wl,-rpath=$$PWD/peerdrive-qt/
macx: QMAKE_LFLAGS_DEBUG += -Wl,-rpath,$$PWD/peerdrive-qt/
