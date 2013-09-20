/*
 * This file is part of the PeerDrive Qt4 library.
 * Copyright (C) 2013  Jan Klötzke <jan DOT kloetzke AT freenet DOT de>
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

#include <QtDebug>
#include <QtEndian>
#include <QFile>
#include <QProcessEnvironment>
#include <stdexcept>

#include "peerdrive.h"
#include "peerdrive_internal.h"

#define FLAG_REQ	0
#define FLAG_CNF	1
#define FLAG_IND	2
#define FLAG_RSP	3

//#define TRACE_LEVEL 1

#ifdef TRACE_LEVEL
static const char *msg_names[] = {
	"ERROR_MSG",
	"INIT_MSG",
	"ENUM_MSG",
	"LOOKUP_DOC_MSG",
	"LOOKUP_REV_MSG",
	"STAT_MSG",
	"PEEK_MSG",
	"CREATE_MSG",
	"FORK_MSG",
	"UPDATE_MSG",
	"RESUME_MSG",
	"READ_MSG",
	"TRUNC_MSG",
	"WRITE_BUFFER_MSG",
	"WRITE_COMMIT_MSG",
	"GET_FLAGS_MSG",
	"SET_FLAGS_MSG",
	"GET_TYPE_MSG",
	"SET_TYPE_MSG",
	"GET_PARENTS_MSG",
	"MERGE_MSG",
	"REBASE_MSG",
	"COMMIT_MSG",
	"SUSPEND_MSG",
	"CLOSE_MSG",
	"WATCH_ADD_MSG",
	"WATCH_REM_MSG",
	"WATCH_PROGRESS_MSG",
	"FORGET_MSG",
	"DELETE_DOC_MSG",
	"DELETE_REV_MSG",
	"FORWARD_DOC_MSG",
	"REPLICATE_DOC_MSG",
	"REPLICATE_REV_MSG",
	"MOUNT_MSG",
	"UNMOUNT_MSG",
	"GET_PATH_MSG",
	"WATCH_MSG",
	"PROGRESS_START_MSG",
	"PROGRESS_MSG",
	"PROGRESS_END_MSG",
	"PROGRESS_QUERY_MSG",
	"WALK_PATH_MSG",
};
#endif

using namespace PeerDrive;

ConnectionHandler::ConnectionHandler()
	: QObject()
{
	nextRef = 0;

	QObject::connect(&socket, SIGNAL(readyRead()), this, SLOT(sockReadyRead()));
	QObject::connect(&socket, SIGNAL(disconnected()), this, SLOT(sockDisconnected()));
	QObject::connect(&socket, SIGNAL(error(QAbstractSocket::SocketError)),
		this, SLOT(sockError(QAbstractSocket::SocketError)));
	QObject::connect(this, SIGNAL(pushSendQueue()), this,
		SLOT(sockReadySend()), Qt::QueuedConnection);
}

ConnectionHandler::~ConnectionHandler()
{
	while (socket.flush())
		socket.waitForBytesWritten(1000);
	socket.disconnectFromHost();

	abortCompletions(ErrConnReset);
}

int ConnectionHandler::connect(const QString &hostName, quint16 port)
{
	socket.connectToHost(hostName, port);
	if (!socket.waitForConnected())
		return ERR_ENODEV;

	return 0;
}

void ConnectionHandler::disconnect()
{
	socket.disconnectFromHost();
}

Error ConnectionHandler::sendReq(int msg, Completion *completion, const QByteArray &req)
{
	completion->done = false;
	completion->err = ErrNoError;

	QMutexLocker locker(&mutex);

	if (socket.state() != QAbstractSocket::ConnectedState)
		return ErrConnReset;

	quint32 ref = nextRef++;
	pendingCompletions.insert(ref, completion);

	uchar header[8];
	qToBigEndian((quint16)(6 + req.size()), header);
	qToBigEndian((quint32)ref, header + 2);
	qToBigEndian((quint16)((msg << 4) | FLAG_REQ), header + 6);

	QByteArray fullReq((const char *)header, 8);
	fullReq.append(req);

	sendQueue.enqueue(fullReq);

	locker.unlock();
	emit pushSendQueue();

	return ErrNoError;
}

Error ConnectionHandler::poll(Completion *completion)
{
	QMutexLocker locker(&mutex);

	// FIXME: use a pool of QWaitCondition's or pre-allocated Completion's
	// with their own semaphore
	while (!completion->done)
		gotConfirmation.wait(&mutex);

	return completion->err;
}

void ConnectionHandler::sockReadySend()
{
	QMutexLocker locker(&mutex);

	while (!sendQueue.isEmpty()) {
		QByteArray req = sendQueue.dequeue();
		locker.unlock();
		if (socket.write(req) != req.size()) {
			socket.disconnectFromHost();
			abortCompletions(ErrConnReset);
			return;
		}
		locker.relock();
	}
}

void ConnectionHandler::sockReadyRead()
{
	m_buf.append(socket.readAll());
	while (m_buf.size() > 2) {
		quint16 expect = qFromBigEndian<quint16>((const uchar *)m_buf.constData());
		if (expect+2 > m_buf.size())
			break;

		quint32 ref = qFromBigEndian<quint32>((const uchar *)m_buf.constData() + 2);
		quint16 msg = qFromBigEndian<quint16>((const uchar *)m_buf.constData() + 6);
		int type = msg & 3;
		int len = expect-6;
		msg >>= 4;

		QMutexLocker locker(&mutex);

		if (type == FLAG_CNF && pendingCompletions.contains(ref)) {
			Completion *c = pendingCompletions.take(ref);
			c->done = true;
			c->msg = msg;
			*(c->cnf) = m_buf.mid(8, len);
			gotConfirmation.wakeAll();
		} else if (type == FLAG_IND) {
			// dispatch with message header
			emit indication(m_buf.mid(6, len+2));
		}

		m_buf.remove(0, expect+2);
	}
}

void ConnectionHandler::sockDisconnected()
{
	abortCompletions(ErrConnReset);
}

void ConnectionHandler::sockError(QAbstractSocket::SocketError /*socketError*/)
{
	socket.disconnectFromHost();
	abortCompletions(ErrConnReset); // TODO: use socketError
}

void ConnectionHandler::abortCompletions(Error err)
{
	QMutexLocker locker(&mutex);

	foreach (Completion *c, pendingCompletions) {
		c->done = true;
		c->err = err;
	}

	gotConfirmation.wakeAll();
}

/****************************************************************************/

