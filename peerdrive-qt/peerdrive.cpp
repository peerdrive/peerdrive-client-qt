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

#include <QtDebug>
#include <QtEndian>
#include <QProcessEnvironment>

#include "peerdrive.h"
#include "peerdrive_internal.h"

#define FLAG_REQ	0
#define FLAG_CNF	1
#define FLAG_IND	2
#define FLAG_RSP	3

using namespace PeerDrive;

Connection *Connection::m_instance = NULL;

Connection::Connection() : QObject()
{
	m_nextRef = 0;
	m_recursion = 0;

	m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
	QObject::connect(&m_socket, SIGNAL(readyRead()), this, SLOT(readReady()));
	QObject::connect(this, SIGNAL(watchReady()), this,
		SLOT(dispatchIndications()), Qt::QueuedConnection);

	QString address = QProcessEnvironment::systemEnvironment().value("PEERDRIVE");
	if (address.isEmpty()) {
		connect();
	} else {
		QString hostName = "127.0.0.1";
		quint16 port = 4567;

		QStringList addrParts = address.split(":");
		switch (addrParts.size()) {
			case 2:
				port = addrParts.at(1).toInt();
				/* fall through */
			case 1:
				hostName = addrParts.at(0);
				/* fall through */
			default:
				connect(hostName, port);
		}
	}
	Connection::m_instance = this;
}

Connection::~Connection()
{
	while (m_socket.flush())
		m_socket.waitForBytesWritten(1000);
	m_socket.disconnectFromHost();

	if (Connection::m_instance == this)
		Connection::m_instance = NULL;
}

Connection* Connection::instance()
{
	if (Connection::m_instance)
		return Connection::m_instance;

	return new Connection();
}

int Connection::connect(const QString &hostName, quint16 port)
{
	m_socket.connectToHost(hostName, port);
	if (!m_socket.waitForConnected(1000))
		return ERR_ENODEV;

	InitReq req;
	InitCnf cnf;
	QByteArray rawReq, rawCnf;

	req.set_major(0);
	req.set_minor(0);

	rawReq.resize(req.ByteSize());
	req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());

	int err = rpc(INIT_MSG, rawReq, rawCnf);
	if (err) {
		qDebug() << "::PeerDrive::Connection: INIT_MSG failed:" << err;
		goto failed;
	}

	if (!cnf.ParseFromArray(rawCnf.constData(), rawCnf.size())) {
		err = ERR_EBADRPC;
		goto failed;
	}

	if (cnf.major() != 0 || cnf.minor() != 0) {
		qDebug() << "::PeerDrive::Connection: unsupported server version:" <<
			cnf.major() << ":" << cnf.minor();
		err = ERR_ERPCMISMATCH;
		goto failed;
	}

	m_maxPacketSize = cnf.max_packet_size();
	return 0;

failed:
	m_socket.disconnectFromHost();
	return err;
}

int Connection::rpc(int msg, const QByteArray &req, QByteArray &cnf)
{
	cnf.clear();

	Completion completion;
	completion.cnf = &cnf;

	return _rpc(msg, req, &completion);
}

int Connection::rpc(int msg, const QByteArray &req)
{
	QByteArray cnf;

	Completion completion;
	completion.cnf = &cnf;

	return _rpc(msg, req, &completion);
}

int Connection::_rpc(int msg, const QByteArray &req, Completion *completion)
{
	quint32 ref = m_nextRef++;
	completion->done = false;
	m_confirmations.insert(ref, completion);

	int ret = send(ref, (msg << 4) | FLAG_REQ, req);
	if (ret) {
		m_confirmations.remove(ref);
		return ret;
	}

	ret = poll(completion);
	if (ret) {
		m_confirmations.remove(ref);
		return ret;
	}

	if (completion->msg == msg) {
		return 0;
	} else if (completion->msg == ERROR_MSG) {
		ErrorCnf errCnf;
		if (!errCnf.ParseFromArray(completion->cnf->constData(),
		                           completion->cnf->size()))
			return ERR_EBADRPC;
		return errCnf.error();
	} else
		return ERR_EBADRPC;
}

int Connection::send(quint32 ref, int msg, const QByteArray &req)
{
	int len = 6 + req.size();

	uchar header[8];
	qToBigEndian((quint16)len, header);
	qToBigEndian((quint32)ref, header + 2);
	qToBigEndian((quint16)msg, header + 6);

	QByteArray fullReq((const char *)header, 8);
	fullReq.append(req);

	return m_socket.write(fullReq) == -1 ? ERR_ECONNRESET : 0;
}

