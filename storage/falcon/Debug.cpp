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

// Debug.cpp: implementation of the Debug class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Engine.h"
#include "Debug.h"
#include "IndexPage.h"
#include "SectionPage.h"
#include "Dbb.h"
#include "BDB.h"
#include "InversionPage.h"
#include "IndexNode.h"
#include "Log.h"
#include "SQLError.h"
#include "Database.h"
#include "RecordLocatorPage.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#define WHITE(c)		(c == ' ' || c == '\n')

struct Commands {
	const char*		string;
	DebugCommand	command;
	};

static const Commands commands [] =
	{
	"quit",		Quit,
	"dump",		Dump,
	"next",		Left,
	"prior",	Right,
	"read",		Read,
	"up",		Up,
	"help",		Help,
	"throw",	Throw,
	"break",	Break,
	NULL,		Break
	};


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Debug::Debug(Dbb *database, Page *sourcePage)
{
	dbb = database;
	++dbb->database->noSchedule;
	bdb = NULL;
	page = sourcePage;
}

Debug::~Debug()
{
	if (bdb)
		bdb->release(REL_HISTORY);

	--dbb->database->noSchedule;
}

void Debug::interpreter()
{
	printf ("\nNetfrastructure Page Walker\n");

	if (page)
		dump(page);

	for (;;)
		{
		char buffer [256];
		printf ("dbg> ");
		if (!fgets(buffer, sizeof(buffer), stdin))
			return;

		char *p = strchr(buffer, '\n');
		if (p)
			*p = 0;

		if (execute(buffer))
			break;
		}
}

void Debug::fetch(int pageNumber)
{
	if (pageNumber <= 0)
		{
		printf ("invalid page number %d\n", pageNumber);
		return;
		}

	if (bdb)
		bdb->release(REL_HISTORY);

	bdb = dbb->fetchPage(pageNumber, PAGE_any, Exclusive);
	BDB_HISTORY(bdb);
	page = bdb->buffer;
	dump(bdb->buffer);
}

const char* Debug::match(const char *string, const char *token)
{
	if (*string++ != *token++)
		return NULL;

	for (; *string == *token && *string; ++string, ++token)
		;

	if (!*string)
		return string;

	if (*string != ' ')
		return NULL;

	while (WHITE(*string))
		++string;

	return string;
}

void Debug::dump(Page *page)
{
	switch (page->pageType)
		{
		case PAGE_btree:
			IndexPage::printPage((IndexPage*)page, (bdb)? bdb->pageNumber : 0, false);
			break;

		case PAGE_sections:			// 2
		//case PAGE_section:			// 3
		case PAGE_record_locator:	// 4
		//case PAGE_btree_leaf:		// 6
		case PAGE_data:				// 7
		case PAGE_inventory:		// 8
		case PAGE_data_overflow:	// 9
		case PAGE_inversion:		// 10
		case PAGE_free:				// 11 Page has been released
			printf ("page type (%d) not handled yet\n", page->pageType);
		}
}

void Debug::execute(DebugCommand command, const char *string)
{
	switch (command)
		{
		case Dump:
			dump (page);
			return;

		case Throw:
			throw SQLError (DEBUG_ERROR, "bailing out");

		case Read:
			fetch(atoi(string));
			return;

		case Break:
			breakpoint();
			return;
		
		default:
			break;
		}

	switch (page->pageType)
		{
		case PAGE_btree:
			execute ((IndexPage*) page, command, string);
			break;

		case PAGE_sections:			// 2
		//case PAGE_section:		// 3
		case PAGE_record_locator:	// 4
		//case PAGE_btree_leaf:		// 6
		case PAGE_data:				// 7
		case PAGE_inventory:		// 8
		case PAGE_data_overflow:	// 9
		case PAGE_inversion:		// 10
		case PAGE_free:				// 11 Page has been released
			printf ("page type (%d) not handled yet\n", page->pageType);
		}
}

void Debug::execute(IndexPage *indexPage, DebugCommand command, const char *string)
{
	switch (command)
		{
		case Right:
			fetch(indexPage->nextPage);
			break;

		case Left:
			fetch(indexPage->priorPage);
			break;

		case Up:
			fetch(indexPage->parentPage);
			break;

		default:
			break;
		}


}

bool Debug::execute(const char *buffer)
{
	for (const Commands *command = commands; command->string; ++command)
		{
		const char *tail = match(buffer, command->string);
		if (tail)
			{
			if (command->command == Quit)
				return true;
			else
				execute(command->command, tail);

			return false;
			}
		}


	if (*buffer)
		printf ("don't recognize command \"%s\"\n", buffer);

	return false;
}

void Debug::breakpoint()
{

}

int Debug::getPageId(Page* page)
{
	switch (page->pageType)
		{
		case PAGE_sections:
			return ((SectionPage*) page)->section;

		case PAGE_record_locator:
			return ((RecordLocatorPage*) page)->section;

		default:
			return 0;
		}
}