Connection* volatile Connection::m_instance = NULL;
QMutex Connection::instanceMutex;

Connection::Connection()
	: QThread(),
	  watchMutex(QMutex::Recursive),
	  progressMutex(QMutex::Recursive),
	  m_maxPacketSize(16384)
{
	QMutexLocker locker(&startupMutex);

	start();
	startupDone.wait(&startupMutex);

	sendInit();

	Connection::m_instance = this;
}

Connection::~Connection()
{
	Connection::m_instance = NULL;

	exit();
	wait();
}

Connection* Connection::instance()
{
	// fast path: instance already created
	if (Connection::m_instance)
		return Connection::m_instance;

	// slow path: create instance
	QMutexLocker l(&instanceMutex);

	if (Connection::m_instance)
		return Connection::m_instance;

	return new Connection();
}

void Connection::run()
{
	handler = new ConnectionHandler();
	QObject::connect(handler, SIGNAL(indication(QByteArray)), this,
		SLOT(dispatchIndication(QByteArray)), Qt::QueuedConnection);

	// first look into the environment
	QString address = QProcessEnvironment::systemEnvironment().value("PEERDRIVE");

	// then look for per-user daemon
	if (address.isEmpty()) {
		// FIXME: add Windows
		QString path = QString("/tmp/peerdrive-%1/server.info").arg(
			QProcessEnvironment::systemEnvironment().value("USER"));

		QFile file(path);
		if (file.open(QIODevice::ReadOnly | QIODevice::Text))
			address = file.readLine().trimmed();
	}

	// look for system daemon as last resort
	if (address.isEmpty()) {
		// FIXME: add Windows
		QFile file("/var/run/peerdrive/server.info");
		if (file.open(QIODevice::ReadOnly | QIODevice::Text))
			address = file.readLine().trimmed();
	}

	// do we have an address?
	if (address.isEmpty()) {
		qDebug() << "::PeerDrive::Connection: don't know where to find a server";
		goto run;
	}

	if (!address.startsWith("tcp://")) {
		qDebug() << "::PeerDrive::Connection: unknown address scheme: " << address;
		goto run;
	}

	{
		QStringList urlParts = address.mid(6).split("/");
		if (urlParts.size() == 2) {
			QStringList addrParts = urlParts.at(0).split(":");
			if (addrParts.size() == 2) {
				QString hostName = addrParts.at(0);
				quint16 port = addrParts.at(1).toInt();

				m_cookie = urlParts.at(1);

				int err = handler->connect(hostName, port);
				if (err)
					qDebug() << "::PeerDrive::Connection: connection failed:" << err;
			} else {
				qDebug() << "::PeerDrive::Connection: malformed host: "
				         << urlParts.at(0);
			}
		} else {
			qDebug() << "::PeerDrive::Connection: malformed url: " << address;
		}
	}

run:
	startupDone.wakeAll();
	exec();
	delete handler;
}

void Connection::sendInit()
{
	InitReq req;
	InitCnf cnf;
	QByteArray rawReq, rawCnf;

	req.set_major(1);
	req.set_minor(0);
	QByteArray cookie = QByteArray::fromHex(m_cookie.toLatin1());
	req.set_cookie(cookie.constData(), cookie.size());

	rawReq.resize(req.ByteSize());
	req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());

	Error err = rpc(INIT_MSG, rawReq, rawCnf);
	if (err) {
		qDebug() << "::PeerDrive::Connection: INIT_MSG failed:" << err;
		goto failed;
	}

	if (!cnf.ParseFromArray(rawCnf.constData(), rawCnf.size())) {
		err = ErrBadRPC;
		goto failed;
	}

	if (cnf.major() != 1 || cnf.minor() != 0) {
		qDebug() << "::PeerDrive::Connection: unsupported server version:" <<
			cnf.major() << ":" << cnf.minor();
		err = ErrRPCMISMATCH;
		goto failed;
	}

	m_maxPacketSize = cnf.max_packet_size();
	return;

failed:
	handler->disconnect();
}

Error Connection::rpc(int msg, const QByteArray &req, QByteArray &cnf)
{
	cnf.clear();

	ConnectionHandler::Completion completion;
	completion.cnf = &cnf;

	return _rpc(msg, req, &completion);
}

Error Connection::rpc(int msg, const QByteArray &req)
{
	QByteArray cnf;

	ConnectionHandler::Completion completion;
	completion.cnf = &cnf;

	return _rpc(msg, req, &completion);
}

Error Connection::_rpc(int msg, const QByteArray &req, ConnectionHandler::Completion *completion)
{
#if TRACE_LEVEL >= 1
	qDebug() << msg_names[msg];
#endif

	Error ret = handler->sendReq(msg, completion, req);
	if (ret)
		return ret;

	ret = handler->poll(completion);
	if (ret)
		return ret;

	if (completion->msg == msg) {
		return ErrNoError;
	} else if (completion->msg == ERROR_MSG) {
		ErrorCnf errCnf;
		if (!errCnf.ParseFromArray(completion->cnf->constData(),
		                           completion->cnf->size()))
			return ErrBadRPC;
		return static_cast<Error>(errCnf.error() + 1);
	} else
		return ErrBadRPC;
}

void Connection::dispatchIndication(const QByteArray &buf)
{
	quint16 msg = qFromBigEndian<quint16>((const uchar *)buf.constData()) >> 4;
	QByteArray packet = buf.mid(2);

	switch (msg) {
		case WATCH_MSG:
			dispatchWatch(packet);
			break;
		case PROGRESS_START_MSG:
		{
			ProgressStartInd ind;
			if (ind.ParseFromArray(packet.constData(), packet.size())) {
				QMutexLocker lock(&progressMutex);
				dispatchProgressStart(ind, true);
			}
			break;
		}
		case PROGRESS_MSG:
		{
			ProgressInd ind;
			if (ind.ParseFromArray(packet.constData(), packet.size())) {
				QMutexLocker lock(&progressMutex);
				dispatchProgress(ind, true);
			}
			break;
		}
		case PROGRESS_END_MSG:
			dispatchProgressEnd(packet);
			break;
	}
}

