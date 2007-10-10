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

// FilterDifferences.cpp: implementation of the FilterDifferences class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "FilterDifferences.h"
#include "InversionWord.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FilterDifferences::FilterDifferences(InversionFilter *leave, InversionFilter *stay)
{
	leaving = leave;
	staying = stay;
}

FilterDifferences::~FilterDifferences()
{
	delete leaving;
	delete staying;
}

void FilterDifferences::start()
{
	staying->start();
	leaving->start();
	nextWord = staying->getWord();
}

InversionWord* FilterDifferences::getWord()
{
	for (InversionWord *word; (word = leaving->getWord());)
		{
		while (nextWord && nextWord->wordNumber < word->wordNumber)
			nextWord = staying->getWord();
		if (nextWord && !word->isEqual (nextWord))
			return word;
		}

	return NULL;
}
