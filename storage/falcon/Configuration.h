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

// Configuration.h: interface for the Configuration class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CONFIGURATION_H__FE192389_82EE_4E37_BA07_19A71BCFF487__INCLUDED_)
#define AFX_CONFIGURATION_H__FE192389_82EE_4E37_BA07_19A71BCFF487__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Configuration  
{
public:
	Configuration(const char *fileName);
	virtual ~Configuration();

	bool		enabled(JString string);
	void		release();
	void		addRef();
	int64		getMemorySize(const char *string);
	uint64		getPhysicalMemory(uint64 *available = NULL, uint64 *total = NULL);
	bool		getLine(void *file, int length, char *line);
	void		setRecordScavengeThreshold(int threshold);
	void		setRecordScavengeFloor(int floor);
	void		setRecordMemoryMax(uint64 value);

	JString		classpath;
	JString		gcSchedule;
	JString		scavengeSchedule;
	JString		checkpointSchedule;
	uint64		recordMemoryMax;
	uint64		recordScavengeThreshold;
	uint64		recordScavengeFloor;
	int			recordScavengeThresholdPct;
	int			recordScavengeFloorPct;
	uint64		initialAllocation;
	uint64		allocationExtent;
	bool		disableFsync;
	uint64		pageCacheSize;
	int64		javaInitialAllocation;
	int64		javaSecondaryAllocation;
	uint		serialLogWindows;
	JString		serialLogDir;
	uint		indexChillThreshold;
	uint		recordChillThreshold;
	int			useCount;
	int			maxThreads;
	uint		maxTransactionBacklog;
	bool		schedulerEnabled;
};

#endif // !defined(AFX_CONFIGURATION_H__FE192389_82EE_4E37_BA07_19A71BCFF487__INCLUDED_)