void Connection::dispatchWatch(const QByteArray &packet)
{
	QMutexLocker lock(&watchMutex);

	WatchInd ind;
	if (!ind.ParseFromArray(packet.constData(), packet.size()))
		return;

	switch (ind.type()) {
		case WatchInd::DOC:
		{
			DId doc(ind.element());
			Link item(DId(ind.store()), doc);
			if (m_docWatches.contains(doc))
				foreach (LinkWatcher *w, m_docWatches.value(doc))
					w->dispatch(item, ind.event());
			break;
		}
		case WatchInd::REV:
		{
			RId rev(ind.element());
			Link item(DId(ind.store()), rev);
			if (m_revWatches.contains(rev))
				foreach (LinkWatcher *w, m_revWatches.value(rev))
					w->dispatch(item, ind.event());
			break;
		}
	}
}

void Connection::dispatchProgressStart(const ProgressStartInd &ind, bool dispatch)
{
	Progress *p = new Progress;
	p->src = DId(ind.source());
	p->dst = DId(ind.dest());
	p->state = ProgressWatcher::StateRunning;
	p->error = ErrNoError;
	p->progress = 0;

	switch (ind.type()) {
		case ProgressStartInd::SYNC:
			p->type = ProgressWatcher::Synchronization;
			break;
		case ProgressStartInd::REP_DOC:
			p->type = ProgressWatcher::Replication;
			p->item = Link(DId(ind.source()), DId(ind.item()));
			break;
		case ProgressStartInd::REP_REV:
			p->type = ProgressWatcher::Replication;
			p->item = Link(DId(ind.source()), RId(ind.item()));
			break;
	}

	unsigned int tag = ind.tag();
	m_progressItems[tag] = p;

	if (dispatch)
		foreach(ProgressWatcher *w, m_progressWatches)
			emit w->started(tag);
}

void Connection::dispatchProgress(const ProgressInd &ind, bool dispatch)
{
	unsigned int tag = ind.tag();
	if (!m_progressItems.contains(tag))
		return;

	Progress *p = m_progressItems.value(tag);
	p->progress = ind.progress();
	p->state = static_cast<ProgressWatcher::State>(ind.state());
	if (ind.has_err_code())
		p->error = static_cast<Error>(ind.err_code() + 1);

	if (ind.has_err_doc()) {
		if (ind.has_err_rev())
			p->errorItem = Link(p->src, DId(ind.err_doc()), RId(ind.err_rev()));
		else
			p->errorItem = Link(p->src, DId(ind.err_doc()));
	} else if (ind.has_err_rev()) {
		p->errorItem = Link(p->src, RId(ind.err_rev()));
	}

	if (dispatch)
		foreach(ProgressWatcher *w, m_progressWatches)
			emit w->changed(tag);
}

void Connection::dispatchProgressEnd(const QByteArray &packet)
{
	ProgressEndInd ind;
	if (!ind.ParseFromArray(packet.constData(), packet.size()))
		return;

	unsigned int tag = ind.tag();

	QMutexLocker lock(&progressMutex);
	if (!m_progressItems.contains(tag))
		return;

	delete m_progressItems.take(tag);

	foreach(ProgressWatcher *w, m_progressWatches)
		emit w->finished(tag);
}

void Connection::addProgressWatch(ProgressWatcher *watch)
{
	QMutexLocker lock(&progressMutex);

	/*
	 * If we're the first then we have to query all ongoing operations and
	 * request to be notified in the future.
	 */
	if (m_progressWatches.isEmpty()) {
		QByteArray rawReq, rawCnf;

		// enable notifications
		WatchProgressReq req;
		req.set_enable(true);
		rawReq.resize(req.ByteSize());
		req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());
		Error err = rpc(WATCH_PROGRESS_MSG, rawReq);
		if (err) {
			qDebug() << "::PeerDrive::Connection::addProgressWatch: enable failed:" << err;
			return;
		}

		// query ongoing operations
		rawReq.resize(0);
		err = rpc(PROGRESS_QUERY_MSG, rawReq, rawCnf);
		if (err) {
			qDebug() << "::PeerDrive::Connection::addProgressWatch: query failed:" << err;
			return;
		}

		ProgressQueryCnf cnf;
		if (!cnf.ParseFromArray(rawCnf.constData(), rawCnf.size()))
			return;

		for (int i = 0; i < cnf.items_size(); i++) {
			const ProgressQueryCnf_Item &item = cnf.items(i);
			dispatchProgressStart(item.item(), false);
			dispatchProgress(item.state(), false);
		}
	}

	m_progressWatches.append(watch);
}

void Connection::delProgressWatch(ProgressWatcher *watch)
{
	QMutexLocker lock(&progressMutex);
	m_progressWatches.removeOne(watch);

	if (m_progressWatches.isEmpty()) {
		// If we were the last free everything...
		QMap<unsigned int, Progress*>::const_iterator i = m_progressItems.constBegin();
		while (i != m_progressItems.constEnd()) {
			delete i.value();
			++i;
		}
		m_progressItems.clear();

		// disable notification
		WatchProgressReq req;
		QByteArray rawReq;
		req.set_enable(false);
		rawReq.resize(req.ByteSize());
		req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());
		Error err = rpc(WATCH_PROGRESS_MSG, rawReq);
		if (err)
			qDebug() << "::PeerDrive::Connection::delProgressWatch failed:" << err;
	}
}

Connection::Progress* Connection::findProgress(unsigned int tag) const
{
	return m_progressItems.value(tag, NULL);
}

QList<unsigned int> Connection::progressTags() const
{
	return m_progressItems.keys();
}

Error Connection::addWatch(LinkWatcher *watch, const DId &doc)
{
	QMutexLocker lock(&watchMutex);
	Error err = ErrNoError;

	if (m_docWatches.contains(doc)) {
		m_docWatches[doc].append(watch);
	} else {
		WatchAddReq req;
		QByteArray rawReq;

		req.set_type(WatchAddReq::DOC);
		req.set_element(doc.toStdString());

		rawReq.resize(req.ByteSize());
		req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());

		err = rpc(WATCH_ADD_MSG, rawReq);
		if (!err)
			m_docWatches[doc].append(watch);
		else
			qDebug() << "::PeerDrive::Connection: addWatch failed:" << err;
	}

	return err;
}

Error Connection::addWatch(LinkWatcher *watch, const RId &rev)
{
	QMutexLocker lock(&watchMutex);
	Error err = ErrNoError;

	if (m_revWatches.contains(rev)) {
		m_revWatches[rev].append(watch);
	} else {
		WatchAddReq req;
		QByteArray rawReq;

		req.set_type(WatchAddReq::REV);
		req.set_element(rev.toStdString());

		rawReq.resize(req.ByteSize());
		req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());

		err = rpc(WATCH_ADD_MSG, rawReq);
		if (!err)
			m_revWatches[rev].append(watch);
		else
			qDebug() << "::PeerDrive::Connection: addWatch failed:" << err;
	}

	return err;
}

