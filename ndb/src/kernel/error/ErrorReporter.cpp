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

#include "Error.hpp"
#include "ErrorReporter.hpp"
#include "ErrorMessages.hpp"

#include <FastScheduler.hpp>
#include <DebuggerNames.hpp>
#include <NdbHost.h>
#include <NdbConfig.h>
#include <Configuration.hpp>

#define MESSAGE_LENGTH 400

const char* errorType[] = { 
  "warning", 
  "error", 
  "fatal", 
  "assert"
};


static int WriteMessage(ErrorCategory thrdType, int thrdMessageID,
			const char* thrdProblemData, 
			const char* thrdObjRef,
			Uint32 thrdTheEmulatedJamIndex = 0,
			Uint8 thrdTheEmulatedJam[] = 0);

static void dumpJam(FILE* jamStream, 
		    Uint32 thrdTheEmulatedJamIndex, 
		    Uint8 thrdTheEmulatedJam[]);


const char*
ErrorReporter::formatTimeStampString(){
  TimeModule DateTime;          /* To create "theDateTimeString" */
  
  static char theDateTimeString[39]; 
  /* Used to store the generated timestamp */
  /* ex: "Wednesday 18 September 2000 - 18:54:37" */

  DateTime.setTimeStamp();
  
  snprintf(theDateTimeString, 39, "%s %d %s %d - %s:%s:%s", 
	   DateTime.getDayName(), DateTime.getDayOfMonth(),
	   DateTime.getMonthName(), DateTime.getYear(), DateTime.getHour(),
	   DateTime.getMinute(), DateTime.getSecond());
  
  return (const char *)&theDateTimeString;
}

void
ErrorReporter::formatTraceFileName(char* theName, int maxLen){
  
  FILE *stream;
  unsigned int traceFileNo;
  char fileNameBuf[255];
  char buf[255];

  NdbConfig_HomePath(fileNameBuf, 255);
  strncat(fileNameBuf, "NextTraceFileNo.log", 255);
  /* 
   * Read last number from tracefile
   */  
  stream = fopen(fileNameBuf, "r+");
  if (stream == NULL){
    traceFileNo = 1;
  } else {
    fgets(buf, 255, stream);
    const int scan = sscanf(buf, "%u", &traceFileNo);
    if(scan != 1){
      traceFileNo = 1;
    }
    fclose(stream);
    traceFileNo++;
  }

  /**
   * Wrap tracefile no 
   */
  Uint32 tmp = globalEmulatorData.theConfiguration->maxNoOfErrorLogs();
  if (traceFileNo > tmp ) {
    traceFileNo = 1;
  }

  /**
   *  Save new number to the file
   */
  stream = fopen(fileNameBuf, "w");
  if(stream != NULL){
    fprintf(stream, "%u", traceFileNo);
    fclose(stream);
  }
  /**
   * Format trace file name
   */
  snprintf(theName, maxLen, "%sNDB_TraceFile_%u.trace", 
	   NdbConfig_HomePath(fileNameBuf, 255), traceFileNo);
}


void
ErrorReporter::formatMessage(ErrorCategory type, 
			     int faultID,
			     const char* problemData, 
			     const char* objRef,
			     const char* theNameOfTheTraceFile,
			     char* messptr){
  int processId;
  
  processId = NdbHost_GetProcessId();
  
  snprintf(messptr, MESSAGE_LENGTH,
	   "Date/Time: %s\nType of error: %s\n"
	   "Message: %s\nFault ID: %d\nProblem data: %s"
	   "\nObject of reference: %s\nProgramName: %s\n"
	   "ProcessID: %d\nTraceFile: %s\n***EOM***\n", 
	   formatTimeStampString() , 
	   errorType[type], 
	   lookupErrorMessage(faultID),
	   faultID, 
	   (problemData == NULL) ? "" : problemData, 
	   objRef, 
	   programName, 
	   processId, 
	   theNameOfTheTraceFile);

  // Add trailing blanks to get a fixed lenght of the message
  while (strlen(messptr) <= MESSAGE_LENGTH-3){
    strcat(messptr, " ");
  }
  
  strcat(messptr, "\n");
  
  return;
}

