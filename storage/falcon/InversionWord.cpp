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

// InversionWord.cpp: implementation of the InversionWord class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "InversionWord.h"
#include "InversionPage.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

InversionWord::InversionWord()
{

}

InversionWord::~InversionWord()
{

}

int InversionWord::makeKey(UCHAR *key)
{
	// Downcase word, creating mask of uppercase characters

	UCHAR caseMask = 0;
	UCHAR *p = key;

	for (int n = 0; n < wordLength; ++n)
		{
		char c = word [n];
		if (ISUPPER (c))
			{
			caseMask |= 1 << n;
			c -= 'A' - 'a';
			}
		*p++ = c;
		}

	// Terminate the string portion of key, and add decompressed
	// tableId, fieldId, recordNumber, and word position

	*p++ = 0;
	Inv::encode (tableId, &p);
	Inv::encode (recordNumber, &p);
	Inv::encode (fieldId, &p);
	Inv::encode (wordNumber, &p);

	// Finally, add single byte of case mask unless all zeros

	if (caseMask)
		*p++ = caseMask;

	ASSERT (p - key <= MAX_INV_KEY);

	return (int) (p - key);
}


bool InversionWord::isEqual(InversionWord *word2)
{
	if (wordNumber != word2->wordNumber ||
		wordLength != word2->wordLength ||
		tableId != word2->tableId ||
		fieldId != word2->fieldId ||
		recordNumber != word2->recordNumber)
		return false;

	const char *end = word + wordLength;

	for (const char *p = word, *q = word2->word; p < end;)
		if (*p++ != *q++)
			return false;

	return true;
}