void Connection::delWatch(LinkWatcher *watch, const DId &doc)
{
	QMutexLocker lock(&watchMutex);
	QList<LinkWatcher*> &watches = m_docWatches[doc];
	watches.removeOne(watch);

	if (watches.isEmpty()) {
		WatchRemReq req;
		QByteArray rawReq;

		req.set_type(WatchRemReq::DOC);
		req.set_element(doc.toStdString());

		rawReq.resize(req.ByteSize());
		req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());

		Error err = rpc(WATCH_REM_MSG, rawReq);
		if (err)
			qDebug() << "::PeerDrive::Connection: delWatch failed:" << err;
		m_docWatches.remove(doc);
	}
}

void Connection::delWatch(LinkWatcher *watch, const RId &rev)
{
	QMutexLocker lock(&watchMutex);
	QList<LinkWatcher*> &watches = m_revWatches[rev];
	watches.removeOne(watch);

	if (watches.isEmpty()) {
		WatchRemReq req;
		QByteArray rawReq;

		req.set_type(WatchRemReq::REV);
		req.set_element(rev.toStdString());

		rawReq.resize(req.ByteSize());
		req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());

		Error err = rpc(WATCH_REM_MSG, rawReq);
		if (err)
			qDebug() << "::PeerDrive::Connection: delWatch failed:" << err;
		m_revWatches.remove(rev);
	}
}

/****************************************************************************/

uint qHash(const PeerDrive::DId &id)
{
	return qHash(id.toByteArray());
}

uint qHash(const PeerDrive::RId &id)
{
	return qHash(id.toByteArray());
}

uint qHash(const PeerDrive::PId &id)
{
	return qHash(id.toByteArray());
}

/****************************************************************************/

Link::Link()
{
	m_state = INVALID;
}

Link::Link(const QString &uri)
{
	m_state = INVALID;
	QStringList frags = uri.split(":");
	if (frags.size() != 3)
		return;
	// TODO: check hex characters

	QByteArray store = QByteArray::fromHex(frags.at(1).toLatin1());
	QByteArray item = QByteArray::fromHex(frags.at(2).toLatin1());

	if (frags.at(0) == "doc") {
		m_state = DOC_HEAD;
		m_store = DId(store);
		m_doc = DId(item);
	} else if (frags.at(0) == "rev") {
		m_state = REV;
		m_store = DId(store);
		m_rev = RId(item);
	}
}

Link::Link(const DId &store, const RId &rev)
{
	m_state = REV;
	m_store = store;
	m_rev = rev;
}

Link::Link(const DId &store, const DId &doc, bool doUpdate)
{
	m_state = DOC_HEAD;
	m_store = store;
	m_doc = doc;

	if (doUpdate)
		update();
}

Link::Link(const DId &store, const DId &doc, const RId &rev, bool isPreRev)
{
	if (isPreRev)
		m_state = DOC_PRE_REV;
	else
		m_state = DOC_HEAD;
	m_store = store;
	m_doc = doc;
	m_rev = rev;
}

bool Link::update()
{
	switch (m_state) {
		case INVALID:
			return false;

		case DOC_HEAD:
		{
			DocInfo info(m_doc, m_store);
			if (!info.exists()) {
				m_rev = RId();
				return false;
			} else {
				m_rev = info.head(m_store).rev();
				return true;
			}
		}

		case DOC_PRE_REV:
		case REV:
			return true;
	}

	return false;
}

bool Link::update(DId &newStore)
{
	m_store = newStore;
	return update();
}

bool Link::isValid() const
{
	return m_state == DOC_HEAD ||
	       m_state == DOC_PRE_REV ||
	       m_state == REV;
}

bool Link::isRevLink() const
{
	return m_state == REV;
}

bool Link::isDocLink() const
{
	return m_state == DOC_HEAD ||
	       m_state == DOC_PRE_REV;
}

bool Link::isDocHeadLink() const
{
	return m_state == DOC_HEAD;
}

bool Link::isDocPreRevLink() const
{
	return m_state == DOC_PRE_REV;
}

DId Link::store() const
{
	return m_store;
}

RId Link::rev() const
{
	return m_rev;
}

DId Link::doc() const
{
	return m_doc;
}

QString Link::getOsPath() const
{
	GetPathReq req;
	GetPathCnf cnf;

	switch (m_state) {
		case INVALID:
			return QString();
		case DOC_HEAD:
			req.set_object(m_doc.toStdString());
			req.set_is_rev(false);
			break;
		case DOC_PRE_REV:
		case REV:
			req.set_object(m_rev.toStdString());
			req.set_is_rev(true);
			break;
	}
	req.set_store(m_store.toStdString());

	if (Connection::defaultRPC<GetPathReq, GetPathCnf>(GET_PATH_MSG, req, cnf))
		return QString();

	return QString::fromUtf8(cnf.path().c_str());
}

QString Link::uri() const
{
	switch (m_state) {
		case DOC_HEAD:
			return "doc:" + m_store.toByteArray().toHex() + ":" +
				m_doc.toByteArray().toHex();
		case REV:
			return "rev:" + m_store.toByteArray().toHex() + ":" +
				m_rev.toByteArray().toHex();
		default:
			return QString();
	}
}

bool Link::operator== (const Link &other) const
{
	if (m_state != other.m_state || m_store != other.m_store)
		return false;

	switch (m_state) {
		case INVALID:
			return true;
		case DOC_PRE_REV:
			return m_rev == other.m_rev && m_doc == other.m_doc;
		case DOC_HEAD:
			return m_doc == other.m_doc;
		case REV:
			return m_rev == other.m_rev;
	}

	// not reached; make compiler happy
	return false;
}

bool Link::operator< (const Link &other) const
{
	if (m_state != other.m_state)
		return m_state < other.m_state;

	if (m_store != other.m_store)
		return m_store < other.m_store;

	switch (m_state) {
		case DOC_PRE_REV:
			if (m_rev != other.m_rev)
				return m_rev < other.m_rev;
		case DOC_HEAD:
			return m_doc < other.m_doc;
		case REV:
			return m_rev < other.m_rev;
		default:
			return false;
	}
}

uint qHash(const PeerDrive::Link &link)
{
	uint h = qHash(link.m_store.toByteArray()) ^ link.m_state;

	switch (link.m_state) {
		case Link::DOC_PRE_REV:
			h ^= qHash(link.m_rev.toByteArray());
		case Link::DOC_HEAD:
			h ^= qHash(link.m_doc.toByteArray());
			break;
		case Link::REV:
			h ^= qHash(link.m_rev.toByteArray());
			break;
		default:
			break;
	}

	return h;
}