void
ErrorReporter::handleAssert(const char* message, const char* file, int line)
{
  char refMessage[100];

#ifdef USE_EMULATED_JAM
  const Uint32 blockNumber = theEmulatedJamBlockNumber;
  const char *blockName = getBlockName(blockNumber);

  snprintf(refMessage, 100, "%s line: %d (block: %s)",
	   file, line, blockName);
  
  WriteMessage(assert, ERR_ERROR_PRGERR, message, refMessage,
	       theEmulatedJamIndex, theEmulatedJam);
#else
  snprintf(refMessage, 100, "file: %s lineNo: %d",
	   file, line);

  WriteMessage(assert, ERR_ERROR_PRGERR, message, refMessage);
#endif

  NdbShutdown(NST_ErrorHandler);
}

void
ErrorReporter::handleThreadAssert(const char* message,
                                  const char* file,
                                  int line)
{
  char refMessage[100];
  snprintf(refMessage, 100, "file: %s lineNo: %d - %s",
	   file, line, message);
  
  NdbShutdown(NST_ErrorHandler);
}//ErrorReporter::handleThreadAssert()


void
ErrorReporter::handleError(ErrorCategory type, int messageID,
			   const char* problemData, 
			   const char* objRef,
			   NdbShutdownType nst)
{
  type = ecError; 
  // The value for type is not always set correctly in the calling function.
  // So, to correct this, we set it set it to the value corresponding to
  // the function that is called.
#ifdef USE_EMULATED_JAM
  WriteMessage(type, messageID, problemData,
	       objRef, theEmulatedJamIndex, theEmulatedJam);
#else
  WriteMessage(type, messageID, problemData, objRef);
#endif
  if(messageID == ERR_ERROR_INSERT){
    NdbShutdown(NST_ErrorInsert);
  } else {
    NdbShutdown(nst);
  }
}

// This is the function to write the error-message, 
// when the USE_EMULATED_JAM-flag is set
// during compilation.
int 
WriteMessage(ErrorCategory thrdType, int thrdMessageID,
	     const char* thrdProblemData, const char* thrdObjRef,
	     Uint32 thrdTheEmulatedJamIndex,
	     Uint8 thrdTheEmulatedJam[]){
  FILE *stream;
  unsigned offset;
  unsigned long maxOffset;  // Maximum size of file.
  char theMessage[MESSAGE_LENGTH];
  char theTraceFileName[255]; 
  char theErrorFileName[255]; 
  ErrorReporter::formatTraceFileName(theTraceFileName, 255);
  
  // The first 69 bytes is info about the current offset
  Uint32 noMsg = globalEmulatorData.theConfiguration->maxNoOfErrorLogs();

  maxOffset = (69 + (noMsg * MESSAGE_LENGTH));
  
  NdbConfig_ErrorFileName(theErrorFileName, 255);
  stream = fopen(theErrorFileName, "r+");
  if (stream == NULL) { /* If the file could not be opened. */
    
    // Create a new file, and skip the first 69 bytes, 
    // which are info about the current offset
    stream = fopen(theErrorFileName, "w");
    fprintf(stream, "%s%u%s", "Current byte-offset of file-pointer is: ", 69,
	    "                        \n\n\n");   
    
    // ...and write the error-message...
    ErrorReporter::formatMessage(thrdType, thrdMessageID,
				 thrdProblemData, thrdObjRef,
				 theTraceFileName, theMessage);
    fprintf(stream, "%s", theMessage);
    fflush(stream);
    
    /* ...and finally, at the beginning of the file, 
       store the position where to
       start writing the next message. */
    offset = ftell(stream);
    // If we have not reached the maximum number of messages...
    if (offset <= (maxOffset - MESSAGE_LENGTH)){
      fseek(stream, 40, SEEK_SET);
      // ...set the current offset...
      fprintf(stream,"%d", offset);
    } else {
      fseek(stream, 40, SEEK_SET);
      // ...otherwise, start over from the beginning.
      fprintf(stream, "%u%s", 69, "             ");
    }
  } else {
    // Go to the latest position in the file...
    fseek(stream, 40, SEEK_SET);
    fscanf(stream, "%u", &offset);
    fseek(stream, offset, SEEK_SET);
    
    // ...and write the error-message there...
    ErrorReporter::formatMessage(thrdType, thrdMessageID,
				 thrdProblemData, thrdObjRef,
				 theTraceFileName, theMessage);
    fprintf(stream, "%s", theMessage);
    fflush(stream);
    
    /* ...and finally, at the beginning of the file, 
       store the position where to
       start writing the next message. */
    offset = ftell(stream);
    
    // If we have not reached the maximum number of messages...
    if (offset <= (maxOffset - MESSAGE_LENGTH)){
      fseek(stream, 40, SEEK_SET);
      // ...set the current offset...
      fprintf(stream,"%d", offset);
    } else {
      fseek(stream, 40, SEEK_SET);
      // ...otherwise, start over from the beginning.
      fprintf(stream, "%u%s", 69, "             ");
    }
  }
  fflush(stream);
  fclose(stream);
  
  // Open the tracefile...
  FILE *jamStream = fopen(theTraceFileName, "w");
  
  //  ...and "dump the jam" there.
  //  ErrorReporter::dumpJam(jamStream);
  if(thrdTheEmulatedJam != 0){
#ifdef USE_EMULATED_JAM
    dumpJam(jamStream, thrdTheEmulatedJamIndex, thrdTheEmulatedJam);
#endif
  }
  
  /* Dont print the jobBuffers until a way to copy them, 
     like the other variables,
     is implemented. Otherwise when NDB keeps running, 
     with this function running
     in the background, the jobBuffers will change during runtime. And when
     they're printed here, they will not be correct anymore.
  */
  globalScheduler.dumpSignalMemory(jamStream);
  
  fclose(jamStream);
  
  return 0;
}

