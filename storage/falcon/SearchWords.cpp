/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// SearchWords.cpp: implementation of the SearchWords class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "SearchWords.h"
#include "Database.h"
#include "Table.h"
#include "SymbolManager.h"
#include "PreparedStatement.h"
#include "ResultSet.h"
#include "ValueEx.h"
#include "RecordVersion.h"

static const char *words [] = {
	"the",
	"a",
	"an",
	"and",
	"or",
	"of",
	"from",
	"to",
	"in",
	"this",
	"that",
	"is",
	"are",
	NULL
	};

static const char *ddl [] = {
	"upgrade table system.stop_words (\n"
		"word varchar (20) not null primary key collation case_insensitive)",
	"grant all on system.stop_words to public",
	NULL
	};

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SearchWords::SearchWords(Database *db) : TableAttachment (POST_COMMIT)
{
	database = db;
	symbols = new SymbolManager;

	for (const char **word = words; *word; ++word)
		symbols->getString (*word);
}

SearchWords::~SearchWords()
{
	delete symbols;
}

void SearchWords::tableAdded(Table *table)
{
	if (table->isNamed ("SYSTEM", "STOP_WORDS"))
		table->addAttachment (this);
}

void SearchWords::initialize()
{
	if (!database->findTable ("SYSTEM", "STOP_WORDS"))
		for (const char **p = ddl; *p; ++p)
			database->execute (*p);

	PreparedStatement *statement = database->prepareStatement (
		"select word from system.stop_words");
	ResultSet *resultSet = statement->executeQuery();

	while (resultSet->next())
		symbols->getString (resultSet->getString (1));

	resultSet->close();
	statement->close();
}

bool SearchWords::isStopWord(const char *word)
{
	return symbols->findString (word) != NULL;
}

JString SearchWords::getWord(Table *table, Record *record)
{
	int wordId = table->getFieldId ("WORD");
	ValueEx value (record, wordId);

	return value.getString ();
}

void SearchWords::postCommit(Table *table, RecordVersion *record)
{
	JString word = getWord (table, record);
	symbols->getString (word);
}
