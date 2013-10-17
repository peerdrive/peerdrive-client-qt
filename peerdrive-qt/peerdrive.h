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
#ifndef _PEERDRIVE_H_
#define _PEERDRIVE_H_

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QSharedData>
#include <QString>

#include <string>

namespace PeerDrive {
	class DId;
	class RId;
	class PId;
	class Link;
}

uint qHash(const PeerDrive::DId&);
uint qHash(const PeerDrive::RId&);
uint qHash(const PeerDrive::PId&);
uint qHash(const PeerDrive::Link&);

namespace PeerDrive {

class EnumCnf_Store; // FIXME: remove from here

enum Error {
	ErrNoError = 0,
	ErrConflict = 1,
	Err2Big = 2,
	ErrAcces = 3,
	ErrAddrInUse = 4,
	ErrAddrNotAvail = 5,
	ErrAdv = 6,
	ErrAFNoSupport = 7,
	ErrAgain = 8,
	ErrAlign = 9,
	ErrAlready = 10,
	ErrBadE = 11,
	ErrBadF = 12,
	ErrBadFd = 13,
	ErrBadMsg = 14,
	ErrBadR = 15,
	ErrBadRPC = 16,
	ErrBadRQC = 17,
	ErrBadSLT = 18,
	ErrBFont = 19,
	ErrBusy = 20,
	ErrChild = 21,
	ErrCHRNG = 22,
	ErrComm = 23,
	ErrConnAborted = 24,
	ErrConnRefused = 25,
	ErrConnReset = 26,
	ErrDeadLK = 27,
	ErrDeadlock = 28,
	ErrDestAddrReq = 29,
	ErrDirty = 30,
	ErrDOM = 31,
	ErrDOTDOT = 32,
	ErrDQUOT = 33,
	ErrDUPPKG = 34,
	ErrEXIST = 35,
	ErrFAULT = 36,
	ErrFBIG = 37,
	ErrHOSTDOWN = 38,
	ErrHOSTUNREACH = 39,
	ErrIDRM = 40,
	ErrINIT = 41,
	ErrINPROGRESS = 42,
	ErrINTR = 43,
	ErrINVAL = 44,
	ErrIO = 45,
	ErrISCONN = 46,
	ErrISDIR = 47,
	ErrISNAM = 48,
	ErrLBIN = 49,
	ErrL2HLT = 50,
	ErrL2NSYNC = 51,
	ErrL3HLT = 52,
	ErrL3RST = 53,
	ErrLIBACC = 54,
	ErrLIBBAD = 55,
	ErrLIBEXEC = 56,
	ErrLIBMAX = 57,
	ErrLIBSCN = 58,
	ErrLNRNG = 59,
	ErrLOOP = 60,
	ErrMFILE = 61,
	ErrMLINK = 62,
	ErrMSGSIZE = 63,
	ErrMULTIHOP = 64,
	ErrNAMETOOLONG = 65,
	ErrNAVAIL = 66,
	ErrNET = 67,
	ErrNETDOWN = 68,
	ErrNETRESET = 69,
	ErrNETUNREACH = 70,
	ErrNFILE = 71,
	ErrNOANO = 72,
	ErrNOBUFS = 73,
	ErrNOCSI = 74,
	ErrNODATA = 75,
	ErrNODEV = 76,
	ErrNOENT = 77,
	ErrNOEXEC = 78,
	ErrNOLCK = 79,
	ErrNOLINK = 80,
	ErrNOMEM = 81,
	ErrNOMSG = 82,
	ErrNONET = 83,
	ErrNOPKG = 84,
	ErrNOPROTOOPT = 85,
	ErrNOSPC = 86,
	ErrNOSR = 87,
	ErrNOSYM = 88,
	ErrNOSYS = 89,
	ErrNOTBLK = 90,
	ErrNOTCONN = 91,
	ErrNOTDIR = 92,
	ErrNOTEMPTY = 93,
	ErrNOTNAM = 94,
	ErrNOTSOCK = 95,
	ErrNOTSUP = 96,
	ErrNOTTY = 97,
	ErrNOTUNIQ = 98,
	ErrNXIO = 99,
	ErrOPNOTSUPP = 100,
	ErrPERM = 101,
	ErrPFNOSUPPORT = 102,
	ErrPIPE = 103,
	ErrPROCLIM = 104,
	ErrPROCUNAVAIL = 105,
	ErrPROGMISMATCH = 106,
	ErrPROGUNAVAIL = 107,
	ErrPROTO = 108,
	ErrPROTONOSUPPORT = 109,
	ErrPROTOTYPE = 110,
	ErrRANGE = 111,
	ErrREFUSED = 112,
	ErrREMCHG = 113,
	ErrREMDEV = 114,
	ErrREMOTE = 115,
	ErrREMOTEIO = 116,
	ErrREMOTERELEASE = 117,
	ErrROFS = 118,
	ErrRPCMISMATCH = 119,
	ErrRREMOTE = 120,
	ErrSHUTDOWN = 121,
	ErrSOCKTNOSUPPORT = 122,
	ErrSPIPE = 123,
	ErrSRCH = 124,
	ErrSRMNT = 125,
	ErrSTALE = 126,
	ErrSUCCESS = 127,
	ErrTIME = 128,
	ErrTIMEDOUT = 129,
	ErrTOOMANYREFS = 130,
	ErrTXTBSY = 131,
	ErrUCLEAN = 132,
	ErrUNATCH = 133,
	ErrUSERS = 134,
	ErrVERSION = 135,
	ErrWOULDBLOCK = 136,
	ErrXDEV = 137,
	ErrXFULL = 138,
	ErrXDOMAIN = 139,
};

// TODO: derive from Exception, add what()
class ValueError
{
};

class ValueData;

class Value {
public:
	enum Type
	{
		NUL,
		INT,
		UINT,
		FLOAT,
		DOUBLE,
		STRING,
		BOOL,
		LIST,
		DICT,
		LINK
	};