/****************************************************************************/

const Link LinkWatcher::rootDoc = Link(DId(QByteArray(16, 0)), DId(QByteArray(16, 0)), RId());

LinkWatcher::LinkWatcher(QObject *parent)
	: QObject(parent)
{
}

LinkWatcher::~LinkWatcher()
{
	foreach (const Link &item, watchedLinks)
		removeWatch(item);
}

void LinkWatcher::addWatch(const Link &item)
{
	if (!item.isValid())
		return;
	if (watchedLinks.contains(item))
		return;

	bool added;
	if (item.isDocLink())
		added = Connection::instance()->addWatch(this, item.doc()) == 0;
	else
		added = Connection::instance()->addWatch(this, item.rev()) == 0;

	if (added)
		watchedLinks.append(item);
}

void LinkWatcher::removeWatch(const Link &item)
{
	if (!item.isValid())
		return;
	if (!watchedLinks.contains(item))
		return;

	watchedLinks.removeAll(item);
	if (item.isDocLink())
		Connection::instance()->delWatch(this, item.doc());
	else
		Connection::instance()->delWatch(this, item.rev());
}

void LinkWatcher::dispatch(const Link &item, int event)
{
	switch (event) {
		case WatchInd::MODIFIED:
			emit modified(item);
			break;
		case WatchInd::APPEARED:
			emit appeared(item);
			emit distributionChanged(item, APPEARED);
			break;
		case WatchInd::REPLICATED:
			emit replicated(item);
			emit distributionChanged(item, REPLICATED);
			break;
		case WatchInd::DIMINISHED:
			emit diminished(item);
			emit distributionChanged(item, DIMINISHED);
			break;
		case WatchInd::DISAPPEARED:
			emit disappeared(item);
			emit distributionChanged(item, DISAPPEARED);
			break;
	}
}

/****************************************************************************/

ProgressWatcher::ProgressWatcher(QObject *parent)
	: QObject(parent)
{
	Connection::instance()->addProgressWatch(this);
}

ProgressWatcher::~ProgressWatcher()
{
	Connection::instance()->delProgressWatch(this);
}

Link ProgressWatcher::source(unsigned int tag) const
{
	Connection::Progress *p = Connection::instance()->findProgress(tag);
	return p ? Link(p->src, p->src) : Link();
}

Link ProgressWatcher::destination(unsigned int tag) const
{
	Connection::Progress *p = Connection::instance()->findProgress(tag);
	return p ? Link(p->dst, p->dst) : Link();
}

Link ProgressWatcher::item(unsigned int tag) const
{
	Connection::Progress *p = Connection::instance()->findProgress(tag);
	return p ? p->item : Link();
}

ProgressWatcher::Type ProgressWatcher::type(unsigned int tag) const
{
	Connection::Progress *p = Connection::instance()->findProgress(tag);
	return p ? p->type : Synchronization;
}

ProgressWatcher::State ProgressWatcher::state(unsigned int tag) const
{
	Connection::Progress *p = Connection::instance()->findProgress(tag);
	return p ? p->state : StateError;
}

Error ProgressWatcher::error(unsigned int tag) const
{
	Connection::Progress *p = Connection::instance()->findProgress(tag);
	return p ? p->error : ErrNoError;
}

Link ProgressWatcher::errorItem(unsigned int tag) const
{
	Connection::Progress *p = Connection::instance()->findProgress(tag);
	return p ? p->errorItem : Link();
}

int ProgressWatcher::progress(unsigned int tag) const
{
	Connection::Progress *p = Connection::instance()->findProgress(tag);
	return p ? p->progress : 0;
}

QList<unsigned int> ProgressWatcher::tags() const
{
	return Connection::instance()->progressTags();
}

int ProgressWatcher::pause(unsigned int tag)
{
	ProgressEndReq req;

	req.set_tag(tag);
	req.set_pause(true);

	return Connection::defaultRPC<ProgressEndReq>(PROGRESS_END_MSG, req);
}

int ProgressWatcher::resume(unsigned int tag, bool skip)
{
	ProgressStartReq req;

	req.set_tag(tag);
	req.set_skip(skip);

	return Connection::defaultRPC<ProgressStartReq>(PROGRESS_START_MSG, req);
}

int ProgressWatcher::stop(unsigned int tag)
{
	ProgressEndReq req;

	req.set_tag(tag);
	req.set_pause(false);

	return Connection::defaultRPC<ProgressEndReq>(PROGRESS_END_MSG, req);
}

/****************************************************************************/

Mounts::Mounts()
	: m_sysStore(NULL)
{
	EnumCnf cnf;
	QByteArray rawReq, rawCnf;
	Error err = Connection::instance()->rpc(ENUM_MSG, rawReq, rawCnf);
	if (err) {
		qDebug() << "::PeerDrive::Mounts: EnumReq failed:" << err << "\n";
		return;
	}

	if (!cnf.ParseFromArray(rawCnf.constData(), rawCnf.size()))
		return;

	m_sysStore = parse(cnf.sys_store());
	m_sysStore->isSystem = true;

	for (int i = 0; i < cnf.stores_size(); i++)
		m_stores.append(parse(cnf.stores(i)));
}

Mounts::~Mounts()
{
	if (m_sysStore)
		delete m_sysStore;

	foreach(Store *s, m_stores)
		delete s;
}

Mounts::Store *Mounts::parse(const EnumCnf_Store &pb)
{
	Store *s = new Store;

	s->sid     = pb.sid();
	s->src     = QString::fromUtf8(pb.src().c_str());
	s->type    = QString::fromUtf8(pb.type().c_str());
	s->label   = QString::fromUtf8(pb.label().c_str());
	s->options = QString::fromUtf8(pb.options().c_str());

	return s;
}

Mounts::Store* Mounts::sysStore() const
{
	return m_sysStore;
}

QList<Mounts::Store*> Mounts::regularStores() const
{
	return m_stores;
}

QList<Mounts::Store*> Mounts::allStores() const
{
	QList<Mounts::Store*> all(m_stores);
	if (m_sysStore)
		all.append(m_sysStore);
	return all;
}

Mounts::Store* Mounts::fromLabel(const QString &label) const
{
	if (label == m_sysStore->label)
		return m_sysStore;

	foreach(Store* s, m_stores) {
		if (s->label == label)
			return s;
	}

	return NULL;
}

