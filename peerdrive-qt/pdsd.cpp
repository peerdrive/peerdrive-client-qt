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

#include <QtEndian>
#include <QStringList>
#include <stdexcept>

#include "pdsd.h"
#include "peerdrive_internal.h"

namespace PeerDrive {

class ValueData : public QSharedData
{
public:
	ValueData();
	ValueData(Value::Type t);
	ValueData(const ValueData &other);
	~ValueData();

	void convert(Value::Type t);

	Value::Type type;
	union {
		qint64 int64;
		quint64 uint64;
		float real32;
		double real64;
		bool boolean;
		QString *string;
		QList<Value> *list;
		QMap<QString, Value> *dict;
		Link *link;
	} value;
};

/****************************************************************************/

enum Tag {
	TAG_DICT   = 0x00,
	TAG_LIST   = 0x10,
	TAG_STRING = 0x20,
	TAG_BOOL   = 0x30,
	TAG_RLINK  = 0x40,
	TAG_DLINK  = 0x41,
	TAG_FLOAT  = 0x50,
	TAG_DOUBLE = 0x51,
	TAG_UINT8  = 0x60,
	TAG_SINT8  = 0x61,
	TAG_UINT16 = 0x62,
	TAG_SINT16 = 0x63,
	TAG_UINT32 = 0x64,
	TAG_SINT32 = 0x65,
	TAG_UINT64 = 0x66,
	TAG_SINT64 = 0x67
};

class Parser {
public:
	Parser(const QByteArray &data, const DId &store)
	{
		m_data = data;
		m_offset = 0,
		m_size = data.size();
		m_store = store;
	}

	Value parse();

private:
	template <typename T>
	T getInt()
	{
		if (m_size < sizeof(T))
			throw ValueError();

		T tmp = qFromLittleEndian<T>((const uchar *)m_data.constData() + m_offset);
		m_offset += sizeof(T);
		m_size -= sizeof(T);
		return tmp;
	}

	QByteArray getBuffer(unsigned int len);

	Value parseDict(unsigned int len);
	Value parseList(unsigned int len);
	QString parseString();

