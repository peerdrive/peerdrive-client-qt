
#include <QList>
#include <QString>
#include <QDateTime>
#include <QMap>

#include <string>

class PDId {
public:
	PDId() { };
	PDId(const std::string &str) : id(str) { };
	void toStdString(std::string *s) const { *s = id; };
	bool operator== (const PDId &other) const { return id == other.id; };
private:
	std::string id;
};

class PRId {
public:
	PRId() { };
	PRId(const std::string &str) : id(str) { };
	void toStdString(std::string *s) const { *s = id; };
	bool operator== (const PRId &other) const { return id == other.id; };
private:
	std::string id;
};

class PPId {
public:
};

class PPart {
public:
	PPart(const std::string &str);

	inline bool operator< (const PPart &other) const
	{
		return part < other.part;
	}

private:
	quint32 part;
};

class EnumCnf_Store;

class PMounts {
public:
	struct Store {
		PDId sid;
		QString src;
		QString type;
		QString label;
		QString options;
		bool isSystem;
	};

	PMounts();
	~PMounts();

	Store* sysStore() const;
	QList<Store*> regularStores() const;
	QList<Store*> allStores() const;
	Store* fromLabel(const QString &label) const;
	Store* fromSId(const PDId &sid) const;

private:
	Store *parse(const EnumCnf_Store &pb);
	Store *m_sysStore;
	QList<Store*> m_stores;
};


class PRevInfo {
public:
	PRevInfo();
	PRevInfo(const PRId &rid);
	PRevInfo(const PRId &rid, const QList<PDId> &stores);
	PRevInfo(const PRevInfo &revInfo);
	~PRevInfo();

	bool exists();

	QList<PDId> stores();
	int flags();
	quint64 size();
	quint64 size(const PPart &part);
	PPId hash(const PPart &part);
	QList<PPart> parts();
	QList<PRId> parents();
	QDateTime mtime();
	QString type();
	QString creator();
	QString comment();

private:
	void fetch(const PRId &rid, const QList<PDId> *stores);

	bool m_exists;
	int m_error;
	quint32 m_flags;
	QList<PRId> m_parents;
	QDateTime m_mtime;
	QString m_type;
	QString m_creator;
	QString m_comment;
	QMap<PPart, quint64> m_partSizes;
	QMap<PPart, PPId> m_partHashes;
};

class PDocInfo {
};