Mounts::Store* Mounts::fromSId(const DId &sid) const
{
	if (sid == m_sysStore->sid)
		return m_sysStore;

	foreach(Store* s, m_stores) {
		if (s->sid == sid)
			return s;
	}

	return NULL;
}

DId Mounts::mount(Error *result, const QString &src, const QString &label,
                  const QString &type, const QString &options,
                  const QString &credentials)
{
	MountReq req;
	MountCnf cnf;

	req.set_src(src.toUtf8().constData());
	req.set_label(label.toUtf8().constData());
	req.set_type(type.toUtf8().constData());
	if (!options.isEmpty())
		req.set_options(options.toUtf8().constData());
	if (!credentials.isEmpty())
		req.set_credentials(credentials.toUtf8().constData());

	Error err = Connection::defaultRPC<MountReq, MountCnf>(MOUNT_MSG, req, cnf);
	if (result)
		*result = err;
	if (err)
		return DId();

	return DId(cnf.sid());
}

Error Mounts::unmount(const DId &sid)
{
	UnmountReq req;

	req.set_sid(sid.toStdString());
	return Connection::defaultRPC<UnmountReq>(UNMOUNT_MSG, req);
}

/****************************************************************************/

RevInfo::RevInfo()
	: m_exists(false)
	, m_error(ErrNoError)
{
}

RevInfo::RevInfo(const RId &rid)
{
	fetch(rid, NULL);
}

RevInfo::RevInfo(const RId &rid, const QList<DId> &stores)
{
	fetch(rid, &stores);
}

void RevInfo::fetch(const RId &rid, const QList<DId> *stores)
{
	StatReq req;
	StatCnf cnf;

	m_exists = false;
	m_flags = 0;

	req.set_rev(rid.toStdString());
	if (stores) {
		QList<DId>::const_iterator i;
		for (i = stores->constBegin(); i != stores->constEnd(); i++)
			req.add_stores((*i).toStdString());
	}

	m_error = Connection::defaultRPC<StatReq, StatCnf>(STAT_MSG, req, cnf);
	if (m_error)
		return;

	m_flags = cnf.flags();
	m_dataSize = cnf.data().size();
	m_dataHash = PId(cnf.data().hash());
	for (int j = 0; j < cnf.attachments_size(); j++) {
		const StatCnf_Attachment & a = cnf.attachments(j);
		QString name(a.name().c_str());
		m_attachmentSizes[name] = a.size();
		m_attachmentHashes[name] = PId(a.hash());
	}
	for (int j = 0; j < cnf.parents_size(); j++) {
		RId rid(cnf.parents(j));
		m_parents.append(rid);
	}
	m_mtime.setTime_t(cnf.mtime() / 1000000); // FIXME: sub-second resolution
	m_type = QString::fromUtf8(cnf.type_code().c_str());
	m_creator = QString::fromUtf8(cnf.creator_code().c_str());
	m_comment = QString::fromUtf8(cnf.comment().c_str());
	m_exists = true;
}

bool RevInfo::exists() const
{
	return m_exists;
}

Error RevInfo::error() const
{
	return m_error;
}

int RevInfo::flags() const
{
	return m_flags;
}

quint64 RevInfo::size() const
{
	quint64 sum = m_dataSize;
	for (QMap<QString, quint64>::const_iterator i = m_attachmentSizes.constBegin();
	     i != m_attachmentSizes.constEnd(); ++i)
		sum += i.value();

	return sum;
}

quint64 RevInfo::dataSize() const
{
	return m_dataSize;
}

quint64 RevInfo::attachmentSize(const QString &attachment) const
{
	return m_attachmentSizes[attachment];
}

PId RevInfo::dataHash() const
{
	return m_dataHash;
}

PId RevInfo::attachmentHash(const QString &attachment) const
{
	return m_attachmentHashes[attachment];
}

QList<QString> RevInfo::attachments() const
{
	return m_attachmentSizes.keys();
}

QList<RId> RevInfo::parents() const
{
	return m_parents;
}

QDateTime RevInfo::mtime() const
{
	return m_mtime;
}

QString RevInfo::type() const
{
	return m_type;
}

QString RevInfo::creator() const
{
	return m_creator;
}

QString RevInfo::comment() const
{
	return m_comment;
}

/****************************************************************************/

DocInfo::DocInfo()
	: m_error(ErrNoError)
{
}

DocInfo::DocInfo(const DId &doc)
{
	fetch(doc);
}

DocInfo::DocInfo(const DId &doc, const DId &store)
{
	QList<DId> stores;

	stores << store;
	fetch(doc, stores);
}

DocInfo::DocInfo(const DId &doc, const QList<DId> &stores)
{
	fetch(doc, stores);
}

void DocInfo::fetch(const DId &doc, const QList<DId> &stores)
{
	LookupDocReq req;
	LookupDocCnf cnf;

	req.set_doc(doc.toStdString());
	foreach(DId store, stores)
		req.add_stores(store.toStdString());
	m_error = Connection::defaultRPC<LookupDocReq, LookupDocCnf>(
		LOOKUP_DOC_MSG, req, cnf);
	if (m_error)
		return;

	for (int i = 0; i < cnf.revs_size(); i++) {
		const LookupDocCnf_RevMap & map = cnf.revs(i);

		RId rev(map.rid());

		for (int j = 0; j < map.stores_size(); j++) {
			DId store(map.stores(j));
			m_stores[store].append(Link(store, doc, rev));
			m_revs[rev].append(store);
		}
	}

	for (int i = 0; i < cnf.pre_revs_size(); i++) {
		const LookupDocCnf_RevMap & map = cnf.pre_revs(i);

		RId preRev(map.rid());

		for (int j = 0; j < map.stores_size(); j++) {
			DId store(map.stores(j));
			m_stores[store].append(Link(store, doc, preRev, true));
			m_preRevs[preRev].append(store);
		}
	}
}

bool DocInfo::exists() const
{
	return !m_stores.empty();
}

bool DocInfo::exists(const DId &store) const
{
	return m_stores.contains(store);
}

Error DocInfo::error() const
{
	return m_error;
}

QList<DId> DocInfo::stores() const
{
	return m_stores.keys();
}

QList<DId> DocInfo::stores(const RId &rev) const
{
	QList<DId> tmp;

	if (m_revs.contains(rev))
		tmp.append(m_revs.value(rev));

	if (m_preRevs.contains(rev))
		tmp.append(m_preRevs.value(rev));

	return tmp;
}

