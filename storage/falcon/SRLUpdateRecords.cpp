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

#include <stdio.h>
#include "Engine.h"
#include "SRLUpdateRecords.h"
#include "Stream.h"
#include "Table.h"
#include "SerialLogControl.h"
#include "SerialLogTransaction.h"
#include "Dbb.h"
#include "Transaction.h"
#include "RecordVersion.h"
#include "Log.h"
#include "Sync.h"
#include "SerialLogWindow.h"
#include "Format.h"

SRLUpdateRecords::SRLUpdateRecords(void)
{
}

SRLUpdateRecords::~SRLUpdateRecords(void)
{
}

void SRLUpdateRecords::chill(Transaction *transaction, RecordVersion *record, uint dataLength)
{
	// Record data has been written to the serial log, so release the data
	// buffer and set the state accordingly
	
	ASSERT(record->format);
	record->deleteData();
	record->state = recChilled;
	
	// Update transaction counter and chillPoint
	
	transaction->chillPoint = &record->nextInTrans;
	ASSERT(transaction->totalRecordData >= dataLength);
	
	if (transaction->totalRecordData >= dataLength)
		transaction->totalRecordData -= dataLength;
}

int SRLUpdateRecords::thaw(RecordVersion *record)
{
	// Nothing to do if record is no longer chilled
	
	if (record->state != recChilled)
		return record->size;

	// Find the window where the record is stored using the record offset, then
	// activate the window, reading from disk if necessary

	SerialLogWindow *window = log->findWindowGivenOffset(record->getVirtualOffset());
	
	// Return if the serial log window is no longer available

	if (!window)
		return 0;
		
	// Return pointer to record data

	control->input = window->buffer + (record->getVirtualOffset() - window->virtualOffset);
	control->inputEnd = window->bufferEnd;
	
	// Get section id, record id and data length written. Input pointer will be at
	// beginning of record data.

	int tableSpaceId;
	
	if (control->version >= srlVersion8)
		tableSpaceId = control->getInt();
	else
		tableSpaceId = 0;
		
	control->getInt();			// sectionId
	int recordNumber = control->getInt();
	int dataLength   = control->getInt();
	ASSERT(recordNumber == record->recordNumber);
	int bytesReallocated = record->setRecordData(control->input, dataLength);

	if (bytesReallocated > 0)
		bytesReallocated = record->getEncodedSize();

	window->deactivateWindow();

	if (log->chilledRecords > 0)
		log->chilledRecords--;
		
	if (log->chilledBytes > uint64(bytesReallocated))
		log->chilledBytes -= bytesReallocated;
	else
		log->chilledBytes = 0;
	
	return bytesReallocated;
}

void SRLUpdateRecords::append(Transaction *transaction, RecordVersion *records, bool chillRecords)
{
	uint32 chilledRecordsWindow = 0;
	uint32 chilledBytesWindow   = 0;
	uint32 windowNumber         = 0;
	SerialLogTransaction *srlTrans = NULL;
	
	for (RecordVersion *record = records; record;)
		{
		START_RECORD(srlUpdateRecords, "SRLUpdateRecords::append");
		
		if (srlTrans == NULL)
			{
			srlTrans = log->getTransaction(transaction->transactionId);
			srlTrans->setTransaction(transaction);
			}

		putInt(transaction->transactionId);
		UCHAR *lengthPtr = putFixedInt(0);
		UCHAR *start = log->writePtr;
		UCHAR *end = log->writeWarningTrack;
		
		chilledRecordsWindow = 0;
		chilledBytesWindow = 0;

		for (; record; record = record->nextInTrans)
			{
			// Skip lock records
			
			if (record->state == recLock)
				continue;
				
			// Skip chilled records, but advance the chillpoint
			
			if (record->state == recChilled)
				{
				transaction->chillPoint = &record->nextInTrans;
				
				continue;
				}
				
			// Skip record that is currently being used, but don't advance chillpoint
			
			if (record->state == recNoChill)
				continue;

			Table *table = record->format->table;
			tableSpaceId = table->dbb->tableSpaceId;
			Stream stream;
			
			// Thawed records are indicated by a non-zero virtual offset, and
			// are already in the serial log. If this record is to be re-chilled,
			// then no need to get the record data or set the virtual offset.

			if (chillRecords && record->state != recDeleted && record->virtualOffset != 0)
				{
				int chillBytes = record->getEncodedSize();
				chill(transaction, record, chillBytes);
				log->chilledRecords++;
				log->chilledBytes += chillBytes;
				ASSERT(transaction->thawedRecords > 0);

				if (transaction->thawedRecords)
					transaction->thawedRecords--;

				continue;
				}
			
			// Load the record data into a stream

			if (record->hasRecord())
				record->getRecord(&stream);
			else
				ASSERT(record->state == recDeleted);
			
			// Ensure record fits within current window

			if (log->writePtr + 
				 byteCount(tableSpaceId) + 
				 byteCount(table->dataSectionId) + 
				 byteCount(record->recordNumber) + 
				 byteCount(stream.totalLength) + stream.totalLength >= end)
				break;
			
			// Set the virtual offset of the record in the serial log

			ASSERT(record->recordNumber >= 0);
			ASSERT(log->writePtr > (UCHAR *)log->writeWindow->buffer);
			record->setVirtualOffset(log->writeWindow->currentLength + log->writeWindow->virtualOffset);
			uint32 sectionId = table->dataSectionId;
			log->updateSectionUseVector(sectionId, tableSpaceId, 1);
			putInt(tableSpaceId);
			putInt((record->priorVersion) ? sectionId : -(int) sectionId - 1);
			putInt(record->recordNumber);
			putStream(&stream);
			
			if (chillRecords && record->state != recDeleted)
				{
				chill(transaction, record, stream.totalLength);
				chilledRecordsWindow++;
				chilledBytesWindow += stream.totalLength;
				}
			} // next record
		
		int len = (int) (log->writePtr - start);
		
		if (len > 0)
			putFixedInt(len, lengthPtr);
		
		if (record)
			log->flush(true, 0, &sync);
		else
			sync.unlock();
			
		if (chillRecords)
			{
			log->chilledRecords += chilledRecordsWindow;
			log->chilledBytes   += chilledBytesWindow;
			transaction->chilledRecords += chilledRecordsWindow;
			windowNumber = (uint32)log->writeWindow->virtualOffset / SRL_WINDOW_SIZE;
			}
		} // next window
}

