include(../paths.pri)

TEMPLATE = lib
CONFIG += dll
QT += network

DEFINES += ICON_PATH=\\\"$$ICON_PATH\\\"

HEADERS += peerdrive.h peerdrive_internal.h pdsd.h
SOURCES += peerdrive.cpp pdsd.cpp

HEADERS += foldermodel.h foldermodel_internal.h
SOURCES += foldermodel.cpp

LIBS += -lprotobuf
PROTOS = peerdrive_client.proto
include(protobuf.pri)

macx:debug {
	QMAKE_POST_LINK = install_name_tool -id "@rpath/libpeerdrive-qt.1.dylib" $(TARGET)
}
