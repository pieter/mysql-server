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

// Protocol.h: interface for the Protocol class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PROTOCOL_H__84FD1986_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
#define AFX_PROTOCOL_H__84FD1986_A97F_11D2_AB5C_0000C01D2301__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "Socket.h"
#include "MsgType.h"

#define PORT				1858
#define PROTOCOL_VERSION_1	1
#define PROTOCOL_VERSION_2	2
#define PROTOCOL_VERSION_3	3
#define PROTOCOL_VERSION_4	4				
#define PROTOCOL_VERSION_5	5				
#define PROTOCOL_VERSION_6	6				
#define PROTOCOL_VERSION_7	7				
#define PROTOCOL_VERSION_8	8				
#define PROTOCOL_VERSION_9	9				
#define PROTOCOL_VERSION_10	10				
#define PROTOCOL_VERSION_11	11				
#define PROTOCOL_VERSION_12	12				
#define PROTOCOL_VERSION	PROTOCOL_VERSION_12

/*
 * History
 *
 *		Version 4	4/4/2001		Passing dates as QUADs w/ millisecond units
 *		Version 5	7/2/2001		Added CloseStatement, GetSequenceValue
 *		Version 6	1/20/2002		Added Connection.setTraceFlags
 *		Version 7	2/19/2002		Added Connect.SetAttribute, Connection.ServerOp
 *		Version 8	3/29/2002		Added GetHolderPrivileges, GetSequences
 *		Version 9	4/1/2003		Added AttachDebugger and DebugRequest
 *		Version 10	6/12/2003		Added Connection.getLimits/setLimits
 *		Version 11	10/4/2003		Added repository info to blobs and clobs
 *		Version 12	10/29/2003		Added Connection.deleteBlobData
 */

class Socket;
class DbResultSet;
class Value;
class SQLException;
class Database;
class BlobReference;

START_NAMESPACE

class Protocol : public Socket
{
public:
	Protocol();
	Protocol (socket_t sock, sockaddr_in *addr);
	virtual ~Protocol();

	void	getRepository (BlobReference *blob);
	void	putValues (Database *database, int count, Value *values);
	int32	createStatement();
	void	putMsg (MsgType type, int32 handle);
	void	getValue (Value *value);
	void	putValue (Database *database, Value *value);
	void	shutdown();
	void	sendFailure(SQLException *exception);
	void	sendSuccess();
	bool	getResponse();
	MsgType getMsg();
	void	putMsg (MsgType type);
	int32	openDatabase (const char *fileName);

	int		protocolVersion;
};
END_NAMESPACE

#endif // !defined(AFX_PROTOCOL_H__84FD1986_A97F_11D2_AB5C_0000C01D2301__INCLUDED_)
