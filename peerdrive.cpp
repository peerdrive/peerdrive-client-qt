
#include <QByteArray>
#include <QtDebug>
#include <QtEndian>
#include <QTcpSocket>

#include "peerdrive.h"
#include "peerdrive_client.pb.h"

#define FLAG_REQ	0
#define FLAG_CNF	1
#define FLAG_IND	2
#define FLAG_RSP	3

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


class PConnection {
public:
	PConnection();
	~PConnection();

	int rpc(int msg, const QByteArray &req, QByteArray &cnf);
	static PConnection *instance();
protected:
	int connect(const QString &hostName, quint16 port);
	int send(quint32 ref, int msg, const QByteArray &req);
private:
	struct Completion {
		bool done;
		int msg;
		QByteArray *cnf;
	};

	int poll(Completion *completion);
	void readReady(void);

	QTcpSocket m_socket;
	quint32 m_nextRef;
	QMap<quint32, Completion*> m_confirmations;
	int m_recursion;
	QByteArray m_buf;

	static PConnection *m_instance;
};

PConnection *PConnection::m_instance = NULL;

PConnection::PConnection()
	: m_nextRef(0),
	  m_recursion(0)
{
	m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
	connect("127.0.0.1", 4567);
	PConnection::m_instance = this;
}

PConnection::~PConnection()
{
	while (m_socket.flush())
		m_socket.waitForBytesWritten(1000);
	m_socket.disconnectFromHost();

	PConnection::m_instance = NULL;
}

PConnection* PConnection::instance()
{
	if (PConnection::m_instance)
		return PConnection::m_instance;

	return new PConnection();
}

int PConnection::connect(const QString &hostName, quint16 port)
{
	m_socket.connectToHost(hostName, port);
	if (!m_socket.waitForConnected(1000))
		return enodev;

	return 0;
}

int PConnection::rpc(int msg, const QByteArray &req, QByteArray &cnf)
{
	quint32 ref = m_nextRef++;
	Completion completion;
	completion.done = false;
	completion.cnf = &cnf;
	m_confirmations.insert(ref, &completion);

	cnf.clear();
	int ret = send(ref, (msg << 4) | FLAG_REQ, req);
	if (ret) {
		m_confirmations.remove(ref);
		return ret;
	}

	ret = poll(&completion);
	if (ret) {
		m_confirmations.remove(ref);
		return ret;
	}

	if (completion.msg == msg) {
		return 0;
	} else if (completion.msg == ERROR_MSG) {
		ErrorCnf errCnf;
		if (!errCnf.ParseFromArray(cnf.constData(), cnf.size()))
			return ebadrpc;
		return errCnf.error();
	} else
		return ebadrpc;
}

int PConnection::send(quint32 ref, int msg, const QByteArray &req)
{
	int len = 6 + req.size();

	uchar header[8];
	qToBigEndian((quint16)len, header);
	qToBigEndian((quint32)ref, header + 2);
	qToBigEndian((quint16)msg, header + 6);

	QByteArray fullReq((const char *)header, 8);
	fullReq.append(req);

	return m_socket.write(fullReq) == -1 ? econnreset : 0;
}

int PConnection::poll(Completion *completion)
{
	int ret = 0;

	m_recursion++;

	while (!completion->done) {
		if (!m_socket.waitForReadyRead(-1)) {
			ret = econnreset;
			goto out;
		}
		readReady();
	}

out:
	m_recursion--;
	return ret;
}

void PConnection::readReady(void)
{
	m_buf.append(m_socket.readAll());
	while (m_buf.size() > 2) {
		quint16 expect = qFromBigEndian<quint16>((const uchar *)m_buf.constData());
		if (expect+2 > m_buf.size())
			break;

		quint32 ref = qFromBigEndian<quint32>((const uchar *)m_buf.constData() + 2);
		quint16 msg = qFromBigEndian<quint16>((const uchar *)m_buf.constData() + 6);
		int type = msg & 3;
		msg >>= 4;

		if (type == FLAG_CNF && m_confirmations.contains(ref)) {
			Completion *c = m_confirmations[ref];
			c->done = true;
			c->msg = msg;
			*(c->cnf) = m_buf.mid(8, expect-6);
		}

		m_buf.remove(0, expect+2);
	}
}