int Connection::poll(Completion *completion)
{
	int ret = 0;

	m_recursion++;

	while (!completion->done) {
		if (!m_socket.waitForReadyRead(-1)) {
			ret = ERR_ECONNRESET;
			goto out;
		}
		readReady();
	}

out:
	m_recursion--;
	return ret;
}

void Connection::readReady()
{
	m_buf.append(m_socket.readAll());
	while (m_buf.size() > 2) {
		quint16 expect = qFromBigEndian<quint16>((const uchar *)m_buf.constData());
		if (expect+2 > m_buf.size())
			break;

		quint32 ref = qFromBigEndian<quint32>((const uchar *)m_buf.constData() + 2);
		quint16 msg = qFromBigEndian<quint16>((const uchar *)m_buf.constData() + 6);
		int type = msg & 3;
		int len = expect-6;
		msg >>= 4;

		if (type == FLAG_CNF && m_confirmations.contains(ref)) {
			Completion *c = m_confirmations[ref];
			c->done = true;
			c->msg = msg;
			*(c->cnf) = m_buf.mid(8, len);
		} else if (type == FLAG_IND) {
			// queue indication with message header
			m_indications.append(m_buf.mid(6, len+2));
		}

		m_buf.remove(0, expect+2);
	}

	if (!m_indications.isEmpty())
		dispatchIndications();
}

void Connection::dispatchIndications()
{
	if (m_recursion > 0) {
		emit watchReady();
		return;
	}

	while (!m_indications.isEmpty()) {
		QByteArray buf = m_indications.takeFirst();

		quint16 msg = qFromBigEndian<quint16>((const uchar *)buf.constData()) >> 4;
		switch (msg) {
			case WATCH_MSG:
				dispatchWatch(buf.mid(2));
				break;
		}
	}
}

void Connection::dispatchWatch(QByteArray packet)
{
	WatchInd ind;
	if (!ind.ParseFromArray(packet.constData(), packet.size()))
		return;

	switch (ind.type()) {
		case WatchInd::DOC:
		{
			DId doc(ind.element());
			if (m_docWatches.contains(doc))
				foreach (Watch *w, m_docWatches.value(doc))
					w->dispatch(ind.event());
			break;
		}
		case WatchInd::REV:
		{
			RId rev(ind.element());
			if (m_revWatches.contains(rev))
				foreach (Watch *w, m_revWatches.value(rev))
					w->dispatch(ind.event());
			break;
		}
	}
}

int Connection::addWatch(Watch *watch, const DId &doc)
{
	int err = 0;

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

int Connection::addWatch(Watch *watch, const RId &rev)
{
	int err = 0;

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

void Connection::delWatch(Watch *watch, const DId &doc)
{
	QList<Watch*> &watches = m_docWatches[doc];
	watches.removeAll(watch);

	if (watches.isEmpty()) {
		WatchRemReq req;
		QByteArray rawReq;

		req.set_type(WatchRemReq::DOC);
		req.set_element(doc.toStdString());

		rawReq.resize(req.ByteSize());
		req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());

		int err = rpc(WATCH_REM_MSG, rawReq);
		if (err)
			qDebug() << "::PeerDrive::Connection: delWatch failed:" << err;
		m_docWatches.remove(doc);
	}
}

void Connection::delWatch(Watch *watch, const RId &rev)
{
	QList<Watch*> &watches = m_revWatches[rev];
	watches.removeAll(watch);

	if (watches.isEmpty()) {
		WatchRemReq req;
		QByteArray rawReq;

		req.set_type(WatchRemReq::REV);
		req.set_element(rev.toStdString());

		rawReq.resize(req.ByteSize());
		req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());

		int err = rpc(WATCH_REM_MSG, rawReq);
		if (err)
			qDebug() << "::PeerDrive::Connection: delWatch failed:" << err;
		m_revWatches.remove(rev);
	}
}

/****************************************************************************/

const Part Part::FILE("FILE");
const Part Part::META("META");
const Part Part::PDSD("PDSD");

/****************************************************************************/

Link::Link()
{
	m_state = INVALID;
}

Link::Link(const DId &store, const RId &rev)
{
	m_state = REV;
	m_store = store;
	m_rev = rev;
}

