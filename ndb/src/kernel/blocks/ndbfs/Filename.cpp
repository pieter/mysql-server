/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>

#include <NdbOut.hpp>

#include "Filename.hpp"
#include "ErrorHandlingMacros.hpp"
#include "Error.hpp"
#include "RefConvert.hpp"
#include "DebuggerNames.hpp"

#include <signaldata/FsOpenReq.hpp>

static const char* fileExtension[] = {
  ".Data",
  ".FragLog",
  ".LocLog",
  ".FragList",
  ".TableList",
  ".SchemaLog",
  ".sysfile",
  ".log",
  ".ctl"
};

static const Uint32 noOfExtensions = sizeof(fileExtension)/sizeof(char*);

Filename::Filename() :
  theLevelDepth(0)
{
}

void
Filename::init(Uint32 nodeid, const char * pFileSystemPath){
  if (pFileSystemPath == NULL) {
    ERROR_SET(fatal, AFS_ERROR_NOPATH, ""," Filename::init()");
    return;
  }

  strncpy(theBaseDirectory, pFileSystemPath, PATH_MAX);
  
  // the environment variable is set,
  // check that it is pointing on a valid directory
  // 
  char buf2[PATH_MAX]; memset(buf2, 0,sizeof(buf2));
#ifdef NDB_WIN32
  char* szFilePart;
  if(!GetFullPathName(theBaseDirectory, sizeof(buf2), buf2, &szFilePart)
     || (::GetFileAttributes(theBaseDirectory)&FILE_ATTRIBUTE_READONLY)) 
#else
    if((::realpath(theBaseDirectory, buf2) == NULL)||
       (::access(theBaseDirectory, W_OK) != 0))
#endif
      {
	ERROR_SET(fatal, AFS_ERROR_INVALIDPATH, pFileSystemPath, " Filename::init()");
      }
  strncpy(theBaseDirectory, buf2, sizeof(theBaseDirectory));
  // path seems ok, add delimiter if missing
  if (strcmp(&theBaseDirectory[strlen(theBaseDirectory) - 1], 
	     DIR_SEPARATOR) != 0)
    strcat(theBaseDirectory, DIR_SEPARATOR);
  
  snprintf(buf2, sizeof(buf2), "ndb_%u_fs%s", nodeid, DIR_SEPARATOR);
  strcat(theBaseDirectory, buf2);

#ifdef NDB_WIN32
  CreateDirectory(theBaseDirectory, 0);
#else
  mkdir(theBaseDirectory, S_IRUSR | S_IWUSR | S_IXUSR | S_IXGRP | S_IRGRP);
#endif
}

Filename::~Filename(){
}

void 
Filename::set(BlockReference blockReference, 
	      const Uint32 filenumber[4], bool dir) 
{
  char buf[PATH_MAX];
  theLevelDepth = 0;
  strncpy(theName, theBaseDirectory, PATH_MAX);
  
  const Uint32 type = FsOpenReq::getSuffix(filenumber);
  const Uint32 version = FsOpenReq::getVersion(filenumber);
  switch(version){
  case 1 :{
    const Uint32 diskNo = FsOpenReq::v1_getDisk(filenumber);
    const Uint32 table  = FsOpenReq::v1_getTable(filenumber);
    const Uint32 frag   = FsOpenReq::v1_getFragment(filenumber);
    const Uint32 S_val  = FsOpenReq::v1_getS(filenumber);
    const Uint32 P_val  = FsOpenReq::v1_getP(filenumber);

    if (diskNo < 0xff){	  
      snprintf(buf, sizeof(buf), "D%d%s", diskNo, DIR_SEPARATOR);
      strcat(theName, buf);
      theLevelDepth++;
    }
    
    {
      const char* blockName = getBlockName( refToBlock(blockReference) );
      if (blockName == NULL){
	ERROR_SET(ecError, AFS_ERROR_PARAMETER,"","No Block Name");
	return;
      }
      snprintf(buf, sizeof(buf), "%s%s", blockName, DIR_SEPARATOR);
      strcat(theName, buf);
      theLevelDepth++;
    }
    
    if (table < 0xffffffff){
      snprintf(buf, sizeof(buf), "T%d%s", table, DIR_SEPARATOR);
      strcat(theName, buf);
      theLevelDepth++;
    }
    
    if (frag < 0xffffffff){
      snprintf(buf, sizeof(buf), "F%d%s", frag, DIR_SEPARATOR);
      strcat(theName, buf);
      theLevelDepth++;
    }
    
    
    if (S_val < 0xffffffff){
      snprintf(buf, sizeof(buf), "S%d", S_val);
      strcat(theName, buf);
    }

    if (P_val < 0xff){
      snprintf(buf, sizeof(buf), "P%d", P_val);
      strcat(theName, buf);
    }
    
  }
  break;
  case 2:{
    const Uint32 seq = FsOpenReq::v2_getSequence(filenumber);
    const Uint32 nodeId = FsOpenReq::v2_getNodeId(filenumber);
    const Uint32 count = FsOpenReq::v2_getCount(filenumber);
    
    snprintf(buf, sizeof(buf), "BACKUP%sBACKUP-%d%s",
	     DIR_SEPARATOR, seq, DIR_SEPARATOR); 
    strcat(theName, buf);
    if(count == 0xffffffff) {
      snprintf(buf, sizeof(buf), "BACKUP-%d.%d",
	       seq, nodeId); strcat(theName, buf);
    } else {
      snprintf(buf, sizeof(buf), "BACKUP-%d-%d.%d",
	       seq, count, nodeId); strcat(theName, buf);
    }
    theLevelDepth = 2;
    break;
  }
  break;
  case 3:{
    const Uint32 diskNo = FsOpenReq::v1_getDisk(filenumber);

    if(diskNo == 0xFF){
      ERROR_SET(ecError, AFS_ERROR_PARAMETER,"","Invalid disk specification");
    }

    snprintf(buf, sizeof(buf), "D%d%s", diskNo, DIR_SEPARATOR);
    strcat(theName, buf);
    theLevelDepth++;
  }
  break;
  default:
    ERROR_SET(ecError, AFS_ERROR_PARAMETER,"","Wrong version");
  }
  if (type >= noOfExtensions){
    ERROR_SET(ecError, AFS_ERROR_PARAMETER,"","File Type doesn't exist");
    return;
  }
  strcat(theName, fileExtension[type]);
  
  if(dir == true){
    for(int l = strlen(theName) - 1; l >= 0; l--){
      if(theName[l] == DIR_SEPARATOR[0]){
	theName[l] = 0;
	break;
      }
    }
  }
}

/**
 * Find out directory name on level
 * Ex: 
 * theName = "/tmp/fs/T0/NDBFS/D0/P0/S27.data"
 * level = 1 
 * would return "/tmp/fs/T0/NDBFS/
 */
const char* Filename::directory(int level)
{
  const char* p;
  
  p = theName;
  p += strlen(theBaseDirectory);
  
  for (int i = 0; i <= level; i++){
    p = strstr(p, DIR_SEPARATOR);
    p++;
  } 
  
  strncpy(theDirectory, theName, p - theName - 1);
  theDirectory[p-theName-1] = 0;
  return theDirectory;
}


