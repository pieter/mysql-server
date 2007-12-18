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

// BERException.h: interface for the BERException class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BEREXCEPTION_H__FB8AE65B_D3CE_4E3C_BE29_B8572DCCB52E__INCLUDED_)
#define AFX_BEREXCEPTION_H__FB8AE65B_D3CE_4E3C_BE29_B8572DCCB52E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "JString.h"

class BERException  
{
public:
	BERException(const char *txt, ...);
	virtual ~BERException();

	JString text;
};

#endif // !defined(AFX_BEREXCEPTION_H__FB8AE65B_D3CE_4E3C_BE29_B8572DCCB52E__INCLUDED_)
