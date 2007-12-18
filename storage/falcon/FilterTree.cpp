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

// FilterTree.cpp: implementation of the FilterTree class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "FilterTree.h"
#include "InversionWord.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FilterTree::FilterTree(InversionFilter *fltr1, InversionFilter *fltr2)
{
	filter1 = fltr1;
	filter2 = fltr2;
}

FilterTree::~FilterTree()
{
	delete filter1;
	delete filter2;
}

void FilterTree::start()
{
	filter1->start();
	filter2->start();
	word1 = filter1->getWord();
	word2 = filter2->getWord();
}

InversionWord* FilterTree::getWord()
{
	InversionWord *word;

	if (!word1)
		{
		if ((word = word2))
			word2 = filter2->getWord();
		}
	else if (!word2 || word1->wordNumber < word2->wordNumber)
		{
		word = word1;
		word1 = filter1->getWord();
		}
	else
		{
		word = word2;
		word2 = filter2->getWord();
		}

	return word;
}
