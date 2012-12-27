TEMPLATE = lib
CONFIG += dll
QT += network

HEADERS += peerdrive.h peerdrive_internal.h pdsd.h
SOURCES += peerdrive.cpp pdsd.cpp

HEADERS += foldermodel.h foldermodel_internal.h
SOURCES += foldermodel.cpp

LIBS += -lprotobuf

PROTOS = peerdrive_client.proto
include(protobuf.pri)

