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


#define DBTUP_C
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTab.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"

#define ljam() { jamLine(20000 + __LINE__); }
#define ljamEntry() { jamEntryLine(20000 + __LINE__); }

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* --------------- ADD/DROP FRAGMENT TABLE MODULE ----------------- */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execTUPFRAGREQ(Signal* signal)
{
  FragoperrecPtr fragOperPtr;
  FragrecordPtr regFragPtr;
  TablerecPtr regTabPtr;

  ljamEntry();
  Uint32 userptr = signal->theData[0];
  Uint32 userblockref = signal->theData[1];
  Uint32 reqinfo = signal->theData[2];
  regTabPtr.i = signal->theData[3];
  Uint32 noOfAttributes = signal->theData[4];
  Uint32 fragId = signal->theData[5];
  Uint32 noOfNullAttr = signal->theData[7];
  /*  Uint32 schemaVersion = signal->theData[8];*/
  Uint32 noOfKeyAttr = signal->theData[9];

  Uint32 noOfNewAttr = signal->theData[10];
  Uint32 checksumIndicator = signal->theData[11];
  Uint32 noOfAttributeGroups = signal->theData[12];
  Uint32 globalCheckpointIdIndicator = signal->theData[13];

  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);
  if (cfirstfreeFragopr == RNIL) {
    ljam();
    signal->theData[0] = userptr;
    signal->theData[1] = ZNOFREE_FRAGOP_ERROR;
    sendSignal(userblockref, GSN_TUPFRAGREF, signal, 2, JBB);
    return;
  }//if
  seizeFragoperrec(fragOperPtr);

  fragOperPtr.p->nextFragoprec = RNIL;
  fragOperPtr.p->lqhBlockrefFrag = userblockref;
  fragOperPtr.p->lqhPtrFrag = userptr;
  fragOperPtr.p->fragidFrag = fragId;
  fragOperPtr.p->tableidFrag = regTabPtr.i;
  fragOperPtr.p->attributeCount = noOfAttributes;
  fragOperPtr.p->freeNullBit = noOfNullAttr;
  fragOperPtr.p->noOfNewAttrCount = noOfNewAttr;

  ndbrequire(reqinfo == ZADDFRAG);

  getFragmentrec(regFragPtr, fragId, regTabPtr.p);
  if (regFragPtr.i != RNIL) {
    ljam();
    terrorCode = ZEXIST_FRAG_ERROR;	/* THE FRAGMENT ALREADY EXIST */
    fragrefuse1Lab(signal, fragOperPtr);
    return;
  }//if
  if (cfirstfreefrag != RNIL) {
    ljam();
    seizeFragrecord(regFragPtr);
  } else {
    ljam();
    terrorCode = ZFULL_FRAGRECORD_ERROR;
    fragrefuse1Lab(signal, fragOperPtr);
    return;
  }//if
  initFragRange(regFragPtr.p);
  if (!addfragtotab(regTabPtr.p, fragId, regFragPtr.i)) {
    ljam();
    terrorCode = ZNO_FREE_TAB_ENTRY_ERROR;
    fragrefuse2Lab(signal, fragOperPtr, regFragPtr);
    return;
  }//if
  if (cfirstfreerange == RNIL) {
    ljam();
    terrorCode = ZNO_FREE_PAGE_RANGE_ERROR;
    fragrefuse3Lab(signal, fragOperPtr, regFragPtr, regTabPtr.p, fragId);
    return;
  }//if

  regFragPtr.p->emptyPrimPage = RNIL;
  regFragPtr.p->thFreeFirst = RNIL;
  regFragPtr.p->thFreeCopyFirst = RNIL;
  regFragPtr.p->noCopyPagesAlloc = 0;
  regFragPtr.p->fragTableId = regTabPtr.i;
  regFragPtr.p->fragmentId = fragId;
  regFragPtr.p->checkpointVersion = RNIL;

  Uint32 noAllocatedPages = 2;
  noAllocatedPages = allocFragPages(regFragPtr.p, noAllocatedPages);

  if (noAllocatedPages == 0) {
    ljam();
    terrorCode = ZNO_PAGES_ALLOCATED_ERROR;
    fragrefuse3Lab(signal, fragOperPtr, regFragPtr, regTabPtr.p, fragId);
    return;
  }//if

  if (regTabPtr.p->tableStatus == NOT_DEFINED) {
    ljam();
//-------------------------------------------------------------------------------------
// We are setting up references to the header of the tuple.
// Active operation  This word contains a reference to the operation active on the tuple
//                   at the moment. RNIL means no one active at all.  Not optional.
// Tuple version     Uses only low 16 bits.  Not optional.
// Checksum          The third header word is optional and contains a checksum of the
//                   tuple header.
// Null-bits         A number of words to contain null bits for all non-dynamic attributes.
//                   Each word contains upto 32 null bits. Each time a new word is needed
//                   we allocate the complete word. Zero nullable attributes means that
//                   there is no word at all
// Global Checkpoint id
//                   This word is optional. When used it is stored as a 32-bit unsigned
//                   attribute with attribute identity 0. Thus the kernel assumes that
//                   this is the first word after the header.
//-------------------------------------------------------------------------------------
    fragOperPtr.p->definingFragment = true;
    regTabPtr.p->tableStatus = DEFINING;
    regTabPtr.p->checksumIndicator = (checksumIndicator != 0 ? true : false);
    regTabPtr.p->GCPIndicator = (globalCheckpointIdIndicator != 0 ? true : false);

    regTabPtr.p->tupChecksumIndex = 2;
    regTabPtr.p->tupNullIndex = 2 + (checksumIndicator != 0 ? 1 : 0);
    regTabPtr.p->tupNullWords = (noOfNullAttr + 31) >> 5;
    regTabPtr.p->tupGCPIndex = regTabPtr.p->tupNullIndex + regTabPtr.p->tupNullWords;
    regTabPtr.p->tupheadsize = regTabPtr.p->tupGCPIndex;

    regTabPtr.p->noOfKeyAttr = noOfKeyAttr;
    regTabPtr.p->noOfAttr = noOfAttributes;
    regTabPtr.p->noOfNewAttr = noOfNewAttr;
    regTabPtr.p->noOfNullAttr = noOfNullAttr;
    regTabPtr.p->noOfAttributeGroups = noOfAttributeGroups;

    regTabPtr.p->notNullAttributeMask.clear();

    Uint32 tableDescriptorRef = allocTabDescr(noOfAttributes, noOfKeyAttr, noOfAttributeGroups);
    if (tableDescriptorRef == RNIL) {
      ljam();
      fragrefuse4Lab(signal, fragOperPtr, regFragPtr, regTabPtr.p, fragId);
      return;
    }//if
    setUpDescriptorReferences(tableDescriptorRef, regTabPtr.p);
  } else {
    ljam();
    fragOperPtr.p->definingFragment = false;
  }//if
  signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
  signal->theData[1] = fragOperPtr.i;
  signal->theData[2] = regFragPtr.i;
  signal->theData[3] = fragId;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUPFRAGCONF, signal, 4, JBB);
  return;
}//Dbtup::execTUPFRAGREQ()