QList<Link> DocInfo::heads() const
{
	QList<Link> tmp;

	foreach(QList<Link> links, m_stores)
		foreach(Link l, links)
			if (l.isDocHeadLink())
				tmp.append(l);

	return tmp;
}

Link DocInfo::head(const DId &store) const
{
	if (m_stores.contains(store)) {
		foreach(Link l, m_stores.value(store))
			if (l.isDocHeadLink())
				return l;
	}

	return Link();
}

QList<Link> DocInfo::preliminaryHeads() const
{
	QList<Link> tmp;

	foreach(RId preRev, m_preRevs.keys()) {
		foreach(DId store, m_preRevs.value(preRev))
			tmp.append(Link(store, m_doc, preRev, true));
	}

	return tmp;
}

QList<Link> DocInfo::preliminaryHeads(const DId &store) const
{
	QList<Link> tmp;

	foreach(Link l, m_stores.value(store, QList<Link>()))
		if (l.isDocPreRevLink())
			tmp.append(l);

	return tmp;
}

/****************************************************************************/

LinkInfo::LinkInfo()
	: m_exists(false)
	, m_error(ErrNoError)
{
}

LinkInfo::LinkInfo(const RId &rev)
{
	fetch(rev, NULL);
}

LinkInfo::LinkInfo(const RId &rev, const QList<DId> &stores)
{
	fetch(rev, &stores);
}

void LinkInfo::fetch(const RId &rev, const QList<DId> *stores)
{
	GetLinksReq req;
	GetLinksCnf cnf;

	m_exists = false;
	m_docLinks.clear();
	m_revLinks.clear();

	req.set_rev(rev.toStdString());
	if (stores) {
		QList<DId>::const_iterator i;
		for (i = stores->constBegin(); i != stores->constEnd(); ++i)
			req.add_stores((*i).toStdString());
	}

	m_error = Connection::defaultRPC<GetLinksReq, GetLinksCnf>(GET_LINKS_MSG, req, cnf);
	if (m_error)
		return;

	m_docLinks.reserve(cnf.doc_links_size());
	for (int i = 0; i < cnf.doc_links_size(); i++)
		m_docLinks.append(DId(cnf.doc_links(i)));

	m_revLinks.reserve(cnf.rev_links_size());
	for (int i = 0; i < cnf.rev_links_size(); i++)
		m_revLinks.append(RId(cnf.rev_links(i)));

	m_exists = true;
}

Error LinkInfo::error() const
{
	return m_error;
}

QList<DId> LinkInfo::docLinks() const
{
	return m_docLinks;
}

QList<RId> LinkInfo::revLinks() const
{
	return m_revLinks;
}

/****************************************************************************/


Document::Document()
{
	m_open = false;
	m_error = ErrBadF;
}

Document::Document(const Link &link)
{
	m_open = false;
	m_error = ErrBadF;
	m_link = link;
}

Document::~Document()
{
	close();
}

//Document *Document::fork(const Link &parent);
//Document *Document::create(const DId &store);

const Link Document::link() const
{
	return m_link;
}

void Document::setLink(const Link &link)
{
	close();
	m_link = link;
}

Error Document::error() const
{
	return m_error;
}

bool Document::peek()
{
	close();
	if (!m_link.isValid()) {
		m_error = ErrBadF;
		return false;
	}

	PeekReq req;
	PeekCnf cnf;

	req.set_store(m_link.store().toStdString());
	req.set_rev(m_link.rev().toStdString());
	m_error = Connection::defaultRPC<PeekReq, PeekCnf>(PEEK_MSG, req, cnf);
	if (m_error)
		return false;

	m_open = true;
	m_handle = cnf.handle();
	return true;
}

bool Document::update(const QString &creator)
{
	close();
	if (!m_link.isDocHeadLink()) {
		m_error = ErrBadF;
		return false;
	}

	UpdateReq req;
	UpdateCnf cnf;

	req.set_store(m_link.store().toStdString());
	req.set_doc(m_link.doc().toStdString());
	req.set_rev(m_link.rev().toStdString());
	if (!creator.isNull())
		req.set_creator_code(creator.toUtf8().constData());
	m_error = Connection::defaultRPC<UpdateReq, UpdateCnf>(UPDATE_MSG, req, cnf);
	if (m_error)
		return false;

	m_open = true;
	m_handle = cnf.handle();
	return true;
}

bool Document::resume(const QString &creator)
{
	close();
	if (!m_link.isDocPreRevLink()) {
		m_error = ErrBadF;
		return false;
	}

	ResumeReq req;
	ResumeCnf cnf;

	req.set_store(m_link.store().toStdString());
	req.set_doc(m_link.doc().toStdString());
	req.set_rev(m_link.rev().toStdString());
	if (!creator.isNull())
		req.set_creator_code(creator.toUtf8().constData());
	m_error = Connection::defaultRPC<ResumeReq, ResumeCnf>(RESUME_MSG, req, cnf);
	if (m_error)
		return false;

	m_open = true;
	m_handle = cnf.handle();
	return true;
}

Value Document::get(const QString &selector)
{
	if (!m_open) {
		m_error = ErrBadF;
		return Value();
	}

	GetDataReq req;
	GetDataCnf cnf;
	req.set_handle(m_handle);
	req.set_selector(selector.toStdString());

	m_error = Connection::defaultRPC<GetDataReq, GetDataCnf>(GET_DATA_MSG, req, cnf);
	if (m_error)
		return Value();

	QByteArray data = QByteArray::fromRawData(cnf.data().data(), cnf.data().length());
	return Value::fromByteArray(data, m_link.store());
}

bool Document::set(const QString &selector, const Value &value)
{
	if (!m_open) {
		m_error = ErrBadF;
		return false;
	}

	QByteArray data = value.toByteArray();

	SetDataReq req;
	req.set_handle(m_handle);
	req.set_selector(selector.toStdString());
	req.set_data(data.constData(), data.size());

	m_error = Connection::defaultRPC<SetDataReq>(SET_DATA_MSG, req);
	if (m_error)
		return false;

	return true;
}

qint64 Document::pos(const QString &attachment) const
{
	return m_pos.value(attachment, 0);
}

bool Document::seek(const QString &attachment, qint64 pos)
{
	if (!m_open || pos < 0)
		return false;

	m_pos[attachment] = pos;
	return true;
}

