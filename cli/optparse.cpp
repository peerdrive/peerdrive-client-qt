/*
 * PeerDrive
 * Copyright (C) 2012  Jan Klötzke <jan DOT kloetzke AT freenet DOT de>
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

#include <QCoreApplication>
#include <QFileInfo>
#include <QtDebug>
#include <iostream>

#include "optparse.h"

/*****************************************************************************/

AbstractOption::AbstractOption(const QStringList &tags)
{
	m_tags = tags;
}

AbstractOption::~AbstractOption()
{
}

QString AbstractOption::help() const
{
	QString tagStr = "    " + helpJoinTags(m_tags);

	if (tagStr.length() > 25)
		tagStr += "\n" + QString(26, ' ');
	else
		tagStr = tagStr.leftJustified(26);

	return tagStr + m_help;
}

QString AbstractOption::helpJoinTags(const QStringList &tags) const
{
	return tags.join(", ");
}

void AbstractOption::setHelp(const QString &help)
{
	m_help = help;
}

int AbstractOption::tryParse(const QString &arg, const QStringList &remaining,
                             QMap<QString, QVariant> &options)
{
	QStringList::const_iterator i;

	for (i = m_tags.constBegin(); i != m_tags.constEnd(); i++) {
		if (arg.startsWith(*i)) {
			int consumed = match(*i, arg, remaining, options);
			if (consumed != TagDidntMatch)
				return consumed;
		}
	}

	return TagDidntMatch;
}

/*****************************************************************************/

class HelpOption : public AbstractOption
{
public:
	HelpOption(const OptionParser *parser);

protected:
	virtual int match(const QString &tag, const QString &match,
	                  const QStringList &remainingArgs,
	                  QMap<QString, QVariant> &options);

private:
	const OptionParser *m_parser;
};

HelpOption::HelpOption(const OptionParser *parser)
	: AbstractOption(QStringList() << "-h" << "--help")
{
	m_parser = parser;
	setHelp("show this help message and exit");
}

int HelpOption::match(const QString &tag, const QString &match,
                      const QStringList & /*remainingArgs*/,
	                  QMap<QString, QVariant> & /*options*/)
{
	if (tag.startsWith("--") && tag != match)
		return TagDidntMatch;

	m_parser->printHelp();
	return AbortParsing;
}

/*****************************************************************************/

Option::Option(const QString &dest, const QStringList &tags)
	: AbstractOption(tags)
{
	m_type = String;
	m_action = StoreArg;
	m_dest = dest;
}

Option::~Option()
{
}

QString Option::helpJoinTags(const QStringList &tags) const
{
	if (m_action != StoreArg)
		return AbstractOption::helpJoinTags(tags);

	QString var = m_dest.toUpper();

	QStringList tmp = tags;
	tmp.replaceInStrings(QRegExp("^(--.*)$"), "\\1=" + var);
	tmp.replaceInStrings(QRegExp("^(-.)$"), "\\1 " + var);

	return AbstractOption::helpJoinTags(tmp);
}

Option& Option::setHelp(const QString &help)
{
	AbstractOption::setHelp(help);
	return *this;
}

Option& Option::setActionStore(Type type)
{
	m_action = StoreArg;
	m_type = type;

	return *this;
}

Option& Option::setActionStoreConst(const QVariant &value)
{
	m_action = StoreConst;
	m_constant = value;

	return *this;
}

Option& Option::setActionStoreTrue()
{
	m_action = StoreConst;
	m_constant = QVariant(true);

	return *this;
}

Option& Option::setActionStoreFalse()
{
	m_action = StoreConst;
	m_constant = QVariant(false);

	return *this;
}

int Option::match(const QString &tag, const QString &match,
                  const QStringList &remaining, QMap<QString, QVariant> &options)
{
	switch (m_action) {
		case StoreArg:
		{
			int consumed = 1;
			QString rawValue;

			if (tag == match) {
				// separate argument
				if (!remaining.isEmpty())
					rawValue = remaining.at(0);
				consumed = 2;
			} else if (tag.startsWith("--")) {
				// long option: --tag=value
				if (match.at(tag.length()) != '=')
					return TagDidntMatch;
				rawValue = match.mid(tag.length()+1);
			} else {
				// short option: -tvalue
				rawValue = match.mid(tag.length());
			}

			if (rawValue.isEmpty()) {
				QString msg = "'%1' options requires an argument";
				throw ParserError(msg.arg(tag));
			}

			QVariant value;
			switch (m_type) {
				case String:
					value = QVariant(rawValue);
					break;
				case Int:
				{
					bool ok;
					value = QVariant(rawValue.toInt(&ok, 0));
					if (!ok) {
						QString msg = "invalid value for option '%1'";
						throw ParserError(msg.arg(tag));
					}
				}
			}

			options[m_dest] = value;

			return consumed;
		}

		case StoreConst:
			// long options cannot be merged
			if (tag.startsWith("--")) {
				if (tag != match)
					return TagDidntMatch; // must match completely
				options[m_dest] = m_constant;
				return 1;
			} else {
				options[m_dest] = m_constant;
				return TagMatched;
			}
	};
}