	Value(Type type = NUL);
	Value(int value);
	Value(unsigned int value);
	Value(qint64 value );
	Value(quint64 value );
	Value(float value);
	Value(double value);
	Value(const QString &value);
	Value(bool value);
	Value(const Link &link);
	Value(const Value &other);
	~Value();

	Value &operator=( const Value &other );

	Type type() const;
	inline bool isNull() { return type() == NUL; }

	QString asString() const;
	int asInt() const;
	unsigned int asUInt() const;
	qint64 asInt64() const;
	quint64 asUInt64() const;
	float asFloat() const;
	double asDouble() const;
	bool asBool() const;
	Link asLink() const;

	int size() const;
	Value &operator[]( int index );
	const Value& operator[](int index) const;
	void append( const Value &value );
	void remove(int index);

	Value &operator[]( const QString &key );
	const Value& operator[](const QString &key) const;
	bool contains(const QString &key) const;
	Value get(const QString &key, const Value &defaultValue = Value()) const;
	void remove(const QString &key);
	QList<QString> keys() const;

	static Value fromByteArray(const QByteArray &data, const DId &store);
	QByteArray toByteArray() const;

private:
	QSharedDataPointer<ValueData> d;
};


class DId {
public:
	DId() { }
	DId(const QByteArray &ba) : id(ba) { }
	DId(const std::string &str) { id = QByteArray(str.c_str(), str.size()); }
	std::string toStdString() const { return std::string(id.constData(), id.size()); }
	QByteArray toByteArray() const { return id; }

	bool operator== (const DId &other) const { return id == other.id; }
	bool operator!= (const DId &other) const { return id != other.id; }
	bool operator< (const DId &other) const { return id < other.id; }

private:
	QByteArray id;
};

class RId {
public:
	RId() { }
	RId(const QByteArray &ba) : id(ba) { }
	RId(const std::string &str) { id = QByteArray(str.c_str(), str.size()); }
	std::string toStdString() const { return std::string(id.constData(), id.size()); }
	QByteArray toByteArray() const { return id; }

	bool operator== (const RId &other) const { return id == other.id; }
	bool operator!= (const RId &other) const { return id != other.id; }
	bool operator< (const RId &other) const { return id < other.id; }

private:
	QByteArray id;
};

class PId {
public:
	PId() { }
	PId(const QByteArray &ba) : id(ba) { }
	PId(const std::string &str) { id = QByteArray(str.c_str(), str.size()); }
	std::string toStdString() const { return std::string(id.constData(), id.size()); }
	QByteArray toByteArray() const { return id; }

