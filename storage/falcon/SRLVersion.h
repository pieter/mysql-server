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

// SRLVersion.h: interface for the SRLVersion class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SRLVERSION_H__E60E0DD2_7FDC_40EF_BC8B_7B30DAB5744F__INCLUDED_)
#define AFX_SRLVERSION_H__E60E0DD2_7FDC_40EF_BC8B_7B30DAB5744F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "SerialLogRecord.h"

static const int srlVersion0		= 0;
static const int srlVersion1		= 1;	// Added length and data to SRLIndexPage
static const int srlVersion2		= 2;	// Added transactionId to SRLCreateIndex
static const int srlVersion3		= 3;	// Added transactionId to SRLDropTable
static const int srlVersion4		= 4;	// Added transactionId to SRLDeleteIndex
static const int srlVersion5		= 5;	// Added locatorPageNumber to SRLDataPage
static const int srlVersion6		= 6;	// Added index version number of SRLIndexUpdate, SRLUpdateIndex, and SRLDeleteIndex
static const int srlVersion7		= 7;	// Added xid to SRLPrepare	1/20/07
static const int srlVersion8		= 8;	// Adding tableSpaceId to many classes	June 5, 2007 (Ann's birthday!)
static const int srlVersion9		= 9;	// Added block number for checkpoint operation	July 9, 2007
static const int srlVersion10		= 10;	// Added transaction id for drop table space	July 9, 2007
static const int srlVersion11		= 11;	// Added table space type (repository support)	December 4, 2007
static const int srlVersion12		= 12;	// Added index version number to SRLIndexPage	February 13, 2008
static const int srlCurrentVersion	= srlVersion12;

class SRLVersion : public SerialLogRecord  
{
public:
	void read();
	SRLVersion();
	virtual ~SRLVersion();

	int version;
};

#endif // !defined(AFX_SRLVERSION_H__E60E0DD2_7FDC_40EF_BC8B_7B30DAB5744F__INCLUDED_)