PMounts::PMounts()
	: m_sysStore(NULL)
{
	EnumCnf cnf;
	QByteArray rawReq, rawCnf;
	int err = PConnection::instance()->rpc(ENUM_MSG, rawReq, rawCnf);
	if (err) {
		qDebug() << "EnumReq failed: " << err << "\n";
		return;
	}

	if (!cnf.ParseFromArray(rawCnf.constData(), rawCnf.size()))
		return;

	m_sysStore = parse(cnf.sys_store());
	m_sysStore->isSystem = true;

	for (int i = 0; i < cnf.stores_size(); i++)
		m_stores.append(parse(cnf.stores(i)));
}

PMounts::~PMounts()
{
	if (m_sysStore)
		delete m_sysStore;

	foreach(Store *s, m_stores)
		delete s;
}

PMounts::Store *PMounts::parse(const EnumCnf_Store &pb)
{
	Store *s = new Store;

	s->sid     = pb.sid();
	s->src     = pb.src().c_str();
	s->type    = pb.type().c_str();
	s->label   = pb.label().c_str();
	s->options = pb.label().c_str();

	return s;
}

PMounts::Store* PMounts::sysStore() const
{
	return m_sysStore;
}

QList<PMounts::Store*> PMounts::regularStores() const
{
	return m_stores;
}

QList<PMounts::Store*> PMounts::allStores() const
{
	QList<PMounts::Store*> all(m_stores);
	if (m_sysStore)
		all.append(m_sysStore);
	return all;
}

PMounts::Store* PMounts::fromLabel(const QString &label) const
{
	if (label == m_sysStore->label)
		return m_sysStore;

	foreach(Store* s, m_stores) {
		if (s->label == label)
			return s;
	}

	return NULL;
}

PMounts::Store* PMounts::fromSId(const PDId &sid) const
{
	if (sid == m_sysStore->sid)
		return m_sysStore;

	foreach(Store* s, m_stores) {
		if (s->sid == sid)
			return s;
	}

	return NULL;
}



PRevInfo::PRevInfo()
{
}

PRevInfo::PRevInfo(const PRId &rid)
{
	fetch(rid, NULL);
}

PRevInfo::PRevInfo(const PRId &rid, const QList<PDId> &stores)
{
	fetch(rid, &stores);
}

void PRevInfo::fetch(const PRId &rid, const QList<PDId> *stores)
{
	StatReq req;
	QByteArray rawReq;

	rid.toStdString(req.mutable_rev());
	if (stores) {
		QList<PDId>::const_iterator i;
		for (i = stores->constBegin(); i != stores->constEnd(); i++)
			(*i).toStdString(req.add_stores());
	}

	rawReq.reserve(req.ByteSize());
	req.SerializeWithCachedSizesToArray((google::protobuf::uint8*)rawReq.data());

	StatCnf cnf;
	QByteArray rawCnf;
	int err = PConnection::instance()->rpc(STAT_MSG, rawReq, rawCnf);
	if (err)
		goto failed;

	if (!cnf.ParseFromArray(rawCnf.constData(), rawCnf.size())) {
		err = enoent;
		goto failed;
	}

	m_flags = cnf.flags();
	//for (int j = 0; j < cnf.parts_size(); j++) {
	//}
	//repeated Part parts = 2;
	for (int j = 0; j < cnf.parents_size(); j++) {
		PRId rid(cnf.parents(j));
		m_parents.append(rid);
	}
	m_mtime.setTime_t(cnf.mtime() / 1000000);
	m_type = cnf.type_code().c_str();
	m_creator = cnf.creator_code().c_str();
	m_comment = cnf.comment().c_str();
	m_exists = true;

	return;

failed:
	m_error = err;
	m_exists = false;
}