qint64 Document::read(const QString &attachment, char *data, qint64 maxSize, qint64 off)
{
	if (!m_open) {
		m_error = ErrBadF;
		return -1;
	}

	qint64 len = 0;
	unsigned int mps = Connection::instance()->maxPacketSize();

	while (maxSize > 0) {
		unsigned int chunk = maxSize > mps ? mps : maxSize;

		ReadReq req;
		ReadCnf cnf;

		req.set_handle(m_handle);
		req.set_part(attachment.toStdString());
		req.set_offset(off);
		req.set_length(chunk);
		m_error = Connection::defaultRPC<ReadReq, ReadCnf>(READ_MSG, req, cnf);
		if (m_error)
			return -1;

		unsigned int size = cnf.data().copy(data, chunk);
		off += size;
		len += size;
		data += size;
		maxSize -= size;

		if (size < chunk)
			break;
	}

	return len;
}

qint64 Document::read(const QString &attachment, char *data, qint64 maxSize)
{
	qint64 off = pos(attachment);
	qint64 len = read(attachment, data, maxSize, off);
	if (len > 0)
		m_pos[attachment] = off + len;

	return len;
}

qint64 Document::read(const QString &attachment, QByteArray &data, qint64 maxSize)
{
	data.resize(maxSize);

	qint64 actual = read(attachment, data.data(), maxSize);

	if (actual < 0)
		data.resize(0);
	else if (actual < maxSize)
		data.resize(actual);

	return actual;
}

qint64 Document::readAll(const QString &attachment, QByteArray &data)
{
	QByteArray tmp;

	tmp.resize(0x10000);
	data.resize(0);

	qint64 off = 0;
	qint64 len;
	do {
		len = read(attachment, tmp.data(), 0x10000, off);
		if (len < 0) {
			data.resize(0);
			return len;
		}

		data.append(tmp);
		off += len;
	} while (len == 0x10000);

	data.resize(off);
	return off;
}

bool Document::write(const QString &attachment, const char *data, qint64 size)
{
	if (!m_open) {
		m_error = ErrBadF;
		return false;
	}

	qint64 len = 0;
	unsigned int mps = Connection::instance()->maxPacketSize();

	// buffer the first chunks
	while (len+mps < size) {
		WriteBufferReq req;

		req.set_handle(m_handle);
		req.set_part(attachment.toStdString());
		req.set_data(data, mps);
		m_error = Connection::defaultRPC<WriteBufferReq>(WRITE_BUFFER_MSG, req);
		if (m_error)
			return false;

		data += mps;
		len += mps;
	}

	// commit the last chunk
	WriteCommitReq req;
	qint64 off = pos(attachment);

	req.set_handle(m_handle);
	req.set_part(attachment.toStdString());
	req.set_offset(off);
	req.set_data(data, size-len);
	m_error = Connection::defaultRPC<WriteCommitReq>(WRITE_COMMIT_MSG, req);
	if (m_error)
		return false;

	m_pos[attachment] = off+size;
	return true;
}

bool Document::write(const QString &attachment, const char *data)
{
	return write(attachment, data, qstrlen(data));
}

bool Document::write(const QString &attachment, const QByteArray &data)
{
	return write(attachment, data.constData(), data.size());
}

bool Document::writeAll(const QString &attachment, const char *data, qint64 size)
{
	// wipe out completely to be nice to COW
	if (!resize(attachment, 0))
		return false;

	// announce what are about to write
	if (!resize(attachment, size))
		return false;

	return write(attachment, data, size);
}

bool Document::writeAll(const QString &attachment, const char *data)
{
	return writeAll(attachment, data, qstrlen(data));
}

bool Document::writeAll(const QString &attachment, const QByteArray &data)
{
	return writeAll(attachment, data.constData(), data.size());
}

bool Document::resize(const QString &attachment, qint64 size)
{
	if (!m_open) {
		m_error = ErrBadF;
		return false;
	}

	TruncReq req;
	req.set_handle(m_handle);
	req.set_part(attachment.toStdString());
	req.set_offset(size);

	m_error = Connection::defaultRPC<TruncReq>(TRUNC_MSG, req);
	if (m_error)
		return false;

	return true;
}

QString Document::type() const
{
	if (!m_open)
		return QString();

	if (!m_type.isNull())
		return m_type;

	GetTypeReq req;
	GetTypeCnf cnf;
	req.set_handle(m_handle);

	int error = Connection::defaultRPC<GetTypeReq, GetTypeCnf>(GET_TYPE_MSG, req, cnf);
	if (error)
		return QString();

	m_type = QString::fromUtf8(cnf.type_code().c_str());
	return m_type;
}

//unsigned int Document::flags() const;
//bool Document::setFlags(unsigned int flags);
//bool Document::setType(const QString &type);
//QList<Link> Document::parents() const;
//bool Document::merge(const Link &rev, QDateTime depth=QDateTime(), bool verbose=false);
//bool Document::rebase(const RId &parent);
//bool Document::forget();
//bool Document::remove();
//bool Document::replicateTo(DId &store, QDateTime depth, bool verbose);
//bool Document::forward(const RId &toRev, const DId &srcStore, QDateTime depth,
//	bool verbose);

void Document::close()
{
	if (!m_open)
		return;

	m_open = false;
	m_pos.clear();
	m_type = QString();

	CloseReq req;
	req.set_handle(m_handle);
	Connection::defaultRPC<CloseReq>(CLOSE_MSG, req);
}

bool Document::commit(const QString &comment)
{
	if (!m_open || !m_link.isDocLink()) {
		m_error = ErrBadF;
		return false;
	}

	CommitReq req;
	CommitCnf cnf;

	req.set_handle(m_handle);
	if (!comment.isNull())
		req.set_comment(comment.toUtf8().constData());

	m_error = Connection::defaultRPC<CommitReq, CommitCnf>(COMMIT_MSG, req, cnf);
	if (m_error)
		return false;

	m_link = Link(m_link.store(), m_link.doc(), RId(cnf.rev()));
	return true;
}

bool Document::suspend(const QString &comment)
{
	if (!m_open || !m_link.isDocLink()) {
		m_error = ErrBadF;
		return false;
	}

	SuspendReq req;
	SuspendCnf cnf;

	req.set_handle(m_handle);
	if (!comment.isNull())
		req.set_comment(comment.toUtf8().constData());

	m_error = Connection::defaultRPC<SuspendReq, SuspendCnf>(SUSPEND_MSG, req, cnf);
	if (m_error)
		return false;

	m_link = Link(m_link.store(), m_link.doc(), RId(cnf.rev()), true);
	return true;
}

