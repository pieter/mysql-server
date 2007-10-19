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

// Filter.cpp: implementation of the Filter class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "Filter.h"
#include "InversionPage.h"
#include "Value.h"

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

#define WHITE		1
#define PUNCTUATION	2
#define DELIMITER	4

//#define STUFF(number,ptr,shift)	if (n = number >> shift) *ptr++ = n | 0x80

static char characters [256];

static const char *white		 = " \t\n";
static const char *delimiters = " \t\n.,?:;!<>(){}[]+!&'\"/";
static const char *punctuation = ".,?:;!<>(){}[]+-!&'\"/";

static int init();
static int foo = init();

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

int init()
{
	if (!characters [(int) ' '])
		{
		const char*p;
		for (p = white; *p;)
			characters [(int) *p++] |= WHITE;
		for (p = punctuation; *p;)
			characters [(int) *p++] |= PUNCTUATION;
		for (p = delimiters; *p;)
			characters [(int) *p++] |= DELIMITER;
		}

	return 0;
}

Filter::Filter(int tableId, int fieldId, int recordNumber, Value *value)
{
	html = false;
	value->getStream (&stream, true);
	word.tableId = tableId;
	word.fieldId = fieldId;
	word.recordNumber = recordNumber;
}

Filter::~Filter()
{

}

int Filter::getWord(int bufferLength, char * buffer)
{
	if (!segment)
		return 0;

	// Skip any white space, punctuation, or html (if appropriate)

	int state = 0;

	for (;; ++p)
		{
		if (p >= endSegment && !nextSegment())
			return 0;
		char type = characters [(int) *p];
		if (state == 0)
			{
			if (html && *p == '<')
				state = 1;
			else if (!(type & (WHITE | PUNCTUATION)))
				break;
			}
		else if (state == 1)
			{
			if (*p == '>')
				state = 0;
			else if (*p == '"')
				state = 2;
			}
		else if (*p == '"')
			state = 1;
		}
	
	// Pick up word

	char *q = buffer;
	char *end = buffer + bufferLength - 1;

	while (q < end)
		{
		if (p >= endSegment && !nextSegment())
			break;
		char c = *p++;
		if (characters [(int) c] & (WHITE | DELIMITER))
			break;
		*q++ = c;
		}

	// Trim trailing punctuation

	while (q > buffer)
		if (characters [(int) q [-1]] & PUNCTUATION)
			--q;
		else
			break;

	*q = 0;

	return q - buffer;
}

void Filter::start()
{
	wordNumber = 0;

	if ((segment = stream.segments))
		{
		p = segment->address;
		endSegment = p + segment->length;

		// Skip over any white space.  If first non-white is "<", filter html

		for (;; ++p)
			{
			if (p >= endSegment && !nextSegment())
				return;
			char type = characters [(int) *p];
			if (!(type & (WHITE)))
				break;
			}

		html = *p == '<';
		}
}

const char* Filter::nextSegment()
{
	if (!(segment = segment->next))
		return NULL;

	p = segment->address;
	endSegment = p + segment->length;

	return p;
}

InversionWord* Filter::getWord()
{
	if ( (word.wordLength = getWord (MAX_INV_WORD, word.word)) )
		{
		word.wordNumber = wordNumber++;
		return &word;
		}

	return NULL;
}