/* -------------------------------------------------------------------- */
/* ------------------------- ADDFRAGTOTAB ----------------------------- */
/* PUTS A FRAGMENT POINTER AND FID IN THE TABLE ARRAY OF THE TID RECORD */
/* -------------------------------------------------------------------- */
bool Dbtup::addfragtotab(Tablerec* const regTabPtr, Uint32 fragId, Uint32 fragIndex) 
{
  for (Uint32 i = 0; i < (2 * NO_OF_FRAG_PER_NODE); i++) {
    ljam();
    if (regTabPtr->fragid[i] == RNIL) {
      ljam();
      regTabPtr->fragid[i] = fragId;
      regTabPtr->fragrec[i] = fragIndex;
      return true;
    }//if
  }//for
  return false;
}//Dbtup::addfragtotab()

void Dbtup::getFragmentrec(FragrecordPtr& regFragPtr, Uint32 fragId, Tablerec* const regTabPtr) 
{
  for (Uint32 i = 0; i < (2 * NO_OF_FRAG_PER_NODE); i++) {
    ljam();
    if (regTabPtr->fragid[i] == fragId) {
      ljam();
/* ---------------------------------------------------------------- */
/* A FRAGMENT  RECORD HAVE BEEN FOUND FOR THIS OPERATION.           */
/* ---------------------------------------------------------------- */
      regFragPtr.i = regTabPtr->fragrec[i];
      ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
      return;
    }//if
  }//for
  regFragPtr.i = RNIL;
  ptrNull(regFragPtr);
}//Dbtup::getFragmentrec()

