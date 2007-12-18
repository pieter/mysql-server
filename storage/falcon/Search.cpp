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

// Search.cpp: implementation of the Search class.
//
//////////////////////////////////////////////////////////////////////

#include "stdio.h"
#include "Engine.h"
#include "Search.h"
#include "SearchHit.h"
#include "Inversion.h"
#include "Scan.h"
#include "ResultList.h"
#include "LinkedList.h"
#include "Database.h"
#include "SearchWords.h"


#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#define WHITE		1
#define PUNCTUATION	2
#define DELIMITER	4
#define ALPHA		8
#define JUNK		16

//#define STUFF(number,ptr,shift)	if (n = number >> shift) *ptr++ = n | 0x80

static char characters [256];

static const char *white			= " \t\n";
static const char *delimiters		= " \t.,?:;!<>(){}[]!&'\"/";
static const char *junk			= " \t.,?:;!<>(){}[]!&'/";
static const char *punctuation	= "+-\"";

static int init();
static int foo = init();

int init()
{
	if (!characters [(int) ' '])
		{
		const char *p;
		
		for (p = white; *p;)
			characters [(int) *p++] |= WHITE;
			
		for (p = punctuation; *p;)
			characters [(int) *p++] |= PUNCTUATION;
			
		for (p = delimiters; *p;)
			characters [(int) *p++] |= DELIMITER;
			
		for (p = junk; *p;)
			characters [(int) *p++] |= JUNK;
			
		int n;
		
		for (n = 'A'; n <= 'Z'; ++n)
			characters [n] |= ALPHA;
			
		for (n = 'a'; n <= 'z'; ++n)
			characters [n] |= ALPHA;
			
		for (n = '0'; n <= '9'; ++n)
			characters [n] |= ALPHA;
		}

	return 0;
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Search::Search(const char *string, SearchWords *words)
{
	searchWords = words;
	LinkedList scanObjects;
	char word [256];
	ScanType type = Simple;
	Scan *scanObject;
		
	for (const char *ptr = string; getToken (&ptr, word);)
		switch (word [0])
			{
			case '+':
				type = Required;
				break;

			case '-':
				type = Excluded;
				break;

			case '"':
				if ( (scanObject = parsePhrase (type, &ptr)) )
					scanObjects.append (scanObject);
				break;

			case '*':
				type = Simple;
				break;

			default:
				scanObject = new Scan (type, word, searchWords);
				scanObjects.append (scanObject);
				type = Simple;
			}

	count = scanObjects.count();
	scans = new Scan* [count];
	int n = 0;
	valid = false;

	FOR_OBJECTS (Scan*, scan, &scanObjects)
		switch (scan->type)
			{
			case Simple:
			case Required:
			//case Phrase:
			case Tail:
				if (!scan->frequentWord)
					valid = true;
				break;
			
			default:
				break;
			}
			
		scans [n++] = scan;
	END_FOR;
}

Search::~Search()
{
	if (scans)
		{
		for (int n = 0; n < count; ++n)
			delete scans [n];
		delete [] scans;
		}
}

void Search::search(ResultList *resultList)
{
	if (!valid)
		return;

	SearchHit *hit = NULL;
	int32 hitNumber = 1;
	int32 lastField = 0;
	int32 lastWord = 0;
	int wordMask = 0;
    int n;

	for (n = 0; n < count; ++n)
		{
		Scan *scan = scans [n];
		scan->start (resultList->database);
		while (!scan->eof && !scan->frequentTail && !scan->validate (NULL))
			scan->getNext();
		}

	int32	temp [4];
	int32	*current = NULL;
	bool	eof = false;
	bool	streamsActive;
	int32	*required = NULL;

	for (;;)
		{

		// Cycle through various streams making sure required items
		// are satisfied and at least one optional guy matches

		for (bool again = true; again && !eof;)
			{
			again = false;
			streamsActive = false;
			for (int n = 0; n < count; ++n)
				{
				Scan *scan = scans [n];
				if (scan->frequentTail)
					continue;
				if (scan->type == Required)
					{
					if (scan->eof)
						{
						eof = true;
						break;
						}
					streamsActive = true;
					if (!required)
						required = scan->numbers;
					else
						{
						while (scan->compare (2, required) < 0)
							{
							if (!scan->getNextValid ())
								{
								eof = true;
								break;
								}
							again = true;
							}
						int ret = scan->compare (2, required);
						if (ret)
							{
							if (ret > 0)
								required = scan->numbers;
							again = true;
							}
						}
					if (!current)
						current = required;
					}
				else if (scan->eof)
					continue;
				else if (scan->type == Simple)
					{
					streamsActive = true;
					if (current)
						{
						if (required)
							{
							while (scan->compare (2, required) < 0)
								if (!scan->getNextValid ())
									break;
							}
						if (scan->compare (2, current) < 0)
							current = scan->numbers;
						}
					else
						current = scan->numbers;
					}
				else if (scan->type == Excluded)
					continue;
				else
					ASSERT (false);
				}
			}

		// If we hit the end, we're obviously done

		if (eof || !current || !streamsActive)
			break;

		// If there was anything deferred as all frequent words, check now

		bool reject = false;

		for (n = 0; n < count; ++n)
			{
			Scan *scan = scans [n];
			if (scan->type == Required && scan->frequentTail &&
				!scan->validateWord (current))
				{
				reject = true;
				break;
				}
			}

		// Now reject anything explicitly excluded

		Scan *excluded = NULL;

		if (!reject)
			for (n = 0; n < count; ++n)
				{
				Scan *scan = scans [n];
				if (scan->type == Excluded)
					{
					if (scan->frequentTail)
						{
						if (scan->validateWord (current))
							{
							reject = true;
							break;
							}
						continue;
						}
					while (scan->compare (2, current) < 0)
						if (!scan->getNextValid ())
							break;
					if (scan->eof)
						continue;
					if (excluded)
						{
						if (scan->compare (2, excluded->numbers) < 0)
							excluded = scan;
						}
					else
						excluded = scan;
					}
				}

		// Unless we're excluded, we've got a hit

		if (!reject && (
			!excluded || excluded->compare (2, current) != 0))
			{
			int distance = current [3];

			if (hit && hit->tableId == current [0] && 
					   hit->recordNumber == current [1])
				{
				if (current [2] == lastField)
					{
					++hitNumber;
					distance -= lastWord;
					}
				else
					{
					hitNumber = 1;
					lastField = current [2];
					}
				}
			else
				{
				hit = resultList->add (current [0], current [1], current [2], 0);
				hitNumber = 1;
				lastField = current [2];
				wordMask = 0;
				}

			if (hit)
				{
				for (n = 0; n < count; ++n)
					{
					Scan *scan = scans [n];
					if (scan->type != Excluded &&
						 scan->numbers[0] == current[0] &&
						 scan->numbers[1] == current [1] &&
						 scan->numbers[2] == current [2])
						wordMask |= 1 << n;
					}

				//double score = (double) hitNumber / (distance + 1);
				double score = (double) 1 / (distance + 1);
				hit->score += score;
				hit->setWordMask(wordMask);
				++hit->hits;
				lastWord = current[3];
				}
			}

		temp [0] = current [0];
		temp [1] = current [1];
		temp [2] = current [2];
		temp [3] = current [3] + 1;
		current = NULL;

		for (int n = 0; n < count; ++n)
			{
			Scan *scan = scans [n];
			while (!scan->eof && !scan->frequentTail &&
				   scan->compare (4, temp) < 0)
				scan->getNextValid ();
			}

		}

	//resultList->sort();
}

bool Search::getToken(const char * * ptr, char * token)
{
	// skip white space
	
	char *t = token;
	char c;
	const char *p = *ptr;

	for (;;)
		{
		while (characters [(int) *p] & JUNK)
			++p;

		if (!(c = *p++))
			return false;

		*t++ = c;

		// if this is punctuation, return single character

		char type = characters [(int) c];

		if (type & ALPHA)
			while ((c = *p) && !(characters [(int) c] & DELIMITER))
				*t++ = *p++;
		else if (!(type & PUNCTUATION))
			continue;

		*t = 0;
		*ptr = p;

		return true;
		}
}

Scan* Search::parsePhrase(ScanType type, const char * * ptr)
{
	char word [256];

	if (!getToken (ptr, word))
		return NULL;

	Scan *scan = new Scan (type, word, searchWords);

	while (getToken (ptr, word))
		if (word [0] == '"')
			break;
		else if (word [0] != '*')
			scan->addWord (word);

	return scan;
}

