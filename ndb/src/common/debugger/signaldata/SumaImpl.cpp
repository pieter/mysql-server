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

#include <signaldata/SumaImpl.hpp>

bool
printSUB_CREATE_REQ(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo) {
  const SubCreateReq * const sig = (SubCreateReq *)theData;
  fprintf(output, " subscriberRef: %x\n", sig->subscriberRef);
  fprintf(output, " subscriberData: %x\n", sig->subscriberData);
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " subscriptionType: %x\n", sig->subscriptionType);
  fprintf(output, " tableId: %x\n", sig->tableId);
  return false;
}

bool
printSUB_CREATE_CONF(FILE * output, const Uint32 * theData, 
		     Uint32 len, Uint16 receiverBlockNo) {
  const SubCreateConf * const sig = (SubCreateConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " subscriberData: %x\n", sig->subscriberData);
  return false;
}

bool
printSUB_START_REQ(FILE * output, const Uint32 * theData, 
		   Uint32 len, Uint16 receiverBlockNo) {
  const SubStartReq * const sig = (SubStartReq *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " startPart: %x\n", sig->part);
  return false;
}

bool
printSUB_START_REF(FILE * output, const Uint32 * theData, 
		   Uint32 len, Uint16 receiverBlockNo) {
  const SubStartRef * const sig = (SubStartRef *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " startPart: %x\n", sig->part);
  fprintf(output, " subscriberData: %x\n", sig->subscriberData);
  fprintf(output, " err: %x\n", sig->err);
  return false;
}

bool
printSUB_START_CONF(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo) {
  const SubStartConf * const sig = (SubStartConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " startPart: %x\n", sig->part);
  fprintf(output, " subscriberData: %x\n", sig->subscriberData);
  return false;
}

bool
printSUB_SYNC_REQ(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncReq * const sig = (SubSyncReq *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " syncPart: %x\n", sig->part);
  return false;
}

bool
printSUB_SYNC_REF(FILE * output, const Uint32 * theData, 
		  Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncRef * const sig = (SubSyncRef *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " syncPart: %x\n", sig->part);
  fprintf(output, " subscriberData: %x\n", sig->subscriberData);
  fprintf(output, " err: %x\n", sig->err);
  return false;
}

bool
printSUB_SYNC_CONF(FILE * output, const Uint32 * theData, 
		   Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncConf * const sig = (SubSyncConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " syncPart: %x\n", sig->part);
  fprintf(output, " subscriberData: %x\n", sig->subscriberData);
  return false;
}

bool
printSUB_META_DATA(FILE * output, const Uint32 * theData, 
		   Uint32 len, Uint16 receiverBlockNo) {
  const SubMetaData * const sig = (SubMetaData *)theData;
  fprintf(output, " gci: %x\n", sig->gci);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " subscriberData: %x\n", sig->subscriberData);
  fprintf(output, " tableId: %x\n", sig->tableId);
  return false;
}

bool
printSUB_TABLE_DATA(FILE * output, const Uint32 * theData, 
		    Uint32 len, Uint16 receiverBlockNo) {
  const SubTableData * const sig = (SubTableData *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " subscriberData: %x\n", sig->subscriberData);
  fprintf(output, " gci: %x\n", sig->gci);
  fprintf(output, " tableId: %x\n", sig->tableId);
  fprintf(output, " operation: %x\n", sig->operation);
  fprintf(output, " noOfAttributes: %x\n", sig->noOfAttributes);
  fprintf(output, " dataSize: %x\n", sig->dataSize);
  return false;
}

bool
printSUB_SYNC_CONTINUE_REQ(FILE * output, const Uint32 * theData, 
			   Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncContinueReq * const sig = (SubSyncContinueReq *)theData;
  fprintf(output, " subscriberData: %x\n", sig->subscriberData);
  fprintf(output, " noOfRowsSent: %x\n", sig->noOfRowsSent);
  return false;
}

bool
printSUB_SYNC_CONTINUE_REF(FILE * output, const Uint32 * theData, 
			   Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncContinueRef * const sig = (SubSyncContinueRef *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  return false;
}

bool
printSUB_SYNC_CONTINUE_CONF(FILE * output, const Uint32 * theData, 
			    Uint32 len, Uint16 receiverBlockNo) {
  const SubSyncContinueConf * const sig = (SubSyncContinueConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  return false;
}

bool
printSUB_GCP_COMPLETE_REP(FILE * output, const Uint32 * theData, 
			  Uint32 len, Uint16 receiverBlockNo) {
  const SubGcpCompleteRep * const sig = (SubGcpCompleteRep *)theData;
  fprintf(output, " gci: %x\n", sig->gci);
  return false;
}