void Dbtup::seizeFragrecord(FragrecordPtr& regFragPtr) 
{
  regFragPtr.i = cfirstfreefrag;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
  cfirstfreefrag = regFragPtr.p->nextfreefrag;
  regFragPtr.p->nextfreefrag = RNIL;
}//Dbtup::seizeFragrecord()

/* ---------------------------------------------------------------- */
/* SEIZE A FRAGMENT OPERATION RECORD                                */
/* ---------------------------------------------------------------- */
void Dbtup::seizeFragoperrec(FragoperrecPtr& fragOperPtr) 
{
  fragOperPtr.i = cfirstfreeFragopr;
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);
  cfirstfreeFragopr = fragOperPtr.p->nextFragoprec;
  fragOperPtr.p->nextFragoprec = RNIL;
}//Dbtup::seizeFragoperrec()

/* **************************************************************** */
/* **************          TUP_ADD_ATTRREQ       ****************** */
/* **************************************************************** */
void Dbtup::execTUP_ADD_ATTRREQ(Signal* signal)
{
  FragrecordPtr regFragPtr;
  FragoperrecPtr fragOperPtr;
  TablerecPtr regTabPtr;

  ljamEntry();
  fragOperPtr.i = signal->theData[0];
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);
  Uint32 attrId = signal->theData[2];
  Uint32 attrDescriptor = signal->theData[3];

  regTabPtr.i = fragOperPtr.p->tableidFrag;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  Uint32 fragId = fragOperPtr.p->fragidFrag;

  getFragmentrec(regFragPtr, fragId, regTabPtr.p);
  ndbrequire(regFragPtr.i != RNIL);

  ndbrequire(fragOperPtr.p->attributeCount > 0);
  fragOperPtr.p->attributeCount--;

  if ((regTabPtr.p->tableStatus == DEFINING) &&
      (fragOperPtr.p->definingFragment)) {
    ljam();
    Uint32 firstTabDesIndex = regTabPtr.p->tabDescriptor + (attrId * ZAD_SIZE);
    setTabDescrWord(firstTabDesIndex, attrDescriptor);
    Uint32 attrLen = AttributeDescriptor::getSize(attrDescriptor);
    Uint32 nullBitPos = 0;                  /* Default pos for NOT NULL attributes */
    if (AttributeDescriptor::getNullable(attrDescriptor)) {
      if (!AttributeDescriptor::getDynamic(attrDescriptor)) {
        ljam();                                                      /* NULL ATTR */
        fragOperPtr.p->freeNullBit--;	                           /* STORE NULL BIT POSTITION */
        nullBitPos = fragOperPtr.p->freeNullBit;
        ndbrequire(fragOperPtr.p->freeNullBit < ZNIL);               /* Check not below zero */
      }//if
    } else {
      ljam();
      regTabPtr.p->notNullAttributeMask.set(attrId);
    }//if

    Uint32 attrDes2 = 0;
    if (!AttributeDescriptor::getDynamic(attrDescriptor)) {
      ljam();
      Uint32 attributePos = regTabPtr.p->tupheadsize;
      switch (AttributeDescriptor::getArrayType(attrDescriptor)) {
      case 1:
      case 2:
      {
        ljam();
        Uint32 bitsUsed = AttributeDescriptor::getArraySize(attrDescriptor) * (1 << attrLen);
        regTabPtr.p->tupheadsize += ((bitsUsed + 31) >> 5);
        break;
      }
      default:
        ndbrequire(false);
        break;
      }//switch
      AttributeOffset::setOffset(attrDes2, attributePos);
      AttributeOffset::setNullFlagPos(attrDes2, nullBitPos);
    } else {
      ndbrequire(false);
    }//if
    setTabDescrWord(firstTabDesIndex + 1, attrDes2);

    if (regTabPtr.p->tupheadsize > MAX_TUPLE_SIZE_IN_WORDS) {
      ljam();
      terrorCode = ZTOO_LARGE_TUPLE_ERROR;
      addattrrefuseLab(signal, regFragPtr, fragOperPtr, regTabPtr.p, fragId);
      return;
    }//if
    if ((fragOperPtr.p->attributeCount == 0) &&
        (fragOperPtr.p->freeNullBit != 0)) {
      ljam();
      terrorCode = ZINCONSISTENT_NULL_ATTRIBUTE_COUNT;
      addattrrefuseLab(signal, regFragPtr, fragOperPtr, regTabPtr.p, fragId);
      return;
    }//if
  }//if
