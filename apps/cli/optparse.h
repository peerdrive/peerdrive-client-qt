/*
 * PeerDrive
 * Copyright (C) 2013  Jan Klötzke <jan DOT kloetzke AT freenet DOT de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CLI_OPTPARSE_H
#define CLI_OPTPARSE_H

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

class OptionParser;

class ParserError
{
public:
	ParserError(const QString &what) { msg = what; }
	QString what() const { return msg; }
private:
	QString msg;
};

class AbstractOption
{
public:
	AbstractOption(const QStringList &tags);
	virtual ~AbstractOption();

	QString help() const;
	void setHelp(const QString &help);

	enum ParseStatus {
		TagMatched = 0,
		TagDidntMatch = -1,
		AbortParsing = -2,
	};

	int tryParse(const QString &tag, const QStringList &remaining,
	             QMap<QString, QVariant> &options);

protected:
	virtual QString helpJoinTags(const QStringList &tags) const;
	virtual int match(const QString &tag, const QString &match,
	                  const QStringList &remainingArgs,
	                  QMap<QString, QVariant> &options) = 0;

private:
	QString m_help;
	QStringList m_tags;
};

class Option : public AbstractOption
{
public:
	Option(const QString &dest, const QStringList &tags);
	virtual ~Option();

	enum Type {
		String,
		Int
	};

	Option& setHelp(const QString &help);

	Option& setActionStore(Type type = String);
	Option& setActionStoreConst(const QVariant &value);
	Option& setActionStoreTrue();
	Option& setActionStoreFalse();
	Option& setActionCounter();

protected:
	virtual QString helpJoinTags(const QStringList &tags) const;
	virtual int match(const QString &tag, const QString &match,
	                  const QStringList &remainingArgs,
	                  QMap<QString, QVariant> &options);

private:
	enum Action {
		StoreArg,
		StoreConst,
		Counter
	};

	Type m_type;
	Action m_action;
	QString m_dest;
	QVariant m_constant;
};

class OptionParser
{
public:
	OptionParser(const QString &usage = QString("$prog [options]"),
	             bool addHelp = true);
	~OptionParser();

	Option& addOption(const QString &dest, const QString &tag,
	                  const QVariant &defaultValue = QVariant());
	Option& addOption(const QString &dest, const QStringList &tags,
	                  const QVariant &defaultValue = QVariant());
	void addOption(AbstractOption *option);

	void setDefault(const QString &dest, const QVariant &value);
	void setProgramName(const QString &name);
	void setDescription(const QString &description);

	enum Result {
		Ok,
		Aborted,
		Error,
	};

	Result parseArgs();
	Result parseArgs(const QStringList &args);

	const QStringList& arguments() const;
	const QMap<QString, QVariant>& options() const;

	void printError(const QString &error) const;
	void printHelp() const;

private:
	int parseOption(QString tag, const QStringList &remaining);

	QMap<QString, QVariant> defaultValues, parsedOptions;
	QStringList parsedArguments;
	QList<AbstractOption*> m_options;

	QString helpProgramName;
	QString helpUsage;
	QString helpDescription;
};

#endif
