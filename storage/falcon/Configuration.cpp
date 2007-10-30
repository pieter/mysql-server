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

// Configuration.cpp: implementation of the Configuration class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "Engine.h"
#include "Configuration.h"
#include "MemoryManager.h"
#include "SQLError.h"
#include "Log.h"
#include "IOx.h"
#include "ScanDir.h"

#ifdef STORAGE_ENGINE
#define CONFIG_FILE	"falcon.conf"

#define PARAMETER(name, text, min, default, max, flags, function) extern uint falcon_##name;
#include "StorageParameters.h"
#undef PARAMETER

extern uint64		falcon_record_memory_max;
extern uint			falcon_record_scavenge_threshold;
extern uint			falcon_record_scavenge_floor;
extern uint64		falcon_initial_allocation;
extern uint			falcon_allocation_extent;
extern uint			falcon_page_size;
extern uint64		falcon_page_cache_size;
//extern uint		falcon_debug_mask;
extern uint			falcon_serial_log_buffers;
extern uint			falcon_index_chill_threshold;
extern uint			falcon_record_chill_threshold;
extern uint			falcon_max_transaction_backlog;
extern char*		falcon_checkpoint_schedule;
extern char*		falcon_scavenge_schedule;
extern char*		falcon_serial_log_dir;
#else
#define CONFIG_FILE	"netfraserver.conf"
#define PARAMETER(name, text, min, default, max, flags, function) uint falcon_##name = default;
#include "StorageParameters.h"
#undef PARAMETER

#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif

static const char RECORD_MEMORY_UPPER[]		= "250mb";
static const char PAGE_CACHE_MEMORY[]		= "4mb";
static const uint32	ONE_MB					= 1024*1024;
static const uint64 MIN_PAGE_CACHE			= 2097152;
static const uint64 MIN_RECORD_MEMORY		= 5000000;
static const int	MIN_SCAVENGE_THRESHOLD	= 10;
static const int	MIN_SCAVENGE_FLOOR		= 10;
static const uint64 MAX_TRANSACTION_BACKLOG	= 10000;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Configuration::Configuration(const char *configFile)
{
	checkpointSchedule = "7,37 * * * * *";
	scavengeSchedule = "15,45 * * * * *";

#ifdef STORAGE_ENGINE
	recordMemoryMax				= falcon_record_memory_max;
	recordScavengeThresholdPct	= falcon_record_scavenge_threshold;
	recordScavengeFloorPct		= falcon_record_scavenge_floor;
	initialAllocation			= falcon_initial_allocation;
	allocationExtent			= falcon_allocation_extent;
	serialLogWindows			= falcon_serial_log_buffers;
	pageCacheSize				= falcon_page_cache_size;
	indexChillThreshold			= falcon_index_chill_threshold * ONE_MB;
	recordChillThreshold		= falcon_record_chill_threshold * ONE_MB;
	maxTransactionBacklog		= falcon_max_transaction_backlog;
	
	if (falcon_checkpoint_schedule)
		checkpointSchedule = falcon_checkpoint_schedule;
	
	if (falcon_scavenge_schedule)
		scavengeSchedule = falcon_scavenge_schedule;
		
	if (falcon_serial_log_dir)
		{
		char fullLogPath[PATH_MAX];
		const char *baseName;
	
		// Fully qualify the serial log path using a dummy file name
		
		JString tempLogDir(falcon_serial_log_dir);
		tempLogDir += "/test.fl1";
		IO io;
		io.expandFileName(tempLogDir, sizeof(fullLogPath), fullLogPath, &baseName);

		// Set the path, remove the file name
		
		serialLogDir = JString(fullLogPath, (int)(baseName - fullLogPath));
	
		// Verify that the directory exists
		
		ScanDir scanDir(serialLogDir, "*.*");
		scanDir.next();
		
		if (!scanDir.isDirectory())
			{
			//throw SQLEXCEPTION (RUNTIME_ERROR, "Invalid serial log directory path \"%s\"", falcon_serial_log_dir);
			serialLogDir = "";
			}
		}
#else
	recordMemoryMax				= getMemorySize(RECORD_MEMORY_UPPER);
	recordScavengeThresholdPct	= 67;
	recordScavengeFloorPct		= 33;
	recordScavengeThreshold		= (recordMemoryMax * 100) / recordScavengeThresholdPct;
	recordScavengeFloor			= (recordMemoryMax * 100) / recordScavengeFloorPct;
	serialLogWindows			= 10;
	initialAllocation			= 0;
	allocationExtent			= 10;
	disableFsync				= false;
	pageCacheSize				= getMemorySize(PAGE_CACHE_MEMORY);
	indexChillThreshold			= 4 * ONE_MB;
	recordChillThreshold		= 5 * ONE_MB;
	maxTransactionBacklog		= MAX_TRANSACTION_BACKLOG;
	disableFsync				= false;
#endif

	javaInitialAllocation = 0;
	javaSecondaryAllocation = 0;
	maxThreads = 0;
	schedulerEnabled = true;
	gcSchedule = "0,30 * * * * *";
	useCount = 1;

	// Handle initialization file

	const char *fileName = (configFile) ? configFile : CONFIG_FILE;
	FILE *file = fopen (fileName, "r");
	
	if (!file)
		{
		if (configFile)
			throw SQLEXCEPTION (RUNTIME_ERROR, "can't open configuration file \"%s\"", configFile);
		}
	else
		{
		char line [1024];

		while (getLine (file, sizeof (line), line))
			{
			const char *p = line;
			
			while (*p == ' ' || *p == '\t')
				++p;
				
			if (*p == '#')
				continue;

			const char *start = p;

			while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '=' && *p != ':' && *p != '\r')
				++p;

			if (p == start)
				continue;

			JString parameter (start, p - start);

			while (*p == ' ' || *p == '\t' || *p == '=' || *p == ':')
				++p;

			start = p;

			while (*p && *p != '#' && *p != '\n' && *p != '\r')
				++p;

			JString value = JString (start, p - start);

			if (parameter.equalsNoCase ("classpath"))
				classpath = value;
			else if (parameter.equalsNoCase ("min_record_memory"))
				recordScavengeFloor = getMemorySize (value);
			else if (parameter.equalsNoCase ("max_record_memory"))
				recordMemoryMax = getMemorySize (value);
			else if (parameter.equalsNoCase ("page_cache_size"))
				pageCacheSize = getMemorySize (value);
			else if (parameter.equalsNoCase ("java_initial_allocation"))
				javaInitialAllocation = getMemorySize (value);
			else if (parameter.equalsNoCase ("java_secondary_allocation"))
				javaSecondaryAllocation = getMemorySize (value);
			else if (parameter.equalsNoCase ("gcSchedule"))
				gcSchedule = value;
			else if (parameter.equalsNoCase ("scavengeSchedule"))
				scavengeSchedule = value;
			else if (parameter.equalsNoCase ("checkpointSchedule"))
				checkpointSchedule = value;
			else if (parameter.equalsNoCase ("max_threads"))
				maxThreads = atoi(value);
			else if (parameter.equalsNoCase ("scrub"))
				Log::scrubWords (value);
			else if (parameter.equalsNoCase ("scheduler"))
				schedulerEnabled = enabled(value);
			else
				throw SQLEXCEPTION (DDL_ERROR, "unknown config parameter \"%s\"", 
									(const char*) parameter);
			}
			
		fclose (file);
		}

	pageCacheSize = MAX(pageCacheSize, MIN_PAGE_CACHE);
	setRecordMemoryMax(recordMemoryMax);
	
#ifdef STORAGE_ENGINE
	falcon_page_cache_size = pageCacheSize;
#endif
}

Configuration::~Configuration()
{

}

bool Configuration::getLine(void *file, int length, char *line)
{
	char *p = line;
	char *end = line + length - 1;
	char buffer [1024];

	for (;;)
		{
		if (!fgets (buffer, sizeof (buffer), (FILE*) file))
			return false;
			
		const char *q = buffer;
		
		while (*q == ' ' || *q == '\t')
			++q;
			
		while (q < end && *q && *q != '\n')
			*p++ = *q++;
			
		if (p [-1] != '\\')
			{
			*p = 0;
			
			return true;
			}
			
		--p;
		}
}