/* **************************************************************** */
/* **************          TUP_ADD_ATTCONF       ****************** */
/* **************************************************************** */
  signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUP_ADD_ATTCONF, signal, 1, JBB);
  if (fragOperPtr.p->attributeCount > 0) {
    ljam();
    return;	/* EXIT AND WAIT FOR MORE */
  }//if
  regFragPtr.p->fragStatus = ACTIVE;
  if (regTabPtr.p->tableStatus == DEFINING) {
    ljam();
    setUpQueryRoutines(regTabPtr.p);
    setUpKeyArray(regTabPtr.p);
    regTabPtr.p->tableStatus = DEFINED;
  }//if
  releaseFragoperrec(fragOperPtr);
  return;
}//Dbtup::execTUP_ADD_ATTRREQ()

void Dbtup::setUpDescriptorReferences(Uint32 descriptorReference,
                                      Tablerec* const regTabPtr)
{
  Uint32 noOfAttributes = regTabPtr->noOfAttr;
  descriptorReference += ZTD_SIZE;
  ReadFunction * tmp = (ReadFunction*)&tableDescriptor[descriptorReference].tabDescr;
  regTabPtr->readFunctionArray = tmp;
  regTabPtr->updateFunctionArray = (UpdateFunction*)(tmp + noOfAttributes);

  TableDescriptor * start = &tableDescriptor[descriptorReference];
  TableDescriptor * end = (TableDescriptor*)(tmp + 2 * noOfAttributes);
  regTabPtr->readKeyArray = descriptorReference + (end - start);
  regTabPtr->attributeGroupDescriptor = regTabPtr->readKeyArray + regTabPtr->noOfKeyAttr;
  regTabPtr->tabDescriptor = regTabPtr->attributeGroupDescriptor + regTabPtr->noOfAttributeGroups;
}//Dbtup::setUpDescriptorReferences()

Uint32
Dbtup::sizeOfReadFunction()
{
  ReadFunction* tmp = (ReadFunction*)&tableDescriptor[0];
  TableDescriptor* start = &tableDescriptor[0];
  TableDescriptor * end = (TableDescriptor*)(tmp + 1);
  return (Uint32)(end - start);
}//Dbtup::sizeOfReadFunction()

void Dbtup::setUpKeyArray(Tablerec* const regTabPtr)
{
  ndbrequire((regTabPtr->readKeyArray + regTabPtr->noOfKeyAttr) < cnoOfTabDescrRec);
  Uint32* keyArray = &tableDescriptor[regTabPtr->readKeyArray].tabDescr;
  Uint32 countKeyAttr = 0;
  for (Uint32 i = 0; i < regTabPtr->noOfAttr; i++) {
    ljam();
    Uint32 refAttr = regTabPtr->tabDescriptor + (i * ZAD_SIZE);
    Uint32 attrDescriptor = getTabDescrWord(refAttr);
    if (AttributeDescriptor::getPrimaryKey(attrDescriptor)) {
      ljam();
      AttributeHeader::init(&keyArray[countKeyAttr], i, 0);
      countKeyAttr++;
    }//if
  }//for
  ndbrequire(countKeyAttr == regTabPtr->noOfKeyAttr);
}//Dbtup::setUpKeyArray()