	bool operator== (const PId &other) const { return id == other.id; }
	bool operator!= (const PId &other) const { return id != other.id; }
	bool operator< (const PId &other) const { return id < other.id; }

private:
	QByteArray id;
};

/**
 * Represents three possible link classes:
 *  - DocHeadLinks
 *  - DocPreLinks
 *  - RevLinks
 */
class Link {
public:
	Link();
	Link(const QString &uri);
	Link(const DId &store, const RId &rev);
	Link(const DId &store, const DId &doc, bool update = true);
	Link(const DId &store, const DId &doc, const RId &rev, bool isPreRev = false);

	bool operator== (const Link &other) const;
	bool operator!= (const Link &other) const { return !(*this == other); }
	bool operator< (const Link &other) const;

	bool update();
	bool update(const DId &newStore);
	void setStore(const DId &newStore);

	bool isValid() const;
	bool isRevLink() const;
	bool isDocLink() const;
	bool isDocHeadLink() const;
	bool isDocPreRevLink() const;

	DId store() const;
	RId rev() const;
	DId doc() const;

	QString uri() const;
	QString getOsPath() const;

protected:
	enum State {
		INVALID,
		DOC_PRE_REV,
		DOC_HEAD,
		REV
	};

	State m_state;
	DId m_store;
	RId m_rev;
	DId m_doc;

	friend uint ::qHash(const Link&);
};

class LinkWatcher : public QObject {
	Q_OBJECT

public:
	LinkWatcher(QObject *parent = NULL);
	virtual ~LinkWatcher();

	void addWatch(const Link &item);
	void removeWatch(const Link &item);

	enum Distribution {
		APPEARED,
		REPLICATED,
		DIMINISHED,
		DISAPPEARED
	};

	static const Link rootDoc;

signals:
	void modified(const Link &item);
	void appeared(const Link &item);
	void replicated(const Link &item);
	void diminished(const Link &item);
	void disappeared(const Link &item);
	void distributionChanged(const Link &item, Distribution how);

private:
	void dispatch(const Link &item, int event);
	friend class Connection;

	QList<Link> watchedLinks;
};

class ProgressWatcher : public QObject
{
	Q_OBJECT

public:
	ProgressWatcher(QObject *parent = NULL);
	virtual ~ProgressWatcher();

	enum Type {
		Synchronization,
		Replication
	};

	enum State {
		StateRunning,
		StatePaused,
		StateError
	};

	QList<unsigned int> tags() const;

	Link source(unsigned int tag) const;
	Link destination(unsigned int tag) const;
	Link item(unsigned int tag) const;
	Type type(unsigned int tag) const;
	State state(unsigned int tag) const;
	Error error(unsigned int tag) const;
	Link errorItem(unsigned int tag) const;
	int progress(unsigned int tag) const;

	static int pause(unsigned int tag);
	static int resume(unsigned int tag, bool skip = false);
	static int stop(unsigned int tag);

signals:
	void started(unsigned int tag);
	void changed(unsigned int tag);
	void finished(unsigned int tag);
	friend class Connection;
};

class Mounts {
public:
	struct Store {
		DId sid;
		QString src;
		QString type;
		QString label;
		QString options;
		bool isSystem;
	};

	Mounts();
	~Mounts();

	Store* sysStore() const;
	QList<Store*> regularStores() const;
	QList<Store*> allStores() const;
	Store* fromLabel(const QString &label) const;
	Store* fromSId(const DId &sid) const;

	static DId mount(Error *result, const QString &src, const QString &label,
	                 const QString &type, const QString &options = QString(),
					 const QString &credentials = QString());
	static Error unmount(const DId &sid);

private:
	Store *parse(const EnumCnf_Store &pb);
	Store *m_sysStore;
	QList<Store*> m_stores;
};


class RevInfoPrivate;

class RevInfo {
public:
	RevInfo();
	RevInfo(const RId &rid);
	RevInfo(const RId &rid, const QList<DId> &stores);
	~RevInfo();

	RevInfo(const RevInfo &other);
	RevInfo& operator=(const RevInfo &other);

	bool exists() const;
	Error error() const;

	QList<DId> stores() const;
	int flags() const;
	quint64 size() const;
	quint64 dataSize() const;
	quint64 attachmentSize(const QString &attachment) const;
	PId dataHash() const;
	PId attachmentHash(const QString &attachment) const;
	QDateTime attachmentMtime(const QString &attachment) const;
	QDateTime attachmentCrtime(const QString &attachment) const;
	QList<QString> attachments() const;
	QList<RId> parents() const;
	QDateTime mtime() const;
	QDateTime crtime() const;
	QString type() const;
	QString creator() const;
	QString comment() const;

private:
	QSharedDataPointer<RevInfoPrivate> d;
	friend class Document;
};

class DocInfo {
public:
	DocInfo();
	DocInfo(const DId &doc);
	DocInfo(const DId &doc, const DId &store);
	DocInfo(const DId &doc, const QList<DId> &stores);