void SRLUpdateRecords::read(void)
{
	transactionId = getInt();
	dataLength = getInt();
	data = getData(dataLength);
}

void SRLUpdateRecords::redo(void)
{
	SerialLogTransaction *transaction = control->getTransaction(transactionId);
	
	if (transaction->state == sltCommitted)
		for (const UCHAR *p = data, *end = data + dataLength; p < end;)
			{
			if (control->version >= srlVersion8)
				tableSpaceId = getInt(&p);
			else
				tableSpaceId = 0;
		
			int id = getInt(&p);
			uint sectionId = (id >= 0) ? id : -id - 1;
			int recordNumber = getInt(&p);
			int length = getInt(&p);
			log->updateSectionUseVector(sectionId, tableSpaceId, -1);
			
			if (log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse))
				{
				Dbb *dbb = log->getDbb(tableSpaceId);
				
				if (length)
					{
					if (id < 0)
						dbb->reInsertStub(sectionId, recordNumber, transactionId);
						
					Stream stream;
					stream.putSegment(length, (const char*) p, false);
					dbb->updateRecord(sectionId, recordNumber, &stream, transactionId, false);
					}
				else
					dbb->updateRecord(sectionId, recordNumber, NULL, transactionId, false);
				}
			
			p += length;
			}
	else
		pass1();
}

void SRLUpdateRecords::pass1(void)
{
	control->getTransaction(transactionId);

	for (const UCHAR *p = data, *end = data + dataLength; p < end;)
		{
		if (control->version >= srlVersion8)
			tableSpaceId = getInt(&p);
		else
			tableSpaceId = 0;
			
		int id = getInt(&p);
		uint sectionId = (id >= 0) ? id : -id - 1;
		getInt(&p);			// recordNumber
		int length = getInt(&p);
		log->bumpSectionIncarnation(sectionId, tableSpaceId, objInUse);
		p += length;
		}
}

void SRLUpdateRecords::pass2(void)
{
	pass1();
}

void SRLUpdateRecords::commit(void)
{
	Sync sync(&log->syncSections, "SRLUpdateRecords::commit");
	sync.lock(Shared);
	
	for (const UCHAR *p = data, *end = data + dataLength; p < end;)
		{
		if (control->version >= srlVersion8)
			tableSpaceId = getInt(&p);
		else
			tableSpaceId = 0;
			
		int id = getInt(&p);
		uint sectionId = (id >= 0) ? id : -id - 1;
		int recordNumber = getInt(&p);
		int length = getInt(&p);
		log->updateSectionUseVector(sectionId, tableSpaceId, -1);
		
		if (log->isSectionActive(sectionId, tableSpaceId))
			{
			Dbb *dbb = log->getDbb(tableSpaceId);

			if (length)
				{
				Stream stream;
				stream.putSegment(length, (const char*) p, false);
				dbb->updateRecord(sectionId, recordNumber, &stream, transactionId, false);
				}
			else
				dbb->updateRecord(sectionId, recordNumber, NULL, transactionId, false);
			}
		
		p += length;
		}
}

void SRLUpdateRecords::print(void)
{
	logPrint("UpdateRecords: transaction %d, length %d\n", transactionId, dataLength);
	
	for (const UCHAR *p = data, *end = data + dataLength; p < end;)
		{
		if (control->version >= srlVersion8)
			tableSpaceId = getInt(&p);
		else
			tableSpaceId = 0;

		int id = getInt(&p);
		uint sectionId = (id >= 0) ? id : -id - 1;
		int recordNumber = getInt(&p);
		int length = getInt(&p);
		char temp[40];
		Log::debug("   rec %d, len %d to section %d/%d %s\n", 
					recordNumber, length, sectionId, tableSpaceId, format(length, p, sizeof(temp), temp));
		p += length;
		}
}