void Dbtup::addattrrefuseLab(Signal* signal,
                             FragrecordPtr regFragPtr,
                             FragoperrecPtr fragOperPtr,
                             Tablerec* const regTabPtr,
                             Uint32 fragId) 
{
  releaseFragPages(regFragPtr.p);
  deleteFragTab(regTabPtr, fragId);
  releaseFragrec(regFragPtr);
  releaseTabDescr(regTabPtr);
  initTab(regTabPtr);

  signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
  signal->theData[1] = terrorCode;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUP_ADD_ATTRREF, signal, 2, JBB);
  releaseFragoperrec(fragOperPtr);
  return;
}//Dbtup::addattrrefuseLab()

void Dbtup::fragrefuse4Lab(Signal* signal,
                           FragoperrecPtr fragOperPtr,
                           FragrecordPtr regFragPtr,
                           Tablerec* const regTabPtr,
                           Uint32 fragId) 
{
  releaseFragPages(regFragPtr.p);
  fragrefuse3Lab(signal, fragOperPtr, regFragPtr, regTabPtr, fragId);
  initTab(regTabPtr);
  return;
}//Dbtup::fragrefuse4Lab()

void Dbtup::fragrefuse3Lab(Signal* signal,
                           FragoperrecPtr fragOperPtr,
                           FragrecordPtr regFragPtr,
                           Tablerec* const regTabPtr,
                           Uint32 fragId) 
{
  fragrefuse2Lab(signal, fragOperPtr, regFragPtr);
  deleteFragTab(regTabPtr, fragId);
  return;
}//Dbtup::fragrefuse3Lab()

void Dbtup::fragrefuse2Lab(Signal* signal, FragoperrecPtr fragOperPtr, FragrecordPtr regFragPtr) 
{
  fragrefuse1Lab(signal, fragOperPtr);
  releaseFragrec(regFragPtr);
  return;
}//Dbtup::fragrefuse2Lab()

void Dbtup::fragrefuse1Lab(Signal* signal, FragoperrecPtr fragOperPtr) 
{
  fragrefuseLab(signal, fragOperPtr);
  releaseFragoperrec(fragOperPtr);
  return;
}//Dbtup::fragrefuse1Lab()

void Dbtup::fragrefuseLab(Signal* signal, FragoperrecPtr fragOperPtr) 
{
  signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
  signal->theData[1] = terrorCode;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUPFRAGREF, signal, 2, JBB);
  return;
}//Dbtup::fragrefuseLab()

void Dbtup::releaseFragoperrec(FragoperrecPtr fragOperPtr) 
{
  fragOperPtr.p->nextFragoprec = cfirstfreeFragopr;
  cfirstfreeFragopr = fragOperPtr.i;
}//Dbtup::releaseFragoperrec()


void Dbtup::deleteFragTab(Tablerec* const regTabPtr, Uint32 fragId) 
{
  for (Uint32 i = 0; i < (2 * NO_OF_FRAG_PER_NODE); i++) {
    ljam();
    if (regTabPtr->fragid[i] == fragId) {
      ljam();
      regTabPtr->fragid[i] = RNIL;
      regTabPtr->fragrec[i] = RNIL;
      return;
    }//if
  }//for
  ndbrequire(false);
}//Dbtup::deleteFragTab()

void
Dbtup::execDROP_TAB_REQ(Signal* signal)
{
  ljamEntry();
  DropTabReq* req = (DropTabReq*)signal->getDataPtr();
  
  TablerecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  tabPtr.p->m_dropTable.tabUserRef = req->senderRef;
  tabPtr.p->m_dropTable.tabUserPtr = req->senderData;

  signal->theData[0] = ZREL_FRAG;
  signal->theData[1] = tabPtr.i;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
}//Dbtup::execDROP_TAB_REQ()