	bool exists() const;
	bool exists(const DId &store) const;
	Error error() const;

	QList<DId> stores() const;
	QList<DId> stores(const RId &rev) const;
	QList<Link> heads() const;
	Link head(const DId &store) const;
	QList<Link> preliminaryHeads() const;
	QList<Link> preliminaryHeads(const DId &store) const;

private:
	void fetch(const DId &doc, const QList<DId> &stores = QList<DId>());

	Error m_error;
	DId m_doc;
	QMap<DId, QList<Link> > m_stores; // store -> revs ++ preRevs
	QMap<RId, QList<DId> > m_revs; // revs -> stores
	QMap<RId, QList<DId> > m_preRevs; // preRevs -> stores
};

class LinkInfo {
public:
	LinkInfo();
	LinkInfo(const RId &rev);
	LinkInfo(const RId &rev, const QList<DId> &stores);

	Error error() const;

	QList<DId> docLinks() const;
	QList<RId> revLinks() const;

private:
	void fetch(const RId &rev, const QList<DId> *stores);

	bool m_exists;
	Error m_error;
	QList<DId> m_docLinks;
	QList<RId> m_revLinks;
};

/**
 *
 * The Document will track the revision of the document that was last used. That
 * means that the object is always pointing to the last assigned, read or committed
 * revision. If the document was changed by some other process the object will not
 *
 */
class Document {
public:
	Document();
	Document(const Link &link);
	~Document();

	static Document *fork(const Link &parent);
	static Document *create(const DId &store);

	const Link link() const;
	void setLink(const Link &link);
	Error error() const;

	bool peek();
	bool update(const QString &creator = QString());
	bool resume(const QString &creator = QString());

	bool forget();
	bool remove();
	bool replicateTo(DId &store, QDateTime depth=QDateTime(), bool verbose=false);
	bool forward(const RId &toRev, const DId &srcStore,
		QDateTime depth=QDateTime(), bool verbose=false);

	/*
	 * Structured data
	 */

	Value get(const QString &selector);
	bool set(const QString &selector, const Value &value);

	/*
	 * Attachments
	 */

	qint64 read(const QString &attachment, char *data, qint64 maxSize);
	qint64 read(const QString &attachment, QByteArray &data, qint64 maxSize);
	qint64 readAll(const QString &attachment, QByteArray &data);

	bool write(const QString &attachment, const char *data, qint64 size);
	bool write(const QString &attachment, const char *data);
	bool write(const QString &attachment, const QByteArray &data);
	bool writeAll(const QString &attachment, const char *data, qint64 size);
	bool writeAll(const QString &attachment, const char *data);
	bool writeAll(const QString &attachment, const QByteArray &data);
	bool resize(const QString &attachment, qint64 size);

	qint64 pos(const QString &attachment) const;
	bool seek(const QString &attachment, qint64 pos);

	/*
	 * Metadata
	 */

	RevInfo info() const;
	bool setFlags(unsigned int flags);
	bool setType(const QString &type);
	bool setMTime(const QDateTime &mtime);
	bool merge(const Link &rev, QDateTime depth=QDateTime(), bool verbose=false);
	bool rebase(const RId &parent);

	void close();
	bool commit(const QString &comment = QString());
	bool suspend(const QString &comment = QString());

private:
	qint64 read(const QString &attachment, char *data, qint64 maxSize, qint64 off);

	bool m_open;
	unsigned int m_handle;
	Error m_error;
	Link m_link;
	QMap<QString, qint64> m_pos;
	mutable QString m_type;
};

class Replicator {
public:
	Replicator();
	~Replicator();

	bool replicate(const Link &item, const DId &dstStore, QDateTime depth=QDateTime(),
		bool verbose=false);
	Error error() const;
	void release();

private:
	bool m_open;
	Error m_error;
	unsigned int m_handle;
};

}

Q_DECLARE_METATYPE(PeerDrive::DId);
Q_DECLARE_METATYPE(PeerDrive::RId);
Q_DECLARE_METATYPE(PeerDrive::PId);
Q_DECLARE_METATYPE(PeerDrive::Link);

#endif