int64 Configuration::getMemorySize(const char *string)
{
	int64 n = 0;

	for (const char *p = string; *p;)
		{
		char c = *p++;
		if (c >= '0' && c <= '9')
			n = n * 10 + c - '0';
		else if (c == 'g' || c == 'G')
			n *= 1024 * 1024 * 1024;
		else if (c == 'm' || c == 'M')
			n *= 1024 * 1024;
		else if (c == 'k' || c == 'K')
			n *= 1024;
		}

	return n;
}

int64 Configuration::getAvailablePhysicalMemory()
{
	int64 availableMemory = 0;

#ifdef _WIN32
	MEMORYSTATUSEX stat;
	DWORD error = 0;

	memset(&stat, 0, sizeof(stat));
	stat.dwLength = sizeof(stat);

	if (GlobalMemoryStatusEx(&stat) != 0)
		availableMemory = stat.ullAvailPhys;
	else
		error = GetLastError();

#elif defined(__APPLE__) || defined(__FreeBSD__)

	// TBD: Hardcode availableMemory until fixed for MAC OS.
    // Todo: Also find a fix for FreeBSD.

	availableMemory = MIN_RECORD_MEMORY * 10;

	// On Mac OS X, sysctl with selectors CTL_HW, HW_PHYSMEM returns only a 
	// 4-byte value, even if passed an 8-byte buffer, and limits the returned 
	// value to 2GB when the actual RAM size is > 2GB.  The Gestalt selector 
	// gestaltPhysicalRAMSizeInMegabytes is available starting with OS 10.3.0.

	/*
	if (Gestalt(gestaltPhysicalRAMSizeInMegabytes, &availableMemory))
		{
		etc.
		)

	availableMemory *= ONE_MB;
	*/
#else
	int32 pageSize		= sysconf(_SC_PAGESIZE);
	//int32 physPages	= sysconf(_SC_PHYS_PAGES);
	int32 avPhysPages	= sysconf(_SC_AVPHYS_PAGES);

	if ((pageSize > 0) && (avPhysPages > 0))
		availableMemory = (pageSize * avPhysPages);
#endif

	return availableMemory;
}


void Configuration::addRef()
{
	++useCount;
}

void Configuration::release()
{
	if (--useCount == 0)
		delete this;
}

bool Configuration::enabled(JString string)
{
	if (string.equalsNoCase ("enabled") ||
		 string.equalsNoCase ("yes") ||
		 string.equalsNoCase ("on"))
		return true;

	if (string.equalsNoCase ("disabled") ||
		 string.equalsNoCase ("no") ||
		 string.equalsNoCase ("off"))
		return false;

	throw SQLEXCEPTION (DDL_ERROR, "unknown enable value \"%s\"", (const char*) string);
}

void Configuration::setRecordScavengeThreshold(int threshold)
{
	recordScavengeThresholdPct = MAX(threshold, MIN_SCAVENGE_THRESHOLD);
	recordScavengeThreshold = (recordMemoryMax * recordScavengeThresholdPct) / 100;
	recordScavengeFloor	= (recordScavengeThreshold * recordScavengeFloorPct) / 100;

#ifdef STORAGE_ENGINE
	falcon_record_scavenge_threshold = recordScavengeThresholdPct;
#endif
}

void Configuration::setRecordScavengeFloor(int floor)
{
	recordScavengeFloorPct = MAX(floor, MIN_SCAVENGE_FLOOR);
	recordScavengeFloor	= (recordScavengeThreshold * recordScavengeFloorPct) / 100;

#ifdef STORAGE_ENGINE
	falcon_record_scavenge_floor = recordScavengeFloorPct;
#endif
}

void Configuration::setRecordMemoryMax(uint64 value)
{
	recordMemoryMax = MAX(value, MIN_RECORD_MEMORY);
	
	setRecordScavengeThreshold(recordScavengeThresholdPct);
	
	MemMgrSetMaxRecordMember(recordMemoryMax);
	
#ifdef STORAGE_ENGINE
	falcon_record_memory_max = recordMemoryMax;
#endif
}