	QByteArray m_data;
	unsigned int m_offset;
	unsigned int m_size;
	DId m_store;
};

template <>
qint8 Parser::getInt<qint8>()
{
	if (m_size < 1)
		throw ValueError();

	m_size--;
	return *((const qint8 *)m_data.constData() + m_offset++);
}

template <>
quint8 Parser::getInt<quint8>()
{
	if (m_size < 1)
		throw ValueError();

	m_size--;
	return *((const quint8 *)m_data.constData() + m_offset++);
}

QByteArray Parser::getBuffer(unsigned int len)
{
	if (m_size < len)
		throw ValueError();

	unsigned int off = m_offset;
	m_offset += len;
	m_size -= len;
	return m_data.mid(off, len);
}

Value Parser::parse()
{
	quint8 id = getInt<quint8>();

	switch (id) {
	case TAG_DICT:
		return parseDict(getInt<quint32>());
	case TAG_LIST:
		return parseList(getInt<quint32>());
	case TAG_STRING:
		return Value(parseString());
	case TAG_BOOL:
		return Value(!!getInt<quint8>());
	case TAG_RLINK:
	{
		quint8 len = getInt<quint8>();
		return Value(Link(m_store, RId(getBuffer(len))));
	}
	case TAG_DLINK:
	{
		quint8 len = getInt<quint8>();
		return Value(Link(m_store, DId(getBuffer(len)), false));
	}
	case TAG_FLOAT:
	{
		quint32 tmp = getInt<quint32>();
		return Value(*((float*)&tmp));
	}
	case TAG_DOUBLE:
	{
		quint64 tmp = getInt<quint64>();
		return Value(*((double*)&tmp));
	}
	case TAG_UINT8:
		return Value(getInt<quint8>());
	case TAG_SINT8:
		return Value(getInt<qint8>());
	case TAG_UINT16:
		return Value(getInt<quint16>());
	case TAG_SINT16:
		return Value(getInt<qint16>());
	case TAG_UINT32:
		return Value(getInt<quint32>());
	case TAG_SINT32:
		return Value(getInt<qint32>());
	case TAG_UINT64:
		return Value(getInt<quint64>());
	case TAG_SINT64:
		return Value(getInt<qint64>());
	default:
		throw ValueError();
	}
}

Value Parser::parseDict(unsigned int len)
{
	Value dict(Value::DICT);

	while (len--) {
		QString key = parseString();
		Value val = parse();
		dict[key] = val;
	}

	return dict;
}

Value Parser::parseList(unsigned int len)
{
	Value list(Value::LIST);

	while (len--)
		list.append(parse());

	return list;
}

QString Parser::parseString()
{
	unsigned int len = getInt<quint32>();
	if (m_size < len)
		throw ValueError();

	QString tmp = QString::fromUtf8(m_data.constData() + m_offset, len);
	m_offset += len;
	m_size -= len;
	return tmp;
}

/****************************************************************************/

ValueData::ValueData()
	: QSharedData()
{
}

ValueData::ValueData(Value::Type t)
	: QSharedData()
{
	type = t;

	switch (t) {
	case Value::NUL:
		break;
	case Value::INT:
		value.int64 = 0;
		break;
	case Value::UINT:
		value.uint64 = 0;
		break;
	case Value::FLOAT:
		value.real32 = 0;
		break;
	case Value::DOUBLE:
		value.real64 = 0;
		break;
	case Value::STRING:
		value.string = new QString();
		break;
	case Value::BOOL:
		value.boolean = false;
		break;
	case Value::LIST:
		value.list = new QList<Value>();
		break;
	case Value::DICT:
		value.dict = new QMap<QString, Value>();
		break;
	case Value::LINK:
		value.link = new Link();
		break;
	}
}

ValueData::ValueData(const ValueData &other)
	: QSharedData()
{
	type = other.type;

	switch (type) {
	case Value::NUL:
		break;
	case Value::INT:
		value.int64 = other.value.int64;
		break;
	case Value::UINT:
		value.uint64 = other.value.uint64;
		break;
	case Value::FLOAT:
		value.real32 = other.value.real32;
		break;
	case Value::DOUBLE:
		value.real64 = other.value.real64;
		break;
	case Value::STRING:
		value.string = new QString(*other.value.string);
		break;
	case Value::BOOL:
		value.boolean = other.value.boolean;
		break;
	case Value::LIST:
		value.list = new QList<Value>(*other.value.list);
		break;
	case Value::DICT:
		value.dict = new QMap<QString, Value>(*other.value.dict);
		break;
	case Value::LINK:
		value.link = new Link(*other.value.link);
		break;
	}
}

ValueData::~ValueData()
{
	switch (type) {
		case Value::STRING: delete value.string; break;
		case Value::LIST:   delete value.list; break;
		case Value::DICT:   delete value.dict; break;
		case Value::LINK:   delete value.link; break;
		default: break;
	}
}

void ValueData::convert(Value::Type t)
{
	if (type == t)
		return;

	if (type != Value::NUL)
		throw ValueError();

	type = t;

	switch (t) {
	case Value::STRING:
		value.string = new QString();
		break;
	case Value::LIST:
		value.list = new QList<Value>();
		break;
	case Value::DICT:
		value.dict = new QMap<QString, Value>();
		break;
	case Value::LINK:
		value.link = new Link();
		break;
	default:
		break;
	}
}

/****************************************************************************/

Value::Value(Type type)
{
	d = new ValueData(type);
}

Value::Value(int value)
{
	d = new ValueData(INT);
	d->value.int64 = value;
}

Value::Value(unsigned int value)
{
	d = new ValueData(UINT);
	d->value.uint64 = value;
}

Value::Value(qint64 value)
{
	d = new ValueData(INT);
	d->value.int64 = value;
}

Value::Value(quint64 value)
{
	d = new ValueData(UINT);
	d->value.uint64 = value;
}

Value::Value(float value)
{
	d = new ValueData(FLOAT);
	d->value.real32 = value;
}

Value::Value(double value)
{
	d = new ValueData(DOUBLE);
	d->value.real64 = value;
}

Value::Value(const QString &value)
{
	d = new ValueData(STRING);
	*(d->value.string) = value;
}

Value::Value(bool value)
{
	d = new ValueData(BOOL);
	d->value.boolean = value;
}

Value::Value(const Link &link)
{
	d = new ValueData(LINK);
	*(d->value.link) = link;
}

Value::Value(const Value &other)
	: d(other.d)
{
}

Value::~Value()
{
}

Value &Value::operator=(const Value &other)
{
	d = other.d;
	return *this;
}

Value::Type Value::type() const
{
	return d->type;
}

QString Value::asString() const
{
	if (d->type != STRING)
		throw ValueError();

	return *d->value.string;
}

int Value::asInt() const
{
	switch (d->type) {
		case INT:    return d->value.int64;
		case UINT:   return d->value.uint64;
		case FLOAT:  return d->value.real32;
		case DOUBLE: return d->value.real64;
		default:     throw ValueError();
	}
}

unsigned int Value::asUInt() const
{
	switch (d->type) {
		case INT:    return d->value.int64;
		case UINT:   return d->value.uint64;
		case FLOAT:  return d->value.real32;
		case DOUBLE: return d->value.real64;
		default:     throw ValueError();
	}
}

qint64 Value::asInt64() const
{
	switch (d->type) {
		case INT:    return d->value.int64;
		case UINT:   return d->value.uint64;
		case FLOAT:  return d->value.real32;
		case DOUBLE: return d->value.real64;
		default:     throw ValueError();
	}
}

quint64 Value::asUInt64() const
{
	switch (d->type) {
		case INT:    return d->value.int64;
		case UINT:   return d->value.uint64;
		case FLOAT:  return d->value.real32;
		case DOUBLE: return d->value.real64;
		default:     throw ValueError();
	}
}

float Value::asFloat() const
{
	switch (d->type) {
		case INT:    return d->value.int64;
		case UINT:   return d->value.uint64;
		case FLOAT:  return d->value.real32;
		case DOUBLE: return d->value.real64;
		default:     throw ValueError();
	}
}

double Value::asDouble() const
{
	switch (d->type) {
		case INT:    return d->value.int64;
		case UINT:   return d->value.uint64;
		case FLOAT:  return d->value.real32;
		case DOUBLE: return d->value.real64;
		default:     throw ValueError();
	}
}

bool Value::asBool() const
{
	switch (d->type) {
		case BOOL:   return d->value.boolean;
		case INT:    return !!d->value.int64;
		case UINT:   return !!d->value.uint64;
		case FLOAT:  return !!d->value.real32;
		case DOUBLE: return !!d->value.real64;
		default:     throw ValueError();
	}
}

Link Value::asLink() const
{
	if (d->type != LINK)
		throw ValueError();

	return *d->value.link;
}

int Value::size() const
{
	switch (d->type) {
		case LIST: return d->value.list->size();
		case DICT: return d->value.dict->size();
		default:   throw ValueError();
	}
}

Value &Value::operator[](int index)
{
	d->convert(LIST);
	return (*d->value.list)[index];
}

const Value& Value::operator[](int index) const
{
	if (d->type != LIST)
		throw ValueError();
	return (*d->value.list)[index];
}

void Value::append(const Value &value)
{
	d->convert(LIST);
	d->value.list->append(value);
}

void Value::remove(int index)
{
	d->convert(LIST);
	d->value.list->removeAt(index);
}

Value &Value::operator[](const QString &key)
{
	d->convert(DICT);
	return (*d->value.dict)[key];
}

const Value& Value::operator[](const QString &key) const
{
	if (d->type != DICT)
		throw ValueError();
	return (*d->value.dict)[key];
}

bool Value::contains(const QString &key) const
{
	if (d->type != DICT)
		throw ValueError();

	return d->value.dict->contains(key);
}

Value Value::get(const QString &key, const Value &defaultValue) const
{
	if (d->type != DICT)
		throw ValueError();

	return d->value.dict->value(key, defaultValue);
}

void Value::remove(const QString &key)
{
	d->convert(DICT);
	d->value.dict->remove(key);
}

QList<QString> Value::keys() const
{
	if (d->type != DICT)
		throw ValueError();

	return d->value.dict->keys();
}

Value Value::fromByteArray(const QByteArray &data, const DId &store)
{
	Parser p(data, store);
	return p.parse();
}

static QByteArray encodeQString(const QString &str, bool withTag)
{
	QByteArray res;
	QByteArray utf8 = str.toUtf8();
	unsigned int len = utf8.size();

	if (withTag) {
		res.resize(5);
		res[0] = TAG_STRING;
		qToLittleEndian<quint32>(len, (uchar*)res.data() + 1);
	} else {
		res.resize(4);
		qToLittleEndian<quint32>(len, (uchar*)res.data());
	}
	res.append(utf8);

	return res;
}

QByteArray Value::toByteArray() const
{
	QByteArray res;

	switch (d->type) {
		case NUL:
			throw ValueError();
		case INT:
			if (d->value.int64 < 0) {
				if (d->value.int64 >= (Q_INT64_C(-1) << 7)) {
					res.resize(2);
					res[0] = TAG_SINT8;
					res[1] = d->value.int64;
				} else if (d->value.int64 >= (Q_INT64_C(-1) << 15)) {
					res.resize(3);
					res[0] = TAG_SINT16;
					qToLittleEndian<qint16>(d->value.int64, (uchar*)res.data() + 1);
				} else if (d->value.int64 >= (Q_INT64_C(-1) << 31)) {
					res.resize(5);
					res[0] = TAG_SINT32;
					qToLittleEndian<qint32>(d->value.int64, (uchar*)res.data() + 1);
				} else {
					res.resize(9);
					res[0] = TAG_SINT64;
					qToLittleEndian<qint64>(d->value.int64, (uchar*)res.data() + 1);
				}

				break;
			}
			/* fall through and treat as uint if positive */
		case UINT:
			if (d->value.uint64 < (Q_INT64_C(1) << 8)) {
				res.resize(2);
				res[0] = TAG_UINT8;
				res[1] = d->value.uint64;
			} else if (d->value.uint64 < (Q_INT64_C(1) << 16)) {
				res.resize(3);
				res[0] = TAG_UINT16;
				qToLittleEndian<quint16>(d->value.uint64, (uchar*)res.data() + 1);
			} else if (d->value.uint64 < (Q_INT64_C(1) << 32)) {
				res.resize(5);
				res[0] = TAG_UINT32;
				qToLittleEndian<quint32>(d->value.uint64, (uchar*)res.data() + 1);
			} else {
				res.resize(9);
				res[0] = TAG_UINT64;
				qToLittleEndian<quint64>(d->value.uint64, (uchar*)res.data() + 1);
			}
			break;
		case FLOAT:
		{
			float tmp = d->value.real32;
			res.resize(5);
			res[0] = TAG_FLOAT;
			qToLittleEndian<quint32>(*((quint32*)&tmp), (uchar*)res.data() + 1);
			break;
		}
		case DOUBLE:
		{
			double tmp = d->value.real64;
			res.resize(9);
			res[0] = TAG_DOUBLE;
			qToLittleEndian<quint64>(*((quint64*)&tmp), (uchar*)res.data() + 1);
			break;
		}
		case STRING:
		{
			res = encodeQString(*d->value.string, true);
			break;
		}
		case BOOL:
			res.resize(2);
			res[0] = TAG_BOOL;
			res[1] = d->value.boolean;
			break;
		case LIST:
		{
			unsigned int i, len = d->value.list->size();

			res.resize(5);
			res[0] = TAG_LIST;
			qToLittleEndian<quint32>(len, (uchar*)res.data() + 1);
			for (i = 0; i < len; i++)
				res.append(d->value.list->at(i).toByteArray());

			break;
		}
		case DICT:
		{
			unsigned int len = d->value.dict->size();

			res.resize(5);
			res[0] = TAG_DICT;
			qToLittleEndian<quint32>(len, (uchar*)res.data() + 1);

			QMap<QString, Value>::const_iterator i = d->value.dict->constBegin();
			while (i != d->value.dict->constEnd()) {
				res.append(encodeQString(i.key(), false));
				res.append(i.value().toByteArray());
				i++;
			}
			break;
		}
		case LINK:
		{
			QByteArray link;

			res.resize(2);
			if (d->value.link->isDocHeadLink()) {
				res[0] = TAG_DLINK;
				link = d->value.link->doc().toByteArray();
			} else if (d->value.link->isRevLink()) {
				res[0] = TAG_RLINK;
				link = d->value.link->rev().toByteArray();
			} else
				throw ValueError();

			res[1] = link.size();
			res.append(link);

			break;
		}
	}

	return res;
}

/****************************************************************************/

QList<Link> Folder::lookup(const QString &path)
{
	WalkPathReq req;
	WalkPathCnf cnf;

	req.set_path(path.toUtf8().constData());
	int err = Connection::defaultRPC<WalkPathReq, WalkPathCnf>(WALK_PATH_MSG,
		req, cnf);
	if (err)
		return QList<Link>();

	QList<Link> result;
	for (int i = 0; i < cnf.items_size(); i++) {
		const WalkPathCnf_Item &item = cnf.items(i);

		DId store(item.store());
		if (item.has_doc())
			result.append(Link(store, DId(item.doc())));
		else if (item.has_rev())
			result.append(Link(store, RId(item.rev())));
	}

	return result;
}

Link Folder::lookupSingle(const QString &path)
{
	QList<Link> res = lookup(path);

	if (res.size() != 1)
		return Link();
	else
		return res.at(0);
}

/****************************************************************************/

FSTab::FSTab(bool autoReload, QObject *parent) : QObject(parent)
{
	reload = autoReload;
	file = NULL;
	QObject::connect(&watch, SIGNAL(modified(Link)), this, SLOT(fsTabModified(Link)));
}

FSTab::~FSTab()
{
	if (file)
		delete file;
}

void FSTab::fsTabModified(const Link & item)
{
	if (item.store() != file->link().store())
		return;

	if (reload)
		load();

	emit modified();
}

bool FSTab::load()
{
	if (!file) {
		// try to lookup fstab
		Link link = Folder::lookupSingle("sys:fstab");
		if (!link.isValid())
			return false;

		watch.addWatch(link);
		file = new Document(link);
	} else {
		// reset to HEAD
		Link link = file->link();
		if (!link.update())
			return false;
		file->setLink(link);
	}

	if (!file->peek())
		return false;

	try {
		QByteArray tmp;
		if (file->readAll(Part::PDSD, tmp) < 0) {
			file->close();
			return false;
		}
		fstab = Value::fromByteArray(tmp, file->link().store());
	} catch (ValueError&) {
		file->close();
		return false;
	}

	file->close();
	return true;
}

bool FSTab::save()
{
	if (!file || !file->update())
		return false;

	if (!file->writeAll(Part::PDSD, fstab.toByteArray())) {
		file->close();
		return false;
	}

	if (!file->commit()) {
		file->close();
		return false;
	}

	file->close();
	return true;
}

QList<QString> FSTab::knownLabels() const
{
	return fstab.keys();
}

bool FSTab::add(const QString &label, const QString &src, const QString &type,
                const QString &options, const QString &credentials)
{
	if (fstab.contains(label))
		return false;

	fstab[label]["src"] = src;
	if (type != "file")
		fstab[label]["type"] = type;
	if (!options.isEmpty())
		fstab[label]["options"] = options;
	if (!credentials.isEmpty())
		fstab[label]["credentials"] = options;

	return true;
}

bool FSTab::remove(const QString &label)
{
	if (fstab.contains(label))
		return false;

	fstab.remove(label);
	return true;
}

QString FSTab::src(const QString &label) const
{
	if (!fstab.contains(label))
		return QString();

	return fstab.get(label).get("src").asString();
}

QString FSTab::type(const QString &label) const
{
	if (!fstab.contains(label))
		return QString();

	return fstab.get(label).get("type", QString("file")).asString();
}

QString FSTab::options(const QString &label) const
{
	if (!fstab.contains(label))
		return QString();

	return fstab.get(label).get("options", QString("")).asString();
}

QString FSTab::credentials(const QString &label) const
{
	if (!fstab.contains(label))
		return QString();

	return fstab.get(label).get("credentials", QString("")).asString();
}

bool FSTab::autoMounted(const QString &label) const
{
	if (!fstab.contains(label))
		return false;

	return fstab.get(label).get("auto", false).asBool();
}

bool FSTab::setAutoMounted(const QString &label, bool enable)
{
	if (!fstab.contains(label))
		return false;

	fstab[label]["auto"] = enable;
	return true;
}

/****************************************************************************/

QMutex Registry::mutex;
Registry* volatile Registry::singleton = NULL;

Registry& Registry::instance()
{
	if (singleton)
		return *singleton;

	QMutexLocker locker(&mutex);

	if (!singleton)
		singleton = new Registry();

	return *singleton;
}

Registry::Registry()
	: QObject()
{
	QObject::connect(&watch, SIGNAL(modified(Link)), this, SLOT(modified(Link)));

	link = Folder::lookupSingle("sys:registry");
	if (!link.isValid())
		throw std::runtime_error("Registry not found");

	watch.addWatch(link);

	modified(link);
}

void Registry::modified(const Link &item)
{
	if (item.store() != link.store())
		return;

	Document file(item);
	if (!file.peek()) {
		qDebug() << "PeerDrive::Registry: cannot read registry!";
		return;
	}

	try {
		QByteArray tmp;
		if (file.readAll(Part::PDSD, tmp) >= 0)
			registry = Value::fromByteArray(tmp, item.store());
		else
			qDebug() << "PeerDrive::Registry: no data?";
	} catch (ValueError&) {
			qDebug() << "PeerDrive::Registry: invalid data!";
	}

	file.close();
}

Value Registry::search(const QString &uti, const QString &key, bool recursive,
                       const Value &defVal) const
{
	if (!registry.contains(uti))
		return defVal;

	const Value &item = registry[uti];
	if (item.contains(key))
		return item[key];
	else if (!recursive)
		return defVal;
	else if (!item.contains("conforming"))
		return defVal;

	// try to search recursive
	const Value &conforming = item["conforming"];
	for (int i = 0; i < conforming.size(); i++) {
		Value result = search(conforming[i].asString(), key);
		if (result.type() != Value::NUL)
			return result;
	}

	// nothing found
	return defVal;
}

bool Registry::conformes(const QString &uti, const QString &superClass) const
{
	if (uti == superClass)
		return true;

	if (!registry.contains(uti))
		return false;

	Value conforming = registry[uti].get("conforming", Value(Value::LIST));
	for (int i = 0; i < conforming.size(); i++)
		if (conformes(conforming[i].asString(), superClass))
			return true;

	return false;
}

QStringList Registry::executables(const QString &uti) const
{
	if (!registry.contains(uti))
		return QStringList();

	// list of executables for uti
	Value exec = registry[uti].get("exec", Value(Value::LIST));
	QStringList result;
	for (int i = 0; i < exec.size(); i++)
		result << exec[i].asString();

	// extend with all superclasses
	Value conforming = registry[uti].get("conforming", Value(Value::LIST));
	for (int i = 0; i < conforming.size(); i++)
		result.append(executables(conforming[i].asString()));

	result.removeDuplicates();
	return result;
}

QString Registry::icon(const QString &uti) const
{
	return search(uti, "icon", true, Value(QString("uti/unknown.png"))).asString();
}

}
