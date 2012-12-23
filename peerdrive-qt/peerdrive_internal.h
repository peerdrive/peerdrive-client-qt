/*
 * This file is part of the PeerDrive Qt4 library.
 * Copyright (C) 2012  Jan Klötzke <jan DOT kloetzke AT freenet DOT de>
 *
 * PeerDrive is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * PeerDrive is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PeerDrive. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _PEERDRIVE_INTERNAL_H_
#define _PEERDRIVE_INTERNAL_H_

#include <QByteArray>
#include <QMap>
#include <QMutex>
#include <QQueue>
#include <QTcpSocket>
#include <QThread>
#include <QWaitCondition>

#include "peerdrive.h"
#include "peerdrive_client.pb.h"

#define ERROR_MSG           0x000
#define INIT_MSG            0x001
#define ENUM_MSG            0x002
#define LOOKUP_DOC_MSG      0x003
#define LOOKUP_REV_MSG      0x004
#define STAT_MSG            0x005
#define PEEK_MSG            0x006
#define CREATE_MSG          0x007
#define FORK_MSG            0x008
#define UPDATE_MSG          0x009
#define RESUME_MSG          0x00a
#define READ_MSG            0x00b
#define TRUNC_MSG           0x00c
#define WRITE_BUFFER_MSG    0x00d
#define WRITE_COMMIT_MSG    0x00e
#define GET_FLAGS_MSG       0x00f
#define SET_FLAGS_MSG       0x010
#define GET_TYPE_MSG        0x011
#define SET_TYPE_MSG        0x012
#define GET_PARENTS_MSG     0x013
#define MERGE_MSG           0x014
#define REBASE_MSG          0x015
#define COMMIT_MSG          0x016
#define SUSPEND_MSG         0x017
#define CLOSE_MSG           0x018
#define WATCH_ADD_MSG       0x019
#define WATCH_REM_MSG       0x01a
#define WATCH_PROGRESS_MSG  0x01b
#define FORGET_MSG          0x01c
#define DELETE_DOC_MSG      0x01d
#define DELETE_REV_MSG      0x01e
#define FORWARD_DOC_MSG     0x01f
#define REPLICATE_DOC_MSG   0x020
#define REPLICATE_REV_MSG   0x021
#define MOUNT_MSG           0x022
#define UNMOUNT_MSG         0x023
#define GET_PATH_MSG        0x024
#define WATCH_MSG           0x025
#define PROGRESS_START_MSG  0x026
#define PROGRESS_MSG        0x027
#define PROGRESS_END_MSG    0x028
#define PROGRESS_QUERY_MSG  0x029
#define WALK_PATH_MSG       0x02a

namespace PeerDrive {

class Watch;

class ConnectionHandler : public QObject
{
	Q_OBJECT

public:
	ConnectionHandler();
	~ConnectionHandler();

	int connect(const QString &hostName, quint16 port);
	void disconnect();

	struct Completion {
		volatile bool done;
		volatile int err;
		int msg;
		QByteArray *cnf;
	};

	int sendReq(int msg, Completion *completion, const QByteArray &req);
	int poll(Completion *completion);

signals:
	void pushSendQueue();
	void indication(const QByteArray &ind);

private slots:
	void sockReadyRead();
	void sockReadySend();
	void sockDisconnected();
	void sockError(QAbstractSocket::SocketError socketError);

private:
	void abortCompletions(int err);

	QTcpSocket socket;
	QQueue<QByteArray> sendQueue;
	QMap<quint32, Completion*> pendingCompletions;
	quint32 nextRef;
	QByteArray m_buf;

	QMutex mutex;
	QWaitCondition gotConfirmation;
};

class Connection : public QThread
{
	Q_OBJECT

public:
	int rpc(int msg, const QByteArray &req);
	int rpc(int msg, const QByteArray &req, QByteArray &cnf);
	static Connection *instance();

	template <typename R, typename C>
	static int defaultRPC(int msg, const R &req, C &cnf)
	{
		QByteArray rawReq;

		rawReq.resize(req.ByteSize());
		req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());

		QByteArray rawCnf;
		int err = Connection::instance()->rpc(msg, rawReq, rawCnf);
		if (err)
			return err;

		if (!cnf.ParseFromArray(rawCnf.constData(), rawCnf.size()))
			return ERR_EBADRPC;

		return 0;
	}

	template <typename R>
	static int defaultRPC(int msg, const R &req)
	{
		QByteArray rawReq;

		rawReq.resize(req.ByteSize());
		req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());

		return Connection::instance()->rpc(msg, rawReq);
	}

	int addWatch(Watch *watch, const DId &doc);
	int addWatch(Watch *watch, const RId &rev);
	void delWatch(Watch *watch, const DId &doc);
	void delWatch(Watch *watch, const RId &rev);

	unsigned int maxPacketSize() { return m_maxPacketSize; }

protected:
	int _rpc(int msg, const QByteArray &req, ConnectionHandler::Completion *completion);
	void run();


private slots:
	void dispatchIndication(const QByteArray &packet);
	void dispatchWatch(const QByteArray &packet);

private:
	Connection();
	~Connection();
	void sendInit();

	QMap<DId, QList<Watch*> > m_docWatches;
	QMap<RId, QList<Watch*> > m_revWatches;
	QMutex watchMutex;

	QMutex startupMutex;
	QWaitCondition startupDone;
	ConnectionHandler *handler;
	unsigned int m_maxPacketSize;

	static QMutex instanceMutex;
	volatile static Connection *m_instance;
};

}

#endif
