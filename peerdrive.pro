QT += network
CONFIG += console

SOURCES += peerdrive.cpp
SOURCES += enum.cpp
# SOURCES += peerdrive_client.pb.cc
LIBS += -lprotobuf

PROTOS = peerdrive_client.proto
include(protobuf.pri)
