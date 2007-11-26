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

// Hdr.h: interface for the Hdr class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_HDR_H__6A019C21_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
#define AFX_HDR_H__6A019C21_A340_11D2_AB5A_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Page.h"
#include "HdrState.h"

enum HdrVariable {
	hdrEnd = 0,
	hdrRepositoryName,
	hdrLogPrefix,
	};

class Dbb;

class Hdr : public Page  
{
public:
	Hdr();
	~Hdr();

	void		putHeaderVariable (Dbb *dbb, HdrVariable variable, int size, const char *buffer);
	int			getHeaderVariable (Dbb *dbb, HdrVariable variable, int bufferSize, char *buffer);
	static void create (Dbb *dbb, FileType fileType, TransId transId, const char *logRoot);

	short		odsVersion;
	int32		pageSize;
	int32		inversion;
	int32		sequence;
	HdrState	state;
	int32		sequenceSectionId;
	short		fileType;
	short		odsMinorVersion;
	int32		volumeNumber;
	uint32		creationTime;
	UCHAR		utf8;
	uint32		logOffset;
	uint32		logLength;
	UCHAR		haveIndexVersionNumber;
	UCHAR		defaultIndexVersionNumber;
	UCHAR		sequenceSectionFixed;
	int32		tableSpaceSectionId;
	uint32		serialLogBlockSize;
};

class HdrV2 : public Page  
{
public:
	short		odsVersion;
	unsigned short	pageSize;
	int32		inversion;
	int32		sequence;
	HdrState	state;
	int32		sequenceSectionId;
	short		fileType;
	short		odsMinorVersion;
	int32		volumeNumber;
	uint32		creationTime;
	UCHAR		utf8;
	uint32		logOffset;
	uint32		logLength;
	UCHAR		haveIndexVersionNumber;
	UCHAR		defaultIndexVersionNumber;
};

#endif // !defined(AFX_HDR_H__6A019C21_A340_11D2_AB5A_0000C01D2301__INCLUDED_)
