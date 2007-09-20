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

// ArgsException.h: interface for the ArgsException class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ARGSEXCEPTION_H__0546C1B7_E895_4AC1_A951_AF1812A505B0__INCLUDED_)
#define AFX_ARGSEXCEPTION_H__0546C1B7_E895_4AC1_A951_AF1812A505B0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "JString.h"

class ArgsException  
{
public:
	const char* getText();
	ArgsException(const char * txt, ...);
	virtual ~ArgsException();

	JString		text;
};

#endif // !defined(AFX_ARGSEXCEPTION_H__0546C1B7_E895_4AC1_A951_AF1812A505B0__INCLUDED_)