void Dbtup::releaseTabDescr(Tablerec* const regTabPtr) 
{
  Uint32 descriptor = regTabPtr->readKeyArray;
  if (descriptor != RNIL) {
    ljam();
    regTabPtr->tabDescriptor = RNIL;
    regTabPtr->readKeyArray = RNIL;
    regTabPtr->readFunctionArray = NULL;
    regTabPtr->updateFunctionArray = NULL;
    regTabPtr->attributeGroupDescriptor= RNIL;

    Uint32 sizeFunctionArrays = 2 * (regTabPtr->noOfAttr * sizeOfReadFunction());
    descriptor -= (sizeFunctionArrays + ZTD_SIZE);
    Uint32 retNo = getTabDescrWord(descriptor + ZTD_DATASIZE);
    ndbrequire(getTabDescrWord(descriptor + ZTD_HEADER) == ZTD_TYPE_NORMAL);
    ndbrequire(retNo == getTabDescrWord((descriptor + retNo) - ZTD_TR_SIZE));
    ndbrequire(ZTD_TYPE_NORMAL == getTabDescrWord((descriptor + retNo) - ZTD_TR_TYPE));
    freeTabDescr(descriptor, retNo);
  }//if
}//Dbtup::releaseTabDescr()

void Dbtup::releaseFragment(Signal* signal, Uint32 tableId)
{
  TablerecPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  Uint32 fragIndex = RNIL;
  Uint32 fragId = RNIL;
  Uint32 i = 0;
  for (i = 0; i < (2 * NO_OF_FRAG_PER_NODE); i++) {
    ljam();
    if (tabPtr.p->fragid[i] != RNIL) {
      ljam();
      fragIndex = tabPtr.p->fragrec[i];
      fragId = tabPtr.p->fragid[i];
      break;
    }//if
  }//for
  if (fragIndex != RNIL) {
    ljam();
    
    FragrecordPtr regFragPtr;
    regFragPtr.i = fragIndex;
    ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
    releaseFragPages(regFragPtr.p);

    tabPtr.p->fragid[i] = RNIL;
    tabPtr.p->fragrec[i] = RNIL;
    releaseFragrec(regFragPtr);

    signal->theData[0] = ZREL_FRAG;
    signal->theData[1] = tableId;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);  
    return;
  }//if
  
  /**
   * Finished...
   */
  sendFSREMOVEREQ(signal, tabPtr);
}//Dbtup::releaseFragment()

void Dbtup::sendFSREMOVEREQ(Signal* signal, TablerecPtr tabPtr)
{
  FsRemoveReq * const fsReq = (FsRemoveReq *)signal->getDataPtrSend();
  fsReq->userReference = cownref;
  fsReq->userPointer = tabPtr.i;
  fsReq->fileNumber[0] = tabPtr.i;
  fsReq->fileNumber[1] = (Uint32)-1; // Remove all fragments
  fsReq->fileNumber[2] = (Uint32)-1; // Remove all data files within fragment
  fsReq->fileNumber[3] = 255 |       // No P-value used here
    (5 <<  8) |  // Data-files in D5
    (0 << 16) | // Data-files
    (1 << 24);  // Version 1 of fileNumber
  
  fsReq->directory = 1;
  fsReq->ownDirectory = 1;
  sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
	     FsRemoveReq::SignalLength, JBA);  
}//Dbtup::sendFSREMOVEREQ()
 
void Dbtup::execFSREMOVECONF(Signal* signal)
{
  ljamEntry();
  
  FsConf * const fsConf = (FsConf *)signal->getDataPtrSend();
  TablerecPtr tabPtr;
  tabPtr.i = fsConf->userPointer;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  
  DropTabConf * const dropConf = (DropTabConf *)signal->getDataPtrSend();
  dropConf->senderRef = reference();
  dropConf->senderData = tabPtr.p->m_dropTable.tabUserPtr;
  dropConf->tableId = tabPtr.i;
  sendSignal(tabPtr.p->m_dropTable.tabUserRef, GSN_DROP_TAB_CONF,
             signal, DropTabConf::SignalLength, JBB);
  
  releaseTabDescr(tabPtr.p);
  initTab(tabPtr.p);
}//Dbtup::execFSREMOVECONF()

void Dbtup::execFSREMOVEREF(Signal* signal)
{
  ljamEntry();
  ndbrequire(false);
}//Dbtup::execFSREMOVEREF()