Link::Link(const DId &store, const DId &doc, bool doUpdate)
{
	m_state = DOC_NOT_FOUND;
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

bool Link::operator== (const Link &other) const
{
	return m_state == other.m_state &&
		m_store == other.m_store &&
		m_rev == other.m_rev &&
		m_doc == other.m_doc;
}

bool Link::operator< (const Link &other) const
{
	return m_state != other.m_state ? m_state < other.m_state :
		(m_store != other.m_store ? m_store < other.m_store :
		(m_rev != other.m_rev ? m_rev < other.m_rev :
		m_doc < other.m_doc));
}

bool Link::update()
{
	switch (m_state) {
		case INVALID:
			return false;

		case DOC_NOT_FOUND:
		case DOC_HEAD:
		{
			DocInfo info(m_doc, m_store);
			if (!info.exists()) {
				m_state = DOC_NOT_FOUND;
				return false;
			}
			m_rev = info.head(m_store).rev();
			m_state = DOC_HEAD;
			return true;
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
	return m_state == DOC_NOT_FOUND ||
	       m_state == DOC_HEAD ||
	       m_state == DOC_PRE_REV;
}

bool Link::isDocHeadLink() const
{
	return m_state == DOC_NOT_FOUND ||
	       m_state == DOC_HEAD;
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
		case DOC_NOT_FOUND:
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

/****************************************************************************/

Watch::Watch() : QObject()
{
	m_watching = false;
}

Watch::~Watch()
{
	clear();
}

void Watch::watch(const Link &item)
{
	clear();
	if (!item.isValid())
		return;

	m_link = item;
	if (item.isDocLink()) {
		if (Connection::instance()->addWatch(this, item.doc()) == 0)
			m_watching = true;
	} else {
		if (Connection::instance()->addWatch(this, item.rev()) == 0)
			m_watching = true;
	}
}

void Watch::clear()
{
	if (m_watching) {
		if (m_link.isDocLink())
			Connection::instance()->delWatch(this, m_link.doc());
		else
			Connection::instance()->delWatch(this, m_link.rev());

		m_watching = false;
	}
}

void Watch::dispatch(int event)
{
	switch (event) {
		case WatchInd::MODIFIED:
			emit modified();
			break;
		case WatchInd::APPEARED:
			emit appeared();
			emit distributionChanged(APPEARED);
			break;
		case WatchInd::REPLICATED:
			emit replicated();
			emit distributionChanged(REPLICATED);
			break;
		case WatchInd::DIMINISHED:
			emit diminished();
			emit distributionChanged(DIMINISHED);
			break;
		case WatchInd::DISAPPEARED:
			emit disappeared();
			emit distributionChanged(DISAPPEARED);
			break;
	}
}

/****************************************************************************/

Mounts::Mounts()
	: m_sysStore(NULL)
{
	EnumCnf cnf;
	QByteArray rawReq, rawCnf;
	int err = Connection::instance()->rpc(ENUM_MSG, rawReq, rawCnf);
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

/****************************************************************************/

RevInfo::RevInfo()
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
	for (int j = 0; j < cnf.parts_size(); j++) {
		const StatCnf_Part & p = cnf.parts(j);
		Part part(p.fourcc());
		m_partSizes[part] = p.size();
		m_partHashes[part] = PId(p.pid());
	}
	for (int j = 0; j < cnf.parents_size(); j++) {
		RId rid(cnf.parents(j));
		m_parents.append(rid);
	}
	m_mtime.setTime_t(cnf.mtime() / 1000000);
	m_type = QString::fromUtf8(cnf.type_code().c_str());
	m_creator = QString::fromUtf8(cnf.creator_code().c_str());
	m_comment = QString::fromUtf8(cnf.comment().c_str());
	m_exists = true;
}

/****************************************************************************/

DocInfo::DocInfo()
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

int DocInfo::error() const
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


Document::Document()
{
	m_open = false;
	m_error = ERR_EBADF;
}

Document::Document(const Link &link)
{
	m_open = false;
	m_error = ERR_EBADF;
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

int Document::error() const
{
	return m_error;
}

bool Document::peek()
{
	close();
	if (!m_link.isValid()) {
		m_error = ERR_EBADF;
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
	if (!m_link.isValid() || !m_link.isDocHeadLink()) {
		m_error = ERR_EBADF;
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
	if (!m_link.isValid() || !m_link.isDocPreRevLink()) {
		m_error = ERR_EBADF;
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

qint64 Document::pos(const Part &part) const
{
	return m_pos.value(part, 0);
}

bool Document::seek(const Part &part, qint64 pos)
{
	if (!m_open || pos < 0)
		return false;

	m_pos[part] = pos;
	return true;
}

qint64 Document::read(const Part &part, char *data, qint64 maxSize, qint64 off)
{
	if (!m_open) {
		m_error = ERR_EBADF;
		return -1;
	}

	qint64 len = 0;
	unsigned int mps = Connection::instance()->maxPacketSize();

	while (maxSize > 0) {
		unsigned int chunk = maxSize > mps ? mps : maxSize;

		ReadReq req;
		ReadCnf cnf;

		req.set_handle(m_handle);
		req.set_part(part.toStdString());
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

qint64 Document::read(const Part &part, char *data, qint64 maxSize)
{
	qint64 off = pos(part);
	qint64 len = read(part, data, maxSize, off);
	if (len > 0)
		m_pos[part] = off + len;

	return len;
}

qint64 Document::read(const Part &part, QByteArray &data, qint64 maxSize)
{
	data.resize(maxSize);

	qint64 actual = read(part, data.data(), maxSize);

	if (actual < 0)
		data.resize(0);
	else if (actual < maxSize)
		data.resize(actual);

	return actual;
}

qint64 Document::readAll(const Part &part, QByteArray &data)
{
	QByteArray tmp;

	tmp.resize(0x10000);
	data.resize(0);

	qint64 off = 0;
	qint64 len;
	do {
		len = read(part, tmp.data(), 0x10000, off);
		if (len < 0) {
			data.resize(0);
			return len;
		}

		data.append(tmp);
		off += len;
	} while (len == 0x10000);

	return off;
}

bool Document::write(const Part &part, const char *data, qint64 size)
{
	if (!m_open) {
		m_error = ERR_EBADF;
		return false;
	}

	qint64 len = 0;
	unsigned int mps = Connection::instance()->maxPacketSize();

	// buffer the first chunks
	while (len+mps < size) {
		WriteBufferReq req;

		req.set_handle(m_handle);
		req.set_part(part.toStdString());
		req.set_data(data, mps);
		m_error = Connection::defaultRPC<WriteBufferReq>(WRITE_BUFFER_MSG, req);
		if (m_error)
			return false;

		data += mps;
		len += mps;
	}

	// commit the last chunk
	WriteCommitReq req;
	qint64 off = pos(part);

	req.set_handle(m_handle);
	req.set_part(part.toStdString());
	req.set_offset(off);
	req.set_data(data, size-len);
	m_error = Connection::defaultRPC<WriteCommitReq>(WRITE_COMMIT_MSG, req);
	if (m_error)
		return false;

	m_pos[part] = off+size;
	return true;
}

bool Document::write(const Part &part, const char *data)
{
	return write(part, data, qstrlen(data));
}

bool Document::write(const Part &part, const QByteArray &data)
{
	return write(part, data.constData(), data.size());
}

bool Document::writeAll(const Part &part, const char *data, qint64 size)
{
	// wipe out completely to be nice to COW
	if (!resize(part, 0))
		return false;

	// announce what are about to write
	if (!resize(part, size))
		return false;

	return write(part, data, size);
}

bool Document::writeAll(const Part &part, const char *data)
{
	return writeAll(part, data, qstrlen(data));
}

bool Document::writeAll(const Part &part, const QByteArray &data)
{
	return writeAll(part, data.constData(), data.size());
}

bool Document::resize(const Part &part, qint64 size)
{
	if (!m_open) {
		m_error = ERR_EBADF;
		return false;
	}

	TruncReq req;
	req.set_handle(m_handle);
	req.set_part(part.toStdString());
	req.set_offset(size);

	m_error = Connection::defaultRPC<TruncReq>(TRUNC_MSG, req);
	if (m_error)
		return false;

	return true;
}

//unsigned int Document::flags() const;
//bool Document::setFlags(unsigned int flags);
//QString Document::type() const;
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

	CloseReq req;
	req.set_handle(m_handle);
	Connection::defaultRPC<CloseReq>(CLOSE_MSG, req);
}

bool Document::commit(const QString &comment)
{
	if (!m_open || !m_link.isDocLink()) {
		m_error = ERR_EBADF;
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
		m_error = ERR_EBADF;
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

