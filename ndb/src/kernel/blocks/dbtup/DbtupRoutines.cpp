/* Copyright (C) 2003 MySQL AB

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


#define DBTUP_C
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <AttributeHeader.hpp>

#define ljam() { jamLine(3000 + __LINE__); }
#define ljamEntry() { jamEntryLine(3000 + __LINE__); }

void
Dbtup::setUpQueryRoutines(Tablerec* const regTabPtr)
{
  Uint32 startDescriptor = regTabPtr->tabDescriptor;
  ndbrequire((startDescriptor + (regTabPtr->noOfAttr << ZAD_LOG_SIZE)) <= cnoOfTabDescrRec);
  for (Uint32 i = 0; i < regTabPtr->noOfAttr; i++) {
    Uint32 attrDescriptorStart = startDescriptor + (i << ZAD_LOG_SIZE);
    Uint32 attrDescriptor = tableDescriptor[attrDescriptorStart].tabDescr;
    Uint32 attrOffset = tableDescriptor[attrDescriptorStart + 1].tabDescr;
    if (!AttributeDescriptor::getDynamic(attrDescriptor)) {
      if ((AttributeDescriptor::getArrayType(attrDescriptor) == ZNON_ARRAY) ||
          (AttributeDescriptor::getArrayType(attrDescriptor) == ZFIXED_ARRAY)) {
        if (!AttributeDescriptor::getNullable(attrDescriptor)) {
	  if (AttributeDescriptor::getSize(attrDescriptor) == 0){
	    ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readBitsNotNULL;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateBitsNotNULL;
	  } else if (AttributeDescriptor::getSizeInWords(attrDescriptor) == 1){
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHOneWordNotNULL;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHOneWordNotNULL;
          } else if (AttributeDescriptor::getSizeInWords(attrDescriptor) == 2) {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHTwoWordNotNULL;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHTwoWordNotNULL;
          } else if (AttributeDescriptor::getSizeInWords(attrDescriptor) > 2) {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHManyWordNotNULL;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHManyWordNotNULL;
          } else {
            ndbrequire(false);
          }//if
          // replace functions for char attribute
          if (AttributeOffset::getCharsetFlag(attrOffset)) {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHManyWordNotNULL;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHManyWordNotNULL;
          }
        } else {
	  if (AttributeDescriptor::getSize(attrDescriptor) == 0){
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readBitsNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateBitsNULLable;
	  } else if (AttributeDescriptor::getSizeInWords(attrDescriptor) == 1){
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHOneWordNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHManyWordNULLable;
          } else if (AttributeDescriptor::getSizeInWords(attrDescriptor) == 2) {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHTwoWordNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHManyWordNULLable;
          } else if (AttributeDescriptor::getSizeInWords(attrDescriptor) > 2) {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHManyWordNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHManyWordNULLable;
          } else {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHZeroWordNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHManyWordNULLable;
          }//if
          // replace functions for char attribute
          if (AttributeOffset::getCharsetFlag(attrOffset)) {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readFixedSizeTHManyWordNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateFixedSizeTHManyWordNULLable;
          }
        }//if
      } else if (AttributeDescriptor::getArrayType(attrDescriptor) == ZVAR_ARRAY) {
        if (!AttributeDescriptor::getNullable(attrDescriptor)) {
          if (AttributeDescriptor::getArraySize(attrDescriptor) == 0) {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readVarSizeUnlimitedNotNULL;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateVarSizeUnlimitedNotNULL;
          } else if (AttributeDescriptor::getArraySize(attrDescriptor) > ZMAX_SMALL_VAR_ARRAY) {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readBigVarSizeNotNULL;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateBigVarSizeNotNULL;
          } else {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readSmallVarSizeNotNULL;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateSmallVarSizeNotNULL;
          }//if
        } else {
          if (AttributeDescriptor::getArraySize(attrDescriptor) == 0) {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readVarSizeUnlimitedNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateVarSizeUnlimitedNULLable;
          } else if (AttributeDescriptor::getArraySize(attrDescriptor) > ZMAX_SMALL_VAR_ARRAY) {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readBigVarSizeNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateBigVarSizeNULLable;
          } else {
            ljam();
            regTabPtr->readFunctionArray[i] = &Dbtup::readSmallVarSizeNULLable;
            regTabPtr->updateFunctionArray[i] = &Dbtup::updateSmallVarSizeNULLable;
          }//if
        }//if
      } else {
        ndbrequire(false);
      }//if
    } else {
      if ((AttributeDescriptor::getArrayType(attrDescriptor) == ZNON_ARRAY) ||
          (AttributeDescriptor::getArrayType(attrDescriptor) == ZFIXED_ARRAY)) {
        ljam();
        regTabPtr->readFunctionArray[i] = &Dbtup::readDynFixedSize;
        regTabPtr->updateFunctionArray[i] = &Dbtup::updateDynFixedSize;
      } else if (AttributeDescriptor::getType(attrDescriptor) == ZVAR_ARRAY) {
        if (AttributeDescriptor::getArraySize(attrDescriptor) == 0) {
          ljam();
          regTabPtr->readFunctionArray[i] = &Dbtup::readDynVarSizeUnlimited;
          regTabPtr->updateFunctionArray[i] = &Dbtup::updateDynVarSizeUnlimited;
        } else if (AttributeDescriptor::getArraySize(attrDescriptor) > ZMAX_SMALL_VAR_ARRAY) {
          ljam();
          regTabPtr->readFunctionArray[i] = &Dbtup::readDynBigVarSize;
          regTabPtr->updateFunctionArray[i] = &Dbtup::updateDynBigVarSize;
        } else {
          ljam();
          regTabPtr->readFunctionArray[i] = &Dbtup::readDynSmallVarSize;
          regTabPtr->updateFunctionArray[i] = &Dbtup::updateDynSmallVarSize;
        }//if
      } else {
        ndbrequire(false);
      }//if
    }//if
  }//for
}//Dbtup::setUpQueryRoutines()

/* ---------------------------------------------------------------- */
/*       THIS ROUTINE IS USED TO READ A NUMBER OF ATTRIBUTES IN THE */
/*       DATABASE AND PLACE THE RESULT IN ATTRINFO RECORDS.         */
//
// In addition to the parameters used in the call it also relies on the
// following variables set-up properly.
//
// operPtr.p      Operation record pointer
// fragptr.p      Fragment record pointer
// tabptr.p       Table record pointer
/* ---------------------------------------------------------------- */
int Dbtup::readAttributes(Page* const pagePtr,
			  Uint32  tupHeadOffset,
			  const Uint32* inBuffer,
			  Uint32  inBufLen,
			  Uint32* outBuffer,
			  Uint32  maxRead,
			  bool    xfrmFlag)
{
  Tablerec* const regTabPtr =  tabptr.p;
  Uint32 numAttributes = regTabPtr->noOfAttr;
  Uint32 attrDescriptorStart = regTabPtr->tabDescriptor;
  Uint32 inBufIndex = 0;

  ndbrequire(attrDescriptorStart + (numAttributes << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);

  tOutBufIndex = 0;
  tCheckOffset = regTabPtr->tupheadsize;
  tMaxRead = maxRead;
  tTupleHeader = &pagePtr->pageWord[tupHeadOffset];
  tXfrmFlag = xfrmFlag;

  ndbrequire(tupHeadOffset + tCheckOffset <= ZWORDS_ON_PAGE);
  while (inBufIndex < inBufLen) {
    Uint32 tmpAttrBufIndex = tOutBufIndex;
    AttributeHeader ahIn(inBuffer[inBufIndex]);
    inBufIndex++;
    Uint32 attributeId = ahIn.getAttributeId();
    Uint32 attrDescriptorIndex = attrDescriptorStart + (attributeId << ZAD_LOG_SIZE);
    ljam();

    AttributeHeader::init(&outBuffer[tmpAttrBufIndex], attributeId, 0);
    AttributeHeader* ahOut = (AttributeHeader*)&outBuffer[tmpAttrBufIndex];
    tOutBufIndex = tmpAttrBufIndex + 1;
    if (attributeId < numAttributes) {
      Uint32 attributeDescriptor = tableDescriptor[attrDescriptorIndex].tabDescr;
      Uint32 attributeOffset = tableDescriptor[attrDescriptorIndex + 1].tabDescr;
      ReadFunction f = regTabPtr->readFunctionArray[attributeId];
      if ((this->*f)(outBuffer,
                     ahOut,
                     attributeDescriptor,
                     attributeOffset)) {
        continue;
      } else {
        return -1;
      }//if
    } else if(attributeId & AttributeHeader::PSUEDO){
      Uint32 sz = read_psuedo(attributeId, 
			      outBuffer+tmpAttrBufIndex+1);
      AttributeHeader::init(&outBuffer[tmpAttrBufIndex], attributeId, sz);
      tOutBufIndex = tmpAttrBufIndex + 1 + sz;
    } else {
      terrorCode = ZATTRIBUTE_ID_ERROR;
      return -1;
    }//if
  }//while
  return tOutBufIndex;
}//Dbtup::readAttributes()

#if 0
int Dbtup::readAttributesWithoutHeader(Page* const pagePtr,
                                       Uint32  tupHeadOffset,
                                       Uint32* inBuffer,
                                       Uint32  inBufLen,
                                       Uint32* outBuffer,
                                       Uint32* attrBuffer,
                                       Uint32  maxRead)
{
  Tablerec* const regTabPtr =  tabptr.p;
  Uint32 numAttributes = regTabPtr->noOfAttr;
  Uint32 attrDescriptorStart = regTabPtr->tabDescriptor;
  Uint32 inBufIndex = 0;
  Uint32 attrBufIndex = 0;

  ndbrequire(attrDescriptorStart + (numAttributes << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);

  tOutBufIndex = 0;
  tCheckOffset = regTabPtr->tupheadsize;
  tMaxRead = maxRead;
  tTupleHeader = &pagePtr->pageWord[tupHeadOffset];

  ndbrequire(tupHeadOffset + tCheckOffset <= ZWORDS_ON_PAGE);
  while (inBufIndex < inBufLen) {
    AttributeHeader ahIn(inBuffer[inBufIndex]);
    inBufIndex++;
    Uint32 attributeId = ahIn.getAttributeId();
    Uint32 attrDescriptorIndex = attrDescriptorStart + (attributeId << ZAD_LOG_SIZE);
    ljam();

    AttributeHeader::init(&attrBuffer[attrBufIndex], attributeId, 0);
    AttributeHeader* ahOut = (AttributeHeader*)&attrBuffer[attrBufIndex];
    attrBufIndex++;
    if (attributeId < numAttributes) {
      Uint32 attributeDescriptor = tableDescriptor[attrDescriptorIndex].tabDescr;
      Uint32 attributeOffset = tableDescriptor[attrDescriptorIndex + 1].tabDescr;
      ReadFunction f = regTabPtr->readFunctionArray[attributeId];
      if ((this->*f)(outBuffer,
                     ahOut,
                     attributeDescriptor,
                     attributeOffset)) {
        continue;
      } else {
        return -1;
      }//if
    } else {
      terrorCode = ZATTRIBUTE_ID_ERROR;
      return -1;
    }//if
  }//while
  ndbrequire(attrBufIndex == inBufLen);
  return tOutBufIndex;
}//Dbtup::readAttributes()
#endif

bool
Dbtup::readFixedSizeTHOneWordNotNULL(Uint32* outBuffer,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDescriptor,
                                     Uint32  attrDes2)
{
  Uint32 indexBuf = tOutBufIndex;
  Uint32 readOffset = AttributeOffset::getOffset(attrDes2);
  Uint32 const wordRead = tTupleHeader[readOffset];
  Uint32 newIndexBuf = indexBuf + 1;
  Uint32 maxRead = tMaxRead;

  ndbrequire(readOffset < tCheckOffset);
  if (newIndexBuf <= maxRead) {
    ljam();
    outBuffer[indexBuf] = wordRead;
    ahOut->setDataSize(1);
    tOutBufIndex = newIndexBuf;
    return true;
  } else {
    ljam();
    terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }//if
}//Dbtup::readFixedSizeTHOneWordNotNULL()

bool
Dbtup::readFixedSizeTHTwoWordNotNULL(Uint32* outBuffer,
                                     AttributeHeader* ahOut,
                                     Uint32  attrDescriptor,
                                     Uint32  attrDes2)
{
  Uint32 indexBuf = tOutBufIndex;
  Uint32 readOffset = AttributeOffset::getOffset(attrDes2);
  Uint32 const wordReadFirst = tTupleHeader[readOffset];
  Uint32 const wordReadSecond = tTupleHeader[readOffset + 1];
  Uint32 newIndexBuf = indexBuf + 2;
  Uint32 maxRead = tMaxRead;

  ndbrequire(readOffset + 1 < tCheckOffset);
  if (newIndexBuf <= maxRead) {
    ljam();
    ahOut->setDataSize(2);
    outBuffer[indexBuf] = wordReadFirst;
    outBuffer[indexBuf + 1] = wordReadSecond;
    tOutBufIndex = newIndexBuf;
    return true;
  } else {
    ljam();
    terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }//if
}//Dbtup::readFixedSizeTHTwoWordNotNULL()

bool
Dbtup::readFixedSizeTHManyWordNotNULL(Uint32* outBuffer,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDescriptor,
                                      Uint32  attrDes2)
{
  Uint32 indexBuf = tOutBufIndex;
  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attrDes2);
  Uint32 readOffset = AttributeOffset::getOffset(attrDes2);
  Uint32 attrNoOfWords = AttributeDescriptor::getSizeInWords(attrDescriptor);
  Uint32 maxRead = tMaxRead;

  ndbrequire((readOffset + attrNoOfWords - 1) < tCheckOffset);
  if (! charsetFlag || ! tXfrmFlag) {
    Uint32 newIndexBuf = indexBuf + attrNoOfWords;
    if (newIndexBuf <= maxRead) {
      ljam();
      ahOut->setDataSize(attrNoOfWords);
      MEMCOPY_NO_WORDS(&outBuffer[indexBuf],
                       &tTupleHeader[readOffset],
                       attrNoOfWords);
      tOutBufIndex = newIndexBuf;
      return true;
    } else {
      ljam();
      terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    }//if
  } else {
    ljam();
    Tablerec* regTabPtr = tabptr.p;
    Uint32 srcBytes = AttributeDescriptor::getSizeInBytes(attrDescriptor);
    uchar* dstPtr = (uchar*)&outBuffer[indexBuf];
    const uchar* srcPtr = (uchar*)&tTupleHeader[readOffset];
    Uint32 i = AttributeOffset::getCharsetPos(attrDes2);
    ndbrequire(i < regTabPtr->noOfCharsets);
    CHARSET_INFO* cs = regTabPtr->charsetArray[i];
    Uint32 typeId = AttributeDescriptor::getType(attrDescriptor);
    Uint32 lb, len;
    bool ok = NdbSqlUtil::get_var_length(typeId, srcPtr, srcBytes, lb, len);
    if (ok) {
      Uint32 xmul = cs->strxfrm_multiply;
      if (xmul == 0)
        xmul = 1;
      // see comment in DbtcMain.cpp
      Uint32 dstLen = xmul * (srcBytes - lb);
      Uint32 maxIndexBuf = indexBuf + (dstLen >> 2);
      if (maxIndexBuf <= maxRead) {
        ljam();
        int n = NdbSqlUtil::strnxfrm_bug7284(cs, dstPtr, dstLen, srcPtr + lb, len);
        ndbrequire(n != -1);
        while ((n & 3) != 0) {
          dstPtr[n++] = 0;
        }
        Uint32 dstWords = (n >> 2);
        ahOut->setDataSize(dstWords);
        Uint32 newIndexBuf = indexBuf + dstWords;
        ndbrequire(newIndexBuf <= maxRead);
        tOutBufIndex = newIndexBuf;
        return true;
      } else {
        ljam();
        terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
      }
    } else {
      ljam();
      terrorCode = ZTUPLE_CORRUPTED_ERROR;
    }
  }
  return false;
}//Dbtup::readFixedSizeTHManyWordNotNULL()

bool
Dbtup::readFixedSizeTHOneWordNULLable(Uint32* outBuffer,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDescriptor,
                                      Uint32  attrDes2)
{
  if (!nullFlagCheck(attrDes2)) {
    ljam();
    return readFixedSizeTHOneWordNotNULL(outBuffer,
                                         ahOut,
                                         attrDescriptor,
                                         attrDes2);
  } else {
    ljam();
    ahOut->setNULL();
    return true;
  }//if
}//Dbtup::readFixedSizeTHOneWordNULLable()

bool
Dbtup::readFixedSizeTHTwoWordNULLable(Uint32* outBuffer,
                                      AttributeHeader* ahOut,
                                      Uint32  attrDescriptor,
                                      Uint32  attrDes2)
{
  if (!nullFlagCheck(attrDes2)) {
    ljam();
    return readFixedSizeTHTwoWordNotNULL(outBuffer,
                                         ahOut,
                                         attrDescriptor,
                                         attrDes2);
  } else {
    ljam();
    ahOut->setNULL();
    return true;
  }//if
}//Dbtup::readFixedSizeTHTwoWordNULLable()

bool
Dbtup::readFixedSizeTHManyWordNULLable(Uint32* outBuffer,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDescriptor,
                                       Uint32  attrDes2)
{
  if (!nullFlagCheck(attrDes2)) {
    ljam();
    return readFixedSizeTHManyWordNotNULL(outBuffer,
                                          ahOut,
                                          attrDescriptor,
                                          attrDes2);
  } else {
    ljam();
    ahOut->setNULL();
    return true;
  }//if
}//Dbtup::readFixedSizeTHManyWordNULLable()

bool
Dbtup::readFixedSizeTHZeroWordNULLable(Uint32* outBuffer,
                                       AttributeHeader* ahOut,
                                       Uint32  attrDescriptor,
                                       Uint32  attrDes2)
{
  ljam();
  if (nullFlagCheck(attrDes2)) {
    ljam();
    ahOut->setNULL();
  }//if
  return true;
}//Dbtup::readFixedSizeTHZeroWordNULLable()

bool
Dbtup::nullFlagCheck(Uint32  attrDes2)
{
  Tablerec* const regTabPtr = tabptr.p;
  Uint32 nullFlagOffsetInTuple = AttributeOffset::getNullFlagOffset(attrDes2);
  ndbrequire(nullFlagOffsetInTuple < regTabPtr->tupNullWords);
  nullFlagOffsetInTuple += regTabPtr->tupNullIndex;
  ndbrequire(nullFlagOffsetInTuple < tCheckOffset);

  return (AttributeOffset::isNULL(tTupleHeader[nullFlagOffsetInTuple], attrDes2));
}//Dbtup::nullFlagCheck()

bool
Dbtup::readVariableSizedAttr(Uint32* outBuffer,
                             AttributeHeader* ahOut,
                             Uint32  attrDescriptor,
                             Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readVariableSizedAttr()

bool
Dbtup::readVarSizeUnlimitedNotNULL(Uint32* outBuffer,
                                   AttributeHeader* ahOut,
                                   Uint32  attrDescriptor,
                                   Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readVarSizeUnlimitedNotNULL()

bool
Dbtup::readVarSizeUnlimitedNULLable(Uint32* outBuffer,
                                    AttributeHeader* ahOut,
                                    Uint32  attrDescriptor,
                                    Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readVarSizeUnlimitedNULLable()

bool
Dbtup::readBigVarSizeNotNULL(Uint32* outBuffer,
                             AttributeHeader* ahOut,
                             Uint32  attrDescriptor,
                             Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readBigVarSizeNotNULL()

bool
Dbtup::readBigVarSizeNULLable(Uint32* outBuffer,
                             AttributeHeader* ahOut,
                             Uint32  attrDescriptor,
                             Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readBigVarSizeNULLable()

bool
Dbtup::readSmallVarSizeNotNULL(Uint32* outBuffer,
                               AttributeHeader* ahOut,
                               Uint32  attrDescriptor,
                               Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readSmallVarSizeNotNULL()

bool
Dbtup::readSmallVarSizeNULLable(Uint32* outBuffer,
                                AttributeHeader* ahOut,
                                Uint32  attrDescriptor,
                                Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readSmallVarSizeNULLable()

bool
Dbtup::readDynFixedSize(Uint32* outBuffer,
                        AttributeHeader* ahOut,
                        Uint32  attrDescriptor,
                        Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readDynFixedSize()

bool
Dbtup::readDynVarSizeUnlimited(Uint32* outBuffer,
                               AttributeHeader* ahOut,
                               Uint32  attrDescriptor,
                               Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readDynVarSizeUnlimited()

bool
Dbtup::readDynBigVarSize(Uint32* outBuffer,
                         AttributeHeader* ahOut,
                         Uint32  attrDescriptor,
                         Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readDynBigVarSize()

bool
Dbtup::readDynSmallVarSize(Uint32* outBuffer,
                           AttributeHeader* ahOut,
                           Uint32  attrDescriptor,
                           Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::readDynSmallVarSize()

/* ---------------------------------------------------------------------- */
/*       THIS ROUTINE IS USED TO UPDATE A NUMBER OF ATTRIBUTES. IT IS     */
/*       USED BY THE INSERT ROUTINE, THE UPDATE ROUTINE AND IT CAN BE     */
/*       CALLED SEVERAL TIMES FROM THE INTERPRETER.                       */
// In addition to the parameters used in the call it also relies on the
// following variables set-up properly.
//
// pagep.p        Page record pointer
// fragptr.p      Fragment record pointer
// operPtr.p      Operation record pointer
// tabptr.p       Table record pointer
/* ---------------------------------------------------------------------- */
int Dbtup::updateAttributes(Page* const pagePtr,
                            Uint32 tupHeadOffset,
                            Uint32* inBuffer,
                            Uint32 inBufLen)
{
  Tablerec* const regTabPtr =  tabptr.p;
  Operationrec* const regOperPtr = operPtr.p;
  Uint32 numAttributes = regTabPtr->noOfAttr;
  Uint32 attrDescriptorStart = regTabPtr->tabDescriptor;
  ndbrequire(attrDescriptorStart + (numAttributes << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);

  tCheckOffset = regTabPtr->tupheadsize;
  tTupleHeader = &pagePtr->pageWord[tupHeadOffset];
  Uint32 inBufIndex = 0;
  tInBufIndex = 0;
  tInBufLen = inBufLen;

  ndbrequire(tupHeadOffset + tCheckOffset <= ZWORDS_ON_PAGE);
  while (inBufIndex < inBufLen) {
    AttributeHeader ahIn(inBuffer[inBufIndex]);
    Uint32 attributeId = ahIn.getAttributeId();
    Uint32 attrDescriptorIndex = attrDescriptorStart + (attributeId << ZAD_LOG_SIZE);
    if (attributeId < numAttributes) {
      Uint32 attrDescriptor = tableDescriptor[attrDescriptorIndex].tabDescr;
      Uint32 attributeOffset = tableDescriptor[attrDescriptorIndex + 1].tabDescr;
      if ((AttributeDescriptor::getPrimaryKey(attrDescriptor)) &&
          (regOperPtr->optype != ZINSERT)) {
        if (checkUpdateOfPrimaryKey(&inBuffer[inBufIndex], regTabPtr)) {
          ljam();
          terrorCode = ZTRY_UPDATE_PRIMARY_KEY;
          return -1;
        }//if
      }//if
      UpdateFunction f = regTabPtr->updateFunctionArray[attributeId];
      ljam();
      regOperPtr->changeMask.set(attributeId);
      if ((this->*f)(inBuffer,
                     attrDescriptor,
                     attributeOffset)) {
        inBufIndex = tInBufIndex;
        continue;
      } else {
        ljam();
        return -1;
      }//if
    } else {
      ljam();
      terrorCode = ZATTRIBUTE_ID_ERROR;
      return -1;
    }//if
  }//while
  return 0;
}//Dbtup::updateAttributes()

bool
Dbtup::checkUpdateOfPrimaryKey(Uint32* updateBuffer, Tablerec* const regTabPtr)
{
  Uint32 keyReadBuffer[MAX_KEY_SIZE_IN_WORDS];
  AttributeHeader ahIn(*updateBuffer);
  Uint32 attributeId = ahIn.getAttributeId();
  Uint32 attrDescriptorIndex = regTabPtr->tabDescriptor + (attributeId << ZAD_LOG_SIZE);
  Uint32 attrDescriptor = tableDescriptor[attrDescriptorIndex].tabDescr;
  Uint32 attributeOffset = tableDescriptor[attrDescriptorIndex + 1].tabDescr;

  Uint32 xfrmBuffer[1 + MAX_KEY_SIZE_IN_WORDS * MAX_XFRM_MULTIPLY];
  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attributeOffset);
  if (charsetFlag) {
    Uint32 csIndex = AttributeOffset::getCharsetPos(attributeOffset);
    CHARSET_INFO* cs = regTabPtr->charsetArray[csIndex];
    Uint32 srcPos = 0;
    Uint32 dstPos = 0;
    xfrm_attr(attrDescriptor, cs, &updateBuffer[1], srcPos,
              &xfrmBuffer[1], dstPos, MAX_KEY_SIZE_IN_WORDS * MAX_XFRM_MULTIPLY);
    ahIn.setDataSize(dstPos);
    xfrmBuffer[0] = ahIn.m_value;
    updateBuffer = xfrmBuffer;
  }

  ReadFunction f = regTabPtr->readFunctionArray[attributeId];

  AttributeHeader attributeHeader(attributeId, 0);
  tOutBufIndex = 0;
  tMaxRead = MAX_KEY_SIZE_IN_WORDS;

  bool tmp = tXfrmFlag;
  tXfrmFlag = true;
  ndbrequire((this->*f)(&keyReadBuffer[0], &attributeHeader, attrDescriptor,
                        attributeOffset));
  tXfrmFlag = tmp;
  ndbrequire(tOutBufIndex == attributeHeader.getDataSize());
  if (ahIn.getDataSize() != attributeHeader.getDataSize()) {
    ljam();
    return true;
  }//if
  if (memcmp(&keyReadBuffer[0], &updateBuffer[1], tOutBufIndex << 2) != 0) {
    ljam();
    return true;
  }//if
  return false;
}//Dbtup::checkUpdateOfPrimaryKey()

#if 0
void Dbtup::checkPages(Fragrecord* const regFragPtr)
{
  Uint32 noPages = getNoOfPages(regFragPtr);
  for (Uint32 i = 0; i < noPages ; i++) {
    PagePtr pagePtr;
    pagePtr.i = getRealpid(regFragPtr, i);
    ptrCheckGuard(pagePtr, cnoOfPage, page);
    ndbrequire(pagePtr.p->pageWord[1] != (RNIL - 1));
  }
}
#endif

bool
Dbtup::updateFixedSizeTHOneWordNotNULL(Uint32* inBuffer,
                                       Uint32  attrDescriptor,
                                       Uint32  attrDes2)
{
  Uint32 indexBuf = tInBufIndex;
  Uint32 inBufLen = tInBufLen;
  Uint32 updateOffset = AttributeOffset::getOffset(attrDes2);
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 nullIndicator = ahIn.isNULL();
  Uint32 newIndex = indexBuf + 2;
  ndbrequire(updateOffset < tCheckOffset);

  if (newIndex <= inBufLen) {
    Uint32 updateWord = inBuffer[indexBuf + 1];
    if (!nullIndicator) {
      ljam();
      tInBufIndex = newIndex;
      tTupleHeader[updateOffset] = updateWord;
      return true;
    } else {
      ljam();
      terrorCode = ZNOT_NULL_ATTR;
      return false;
    }//if
  } else {
    ljam();
    terrorCode = ZAI_INCONSISTENCY_ERROR;
    return false;
  }//if
  return true;
}//Dbtup::updateFixedSizeTHOneWordNotNULL()

bool
Dbtup::updateFixedSizeTHTwoWordNotNULL(Uint32* inBuffer,
                                       Uint32  attrDescriptor,
                                       Uint32  attrDes2)
{
  Uint32 indexBuf = tInBufIndex;
  Uint32 inBufLen = tInBufLen;
  Uint32 updateOffset = AttributeOffset::getOffset(attrDes2);
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 nullIndicator = ahIn.isNULL();
  Uint32 newIndex = indexBuf + 3;
  ndbrequire((updateOffset + 1) < tCheckOffset);

  if (newIndex <= inBufLen) {
    Uint32 updateWord1 = inBuffer[indexBuf + 1];
    Uint32 updateWord2 = inBuffer[indexBuf + 2];
    if (!nullIndicator) {
      ljam();
      tInBufIndex = newIndex;
      tTupleHeader[updateOffset] = updateWord1;
      tTupleHeader[updateOffset + 1] = updateWord2;
      return true;
    } else {
      ljam();
      terrorCode = ZNOT_NULL_ATTR;
      return false;
    }//if
  } else {
    ljam();
    terrorCode = ZAI_INCONSISTENCY_ERROR;
    return false;
  }//if
}//Dbtup::updateFixedSizeTHTwoWordNotNULL()

bool
Dbtup::updateFixedSizeTHManyWordNotNULL(Uint32* inBuffer,
                                        Uint32  attrDescriptor,
                                        Uint32  attrDes2)
{
  Uint32 indexBuf = tInBufIndex;
  Uint32 inBufLen = tInBufLen;
  Uint32 updateOffset = AttributeOffset::getOffset(attrDes2);
  Uint32 charsetFlag = AttributeOffset::getCharsetFlag(attrDes2);
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 nullIndicator = ahIn.isNULL();
  Uint32 noOfWords = AttributeDescriptor::getSizeInWords(attrDescriptor);
  Uint32 newIndex = indexBuf + noOfWords + 1;
  ndbrequire((updateOffset + noOfWords - 1) < tCheckOffset);

  if (newIndex <= inBufLen) {
    if (!nullIndicator) {
      ljam();
      if (charsetFlag) {
        ljam();
        Tablerec* regTabPtr = tabptr.p;
	Uint32 typeId = AttributeDescriptor::getType(attrDescriptor);
        Uint32 bytes = AttributeDescriptor::getSizeInBytes(attrDescriptor);
        Uint32 i = AttributeOffset::getCharsetPos(attrDes2);
        ndbrequire(i < regTabPtr->noOfCharsets);
        // not const in MySQL
        CHARSET_INFO* cs = regTabPtr->charsetArray[i];
        int not_used;
        const char* ssrc = (const char*)&inBuffer[tInBufIndex + 1];
        Uint32 lb, len;
        if (! NdbSqlUtil::get_var_length(typeId, ssrc, bytes, lb, len)) {
          ljam();
          terrorCode = ZINVALID_CHAR_FORMAT;
          return false;
        }
	// fast fix bug#7340
        if (typeId != NDB_TYPE_TEXT &&
	    (*cs->cset->well_formed_len)(cs, ssrc + lb, ssrc + lb + len, ZNIL, &not_used) != len) {
          ljam();
          terrorCode = ZINVALID_CHAR_FORMAT;
          return false;
        }
      }
      tInBufIndex = newIndex;
      MEMCOPY_NO_WORDS(&tTupleHeader[updateOffset],
                       &inBuffer[indexBuf + 1],
                       noOfWords);
      return true;
    } else {
      ljam();
      terrorCode = ZNOT_NULL_ATTR;
      return false;
    }//if
  } else {
    ljam();
    terrorCode = ZAI_INCONSISTENCY_ERROR;
    return false;
  }//if
}//Dbtup::updateFixedSizeTHManyWordNotNULL()

bool
Dbtup::updateFixedSizeTHManyWordNULLable(Uint32* inBuffer,
                                         Uint32  attrDescriptor,
                                         Uint32  attrDes2)
{
  Tablerec* const regTabPtr =  tabptr.p;
  AttributeHeader ahIn(inBuffer[tInBufIndex]);
  Uint32 nullIndicator = ahIn.isNULL();
  Uint32 nullFlagOffset = AttributeOffset::getNullFlagOffset(attrDes2);
  Uint32 nullFlagBitOffset = AttributeOffset::getNullFlagBitOffset(attrDes2);
  Uint32 nullWordOffset = nullFlagOffset + regTabPtr->tupNullIndex;
  ndbrequire((nullFlagOffset < regTabPtr->tupNullWords) &&
             (nullWordOffset < tCheckOffset));
  Uint32 nullBits = tTupleHeader[nullWordOffset];

  if (!nullIndicator) {
    nullBits &= (~(1 << nullFlagBitOffset));
    ljam();
    tTupleHeader[nullWordOffset] = nullBits;
    return updateFixedSizeTHManyWordNotNULL(inBuffer,
                                            attrDescriptor,
                                            attrDes2);
  } else {
    Uint32 newIndex = tInBufIndex + 1;
    if (newIndex <= tInBufLen) {
      nullBits |= (1 << nullFlagBitOffset);
      ljam();
      tTupleHeader[nullWordOffset] = nullBits;
      tInBufIndex = newIndex;
      return true;
    } else {
      ljam();
      terrorCode = ZAI_INCONSISTENCY_ERROR;
      return false;
    }//if
  }//if
}//Dbtup::updateFixedSizeTHManyWordNULLable()

bool
Dbtup::updateVariableSizedAttr(Uint32* inBuffer,
                               Uint32  attrDescriptor,
                               Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::updateVariableSizedAttr()

bool
Dbtup::updateVarSizeUnlimitedNotNULL(Uint32* inBuffer,
                                     Uint32  attrDescriptor,
                                     Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::updateVarSizeUnlimitedNotNULL()

bool
Dbtup::updateVarSizeUnlimitedNULLable(Uint32* inBuffer,
                                      Uint32  attrDescriptor,
                                      Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::updateVarSizeUnlimitedNULLable()

bool
Dbtup::updateBigVarSizeNotNULL(Uint32* inBuffer,
                               Uint32  attrDescriptor,
                               Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::updateBigVarSizeNotNULL()

bool
Dbtup::updateBigVarSizeNULLable(Uint32* inBuffer,
                                Uint32  attrDescriptor,
                                Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::updateBigVarSizeNULLable()

bool
Dbtup::updateSmallVarSizeNotNULL(Uint32* inBuffer,
                                 Uint32  attrDescriptor,
                                 Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::updateSmallVarSizeNotNULL()

bool
Dbtup::updateSmallVarSizeNULLable(Uint32* inBuffer,
                                  Uint32  attrDescriptor,
                                  Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::updateSmallVarSizeNULLable()

bool
Dbtup::updateDynFixedSize(Uint32* inBuffer,
                          Uint32  attrDescriptor,
                          Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::updateDynFixedSize()

bool
Dbtup::updateDynVarSizeUnlimited(Uint32* inBuffer,
                                 Uint32  attrDescriptor,
                                 Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::updateDynVarSizeUnlimited()

bool
Dbtup::updateDynBigVarSize(Uint32* inBuffer,
                           Uint32  attrDescriptor,
                           Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::updateDynBigVarSize()

bool
Dbtup::updateDynSmallVarSize(Uint32* inBuffer,
                             Uint32  attrDescriptor,
                             Uint32  attrDes2)
{
  ljam();
  terrorCode = ZVAR_SIZED_NOT_SUPPORTED;
  return false;
}//Dbtup::updateDynSmallVarSize()

Uint32 
Dbtup::read_psuedo(Uint32 attrId, Uint32* outBuffer){
  Uint32 tmp[sizeof(SignalHeader)+25];
  Signal * signal = (Signal*)&tmp;
  switch(attrId){
  case AttributeHeader::FRAGMENT:
    * outBuffer = operPtr.p->fragId >> 1; // remove "hash" bit
    return 1;
  case AttributeHeader::FRAGMENT_MEMORY:
    {
      Uint64 tmp = 0;
      tmp += fragptr.p->noOfPages;
      {
        /**
         * Each fragment is split into 2...get #pages from other as well
         */
        Uint32 twin = fragptr.p->fragmentId ^ 1;
        FragrecordPtr twinPtr;
        getFragmentrec(twinPtr, twin, tabptr.p);
        ndbrequire(twinPtr.p != 0);
        tmp += twinPtr.p->noOfPages;
      }
      tmp *= 32768;
      memcpy(outBuffer,&tmp,8);
    }
    return 2;
  case AttributeHeader::ROW_SIZE:
    * outBuffer = tabptr.p->tupheadsize << 2;
    return 1;
  case AttributeHeader::ROW_COUNT:
  case AttributeHeader::COMMIT_COUNT:
    signal->theData[0] = operPtr.p->userpointer;
    signal->theData[1] = attrId;
    
    EXECUTE_DIRECT(DBLQH, GSN_READ_PSUEDO_REQ, signal, 2);
    outBuffer[0] = signal->theData[0];
    outBuffer[1] = signal->theData[1];
    return 2;
  case AttributeHeader::RANGE_NO:
    signal->theData[0] = operPtr.p->userpointer;
    signal->theData[1] = attrId;
    
    EXECUTE_DIRECT(DBLQH, GSN_READ_PSUEDO_REQ, signal, 2);
    outBuffer[0] = signal->theData[0];
    return 1;
  default:
    return 0;
  }
}

bool
Dbtup::readBitsNotNULL(Uint32* outBuffer,
		       AttributeHeader* ahOut,
		       Uint32  attrDescriptor,
		       Uint32  attrDes2)
{
  Tablerec* const regTabPtr = tabptr.p;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = AttributeDescriptor::getArraySize(attrDescriptor);
  Uint32 indexBuf = tOutBufIndex;
  Uint32 newIndexBuf = indexBuf + ((bitCount + 31) >> 5);
  Uint32 maxRead = tMaxRead;
  
  if (newIndexBuf <= maxRead) {
    ljam();
    ahOut->setDataSize((bitCount + 31) >> 5);
    tOutBufIndex = newIndexBuf;
    
    BitmaskImpl::getField(regTabPtr->tupNullWords,
			  tTupleHeader+regTabPtr->tupNullIndex,
			  pos, 
			  bitCount,
			  outBuffer+indexBuf);
    
    return true;
  } else {
    ljam();
    terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }//if
}

bool
Dbtup::readBitsNULLable(Uint32* outBuffer,
			AttributeHeader* ahOut,
			Uint32  attrDescriptor,
			Uint32  attrDes2)
{
  Tablerec* const regTabPtr = tabptr.p;
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = AttributeDescriptor::getArraySize(attrDescriptor);
  
  Uint32 indexBuf = tOutBufIndex;
  Uint32 newIndexBuf = indexBuf + ((bitCount + 31) >> 5);
  Uint32 maxRead = tMaxRead;
  
  if(BitmaskImpl::get(regTabPtr->tupNullWords,
		      tTupleHeader+regTabPtr->tupNullIndex,
		      pos))
  {
    ljam();
    ahOut->setNULL();
    return true;
  }


  if (newIndexBuf <= maxRead) {
    ljam();
    ahOut->setDataSize((bitCount + 31) >> 5);
    tOutBufIndex = newIndexBuf;
    BitmaskImpl::getField(regTabPtr->tupNullWords,
			  tTupleHeader+regTabPtr->tupNullIndex,
			  pos+1, 
			  bitCount,
			  outBuffer+indexBuf);
    return true;
  } else {
    ljam();
    terrorCode = ZTRY_TO_READ_TOO_MUCH_ERROR;
    return false;
  }//if
}

bool
Dbtup::updateBitsNotNULL(Uint32* inBuffer,
			 Uint32  attrDescriptor,
			 Uint32  attrDes2)
{
  Tablerec* const regTabPtr =  tabptr.p;
  Uint32 indexBuf = tInBufIndex;
  Uint32 inBufLen = tInBufLen;
  AttributeHeader ahIn(inBuffer[indexBuf]);
  Uint32 nullIndicator = ahIn.isNULL();
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = AttributeDescriptor::getArraySize(attrDescriptor);
  Uint32 newIndex = indexBuf + 1 + ((bitCount + 31) >> 5);
  
  if (newIndex <= inBufLen) {
    if (!nullIndicator) {
      BitmaskImpl::setField(regTabPtr->tupNullWords,
			    tTupleHeader+regTabPtr->tupNullIndex,
			    pos,
			    bitCount,
			    inBuffer+indexBuf+1);
      tInBufIndex = newIndex;
      return true;
    } else {
      ljam();
      terrorCode = ZNOT_NULL_ATTR;
      return false;
    }//if
  } else {
    ljam();
    terrorCode = ZAI_INCONSISTENCY_ERROR;
    return false;
  }//if
  return true;
}

bool
Dbtup::updateBitsNULLable(Uint32* inBuffer,
			  Uint32  attrDescriptor,
			  Uint32  attrDes2)
{
  Tablerec* const regTabPtr =  tabptr.p;
  AttributeHeader ahIn(inBuffer[tInBufIndex]);
  Uint32 indexBuf = tInBufIndex;
  Uint32 nullIndicator = ahIn.isNULL();
  Uint32 pos = AttributeOffset::getNullFlagPos(attrDes2);
  Uint32 bitCount = AttributeDescriptor::getArraySize(attrDescriptor);
  
  if (!nullIndicator) {
    BitmaskImpl::clear(regTabPtr->tupNullWords,
		       tTupleHeader+regTabPtr->tupNullIndex,
		       pos);
    BitmaskImpl::setField(regTabPtr->tupNullWords,
			  tTupleHeader+regTabPtr->tupNullIndex,
			  pos+1,
			  bitCount,
			  inBuffer+indexBuf+1);
    
    Uint32 newIndex = indexBuf + 1 + ((bitCount + 31) >> 5);
    tInBufIndex = newIndex;
    return true;
  } else {
    Uint32 newIndex = tInBufIndex + 1;
    if (newIndex <= tInBufLen) {
      ljam();
      BitmaskImpl::set(regTabPtr->tupNullWords,
		       tTupleHeader+regTabPtr->tupNullIndex,
		       pos);
      
      tInBufIndex = newIndex;
      return true;
    } else {
      ljam();
      terrorCode = ZAI_INCONSISTENCY_ERROR;
      return false;
    }//if
  }//if
}