/*****************************************************************************/

OptionParser::OptionParser(const QString &usage, bool addHelp)
{
	helpUsage = usage;
	QString path = QCoreApplication::applicationFilePath();
	helpProgramName = QFileInfo(path).fileName();

	if (addHelp)
		addOption(new HelpOption(this));
}

OptionParser::~OptionParser()
{
	foreach (AbstractOption *i, m_options)
		delete i;
}

Option& OptionParser::addOption(const QString &dest, const QString &tag,
                                const QVariant &defaultValue)
{
	return addOption(dest, QStringList() << tag, defaultValue);
}

Option& OptionParser::addOption(const QString &dest, const QStringList &tags,
                                const QVariant &defaultValue)
{
	Option *opt = new Option(dest, tags);

	if (!defaultValue.isNull())
		defaultValues[dest] = defaultValue;

	m_options.append(opt);
	return *opt;
}

void OptionParser::addOption(AbstractOption *opt)
{
	m_options.append(opt);
}

void OptionParser::setDefault(const QString &dest, const QVariant &value)
{
	defaultValues[dest] = value;
}

void OptionParser::setProgramName(const QString &name)
{
	helpProgramName = name;
}

OptionParser::Result OptionParser::parseArgs()
{
	QStringList args = QCoreApplication::arguments();
	args.removeAt(0);
	return parseArgs(args);
}

OptionParser::Result OptionParser::parseArgs(const QStringList &args)
{
	int pos = 0;

	parsedOptions = defaultValues;
	parsedArguments.clear();

	try {
		while (pos < args.size()) {
			QStringList remaining = args.mid(pos);
			QString current = remaining.takeAt(0);

			// don't parse anything after --
			if (current == "--") {
				parsedArguments.append(remaining);
				break;
			}

			// is it an option?
			if (current.length() > 1 && current.startsWith("-")) {
				int consumed = parseOption(current, remaining);
				if (consumed == AbstractOption::AbortParsing)
					return Aborted;
				pos += consumed;
			} else {
				parsedArguments.append(current);
				pos++;
			}
		}
	} catch (ParserError &err) {
		printError(err.what());
		return Error;
	}

	return Ok;
}

const QStringList& OptionParser::arguments() const
{
	return parsedArguments;
}

const QMap<QString, QVariant>& OptionParser::options() const
{
	return parsedOptions;
}

int OptionParser::parseOption(QString tag, const QStringList &remaining)
{
	QList<AbstractOption*>::const_iterator i;

reparse:
	for (i = m_options.constBegin(); i != m_options.constEnd(); i++) {
		int consumed = (*i)->tryParse(tag, remaining, parsedOptions);

		if (consumed > 0)
			return consumed; // argument(s) has/have been consumed
		if (consumed == AbstractOption::TagDidntMatch)
			continue;	// didn't match
		if (consumed == AbstractOption::AbortParsing)
			return consumed;

		// short options without argument can be merged together, e.g.
		// -x -F => -xF
		if (tag.length() <= 2)
			return 1; // exhausted

		tag.remove(1, 1);
		goto reparse;
	}

	if (i == m_options.constEnd())
		throw(ParserError(QString("unknown option '%1'").arg(tag)));

	return 255; // not reached
}

void OptionParser::printError(const QString &error) const
{
	QString usage = helpUsage;
	std::cerr << qPrintable(usage.replace("$prog", helpProgramName)) << "\n\n";
	std::cerr << qPrintable(QString("%1: error: %2\n").arg(helpProgramName, error));
}

void OptionParser::printHelp() const
{
	QString usage = helpUsage;
	std::cout << qPrintable(usage.replace("$prog", helpProgramName)) << "\n\n";

	std::cout << "Options:\n";
	foreach (AbstractOption *opt, m_options)
		std::cout << qPrintable(opt->help()) << "\n";
}