void 
dumpJam(FILE *jamStream, 
	Uint32 thrdTheEmulatedJamIndex, 
	Uint8 thrdTheEmulatedJam[]) {
#ifdef USE_EMULATED_JAM   
  // print header
  const int maxaddr = 8;
  fprintf(jamStream, "JAM CONTENTS up->down left->right ?=not block entry\n");
  fprintf(jamStream, "%-7s ", "BLOCK");
  for (int i = 0; i < maxaddr; i++)
    fprintf(jamStream, "%-6s ", "ADDR");
  fprintf(jamStream, "\n");

  // treat as array of Uint32
  const Uint32 *base = (Uint32 *)thrdTheEmulatedJam;
  const int first = thrdTheEmulatedJamIndex / sizeof(Uint32);	// oldest
  int cnt, idx;

  // look for first block entry
  for (cnt = 0, idx = first; cnt < EMULATED_JAM_SIZE; cnt++, idx++) {
    if (idx >= EMULATED_JAM_SIZE)
      idx = 0;
    const Uint32 aJamEntry = base[idx];
    if (aJamEntry > (1 << 20))
      break;
  }

  // 1. if first entry is a block entry, it is printed in the main loop
  // 2. else if any block entry exists, the jam starts in an unknown block
  // 3. else if no block entry exists, the block is theEmulatedJamBlockNumber
  // a "?" indicates first addr is not a block entry
  if (cnt == 0)
    ;
  else if (cnt < EMULATED_JAM_SIZE)
    fprintf(jamStream, "%-7s?", "");
  else {
    const Uint32 aBlockNumber = theEmulatedJamBlockNumber;
    const char *aBlockName = getBlockName(aBlockNumber);
    if (aBlockName != 0)
      fprintf(jamStream, "%-7s?", aBlockName);
    else
      fprintf(jamStream, "0x%-5X?", aBlockNumber);
  }

  // loop over all entries
  int cntaddr = 0;
  for (cnt = 0, idx = first; cnt < EMULATED_JAM_SIZE; cnt++, idx++) {
    globalData.incrementWatchDogCounter(4);	// watchdog not to kill us ?
    if (idx >= EMULATED_JAM_SIZE)
      idx = 0;
    const Uint32 aJamEntry = base[idx];
    if (aJamEntry > (1 << 20)) {
      const Uint32 aBlockNumber = aJamEntry >> 20;
      const char *aBlockName = getBlockName(aBlockNumber);
      if (cnt > 0)
	  fprintf(jamStream, "\n");
      if (aBlockName != 0)
	fprintf(jamStream, "%-7s ", aBlockName);
      else
	fprintf(jamStream, "0x%-5X ", aBlockNumber);
      cntaddr = 0;
    }
    if (cntaddr == maxaddr) {
      fprintf(jamStream, "\n%-7s ", "");
      cntaddr = 0;
    }
    fprintf(jamStream, "%06u ", aJamEntry & 0xFFFFF);
    cntaddr++;
  }
  fprintf(jamStream, "\n");
  fflush(jamStream);
#endif // USE_EMULATED_JAM
}
