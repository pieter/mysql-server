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


#include <ndb_global.h>
#include <NdbOut.hpp>
#include <NdbRecAttr.hpp>
#include <NdbBlob.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbTCP.h>

NdbRecAttr::NdbRecAttr(Ndb*)
{
  theStorageX = 0;
  init();
}

NdbRecAttr::~NdbRecAttr()
{
  release();
}

int
NdbRecAttr::setup(const class NdbDictionary::Column* col, char* aValue)
{
  return setup(&(col->m_impl), aValue);
}
int
NdbRecAttr::setup(const NdbColumnImpl* anAttrInfo, char* aValue)
{
  Uint32 tAttrSize = anAttrInfo->m_attrSize;
  Uint32 tArraySize = anAttrInfo->m_arraySize;
  Uint32 tAttrByteSize = tAttrSize * tArraySize;
  
  m_column = anAttrInfo;

  theAttrId = anAttrInfo->m_attrId;
  theAttrSize = tAttrSize;
  theArraySize = tArraySize;
  theValue = aValue;
  theNULLind = 0;
  m_nullable = anAttrInfo->m_nullable;

  if (theStorageX)
    delete[] theStorageX;

  // check alignment to signal data
  // a future version could check alignment per data type as well
  
  if (aValue != NULL && (UintPtr(aValue)&3) == 0 && (tAttrByteSize&3) == 0) {
    theStorageX = NULL;
    theRef = aValue;
    return 0;
  }
  if (tAttrByteSize <= 32) {
    theStorageX = NULL;
    theStorage[0] = 0;
    theStorage[1] = 0;
    theStorage[2] = 0;
    theStorage[3] = 0;
    theRef = theStorage;
    return 0;
  }
  Uint32 tSize = (tAttrByteSize + 7) >> 3;
  Uint64* tRef = new Uint64[tSize];
  if (tRef != NULL) {
    for (Uint32 i = 0; i < tSize; i++) {
      tRef[i] = 0;
    }
    theStorageX = tRef;
    theRef = tRef;
    return 0;
  }
  return -1;
}

void
NdbRecAttr::copyout()
{
  char* tRef = (char*)theRef;
  char* tValue = theValue;
  if (tRef != tValue && tRef != NULL && tValue != NULL) {
    Uint32 n = theAttrSize * theArraySize;
    while (n-- > 0) {
      *tValue++ = *tRef++;
    }
  }
}

NdbRecAttr *
NdbRecAttr::clone() const {
  NdbRecAttr * ret = new NdbRecAttr(0);

  ret->theAttrId = theAttrId;
  ret->theNULLind = theNULLind;
  ret->theAttrSize = theAttrSize;
  ret->theArraySize = theArraySize;
  ret->m_column = m_column;
  
  Uint32 n = theAttrSize * theArraySize;  
  if(n <= 32){
    ret->theRef = (char*)&ret->theStorage[0];
    ret->theStorageX = 0;
    ret->theValue = 0;
  } else {
    ret->theStorageX = new Uint64[((n + 7) >> 3)];
    ret->theRef = (char*)ret->theStorageX;    
    ret->theValue = 0;
  }
  memcpy(ret->theRef, theRef, n);
  return ret;
}

bool
NdbRecAttr::receive_data(const Uint32 * data, Uint32 sz){
  const Uint32 n = (theAttrSize * theArraySize + 3) >> 2;  
  if(n == sz){
    theNULLind = 0;
    if(!copyoutRequired())
      memcpy(theRef, data, 4 * sz);
    else
      memcpy(theValue, data, theAttrSize * theArraySize);
    return true;
  } else if(sz == 0){
    setNULL();
    return true;
  }
  return false;
}

static void
ndbrecattr_print_string(NdbOut& out, const char *type,
			const char *aref, unsigned sz)
{
  const unsigned char* ref = (const unsigned char*)aref;
  int i, len, printable= 1;
  // trailing zeroes are not printed
  for (i=sz-1; i >= 0; i--)
    if (ref[i] == 0) sz--;
    else break;
  if (sz == 0) return; // empty

  for (len=0; len < (int)sz && ref[i] != 0; len++)
    if (printable && !isprint((int)ref[i]))
      printable= 0;

  if (printable)
    out.print("%.*s", len, ref);
  else
  {
    out.print("0x");
    for (i=0; i < len; i++)
      out.print("%02X", (int)ref[i]);
  }
  if (len != (int)sz)
  {
    out.print("[");
    for (i= len+1; ref[i] != 0; i++)
    out.print("%u]",len-i);
    assert((int)sz > i);
    ndbrecattr_print_string(out,type,aref+i,sz-i);
  }
}

NdbOut& operator<<(NdbOut& out, const NdbRecAttr &r)
{
  if (r.isNULL())
  {
    out << "[NULL]";
    return out;
  }

  const NdbDictionary::Column* c = r.getColumn();
  uint length = c->getLength();
  if (length > 1)
    out << "[";

  for (Uint32 j = 0; j < length; j++) 
  {
    if (j > 0)
      out << " ";

    switch(r.getType())
      {
      case NdbDictionary::Column::Bigunsigned:
	out << r.u_64_value();
	break;
      case NdbDictionary::Column::Bit:
	out << hex << "H'" << r.u_32_value() << dec;
	break;
      case NdbDictionary::Column::Unsigned:
	out << r.u_32_value();
	break;
      case NdbDictionary::Column::Smallunsigned:
	out << r.u_short_value();
	break;
      case NdbDictionary::Column::Tinyunsigned:
	out << (unsigned) r.u_char_value();
	break;
      case NdbDictionary::Column::Bigint:
	out << r.int64_value();
	break;
      case NdbDictionary::Column::Int:
	out << r.int32_value();
	break;
      case NdbDictionary::Column::Smallint:
	out << r.short_value();
	break;
      case NdbDictionary::Column::Tinyint:
	out << (int) r.char_value();
	break;
      case NdbDictionary::Column::Binary:
	ndbrecattr_print_string(out,"Binary",r.aRef(),r.arraySize());
	j = r.arraySize();
	break;
      case NdbDictionary::Column::Char:
	ndbrecattr_print_string(out,"Char",r.aRef(),r.arraySize());
	j = length;
	break;
      case NdbDictionary::Column::Varchar:
        {
          unsigned len = *(const unsigned char*)r.aRef();
          ndbrecattr_print_string(out,"Varchar", r.aRef()+1,len);
          j = length;
        }
	break;
      case NdbDictionary::Column::Varbinary:
        {
          unsigned len = *(const unsigned char*)r.aRef();
          ndbrecattr_print_string(out,"Varbinary", r.aRef()+1,len);
          j = length;
        }
	break;
      case NdbDictionary::Column::Float:
	out << r.float_value();
	break;
      case NdbDictionary::Column::Double:
	out << r.double_value();
	break;
      case NdbDictionary::Column::Olddecimal:
        {
          short len = 1 + c->getPrecision() + (c->getScale() > 0);
          out.print("%.*s", len, r.aRef());
        }
        break;
      case NdbDictionary::Column::Olddecimalunsigned:
        {
          short len = 0 + c->getPrecision() + (c->getScale() > 0);
          out.print("%.*s", len, r.aRef());
        }
	break;
      case NdbDictionary::Column::Decimal:
      case NdbDictionary::Column::Decimalunsigned:
        goto unknown;   // TODO
        break;
      // for dates cut-and-paste from field.cc
      case NdbDictionary::Column::Datetime:
        {
          ulonglong tmp=r.u_64_value();
          long part1,part2,part3;
          part1=(long) (tmp/LL(1000000));
          part2=(long) (tmp - (ulonglong) part1*LL(1000000));
          char buf[40];
          char* pos=(char*) buf+19;
          *pos--=0;
          *pos--= (char) ('0'+(char) (part2%10)); part2/=10; 
          *pos--= (char) ('0'+(char) (part2%10)); part3= (int) (part2 / 10);
          *pos--= ':';
          *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
          *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
          *pos--= ':';
          *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
          *pos--= (char) ('0'+(char) part3);
          *pos--= '/';
          *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
          *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
          *pos--= '-';
          *pos--= (char) ('0'+(char) (part1%10)); part1/=10;
          *pos--= (char) ('0'+(char) (part1%10)); part3= (int) (part1/10);
          *pos--= '-';
          *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
          *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
          *pos--= (char) ('0'+(char) (part3%10)); part3/=10;
          *pos=(char) ('0'+(char) part3);
          out << buf;
        }
	break;
      case NdbDictionary::Column::Date:
        {
          uint32 tmp=(uint32) uint3korr(r.aRef());
          int part;
          char buf[40];
          char *pos=(char*) buf+10;
          *pos--=0;
          part=(int) (tmp & 31);
          *pos--= (char) ('0'+part%10);
          *pos--= (char) ('0'+part/10);
          *pos--= '-';
          part=(int) (tmp >> 5 & 15);
          *pos--= (char) ('0'+part%10);
          *pos--= (char) ('0'+part/10);
          *pos--= '-';
          part=(int) (tmp >> 9);
          *pos--= (char) ('0'+part%10); part/=10;
          *pos--= (char) ('0'+part%10); part/=10;
          *pos--= (char) ('0'+part%10); part/=10;
          *pos=   (char) ('0'+part);
          out << buf;
        }
	break;
      case NdbDictionary::Column::Time:
        {
          long tmp=(long) sint3korr(r.aRef());
          int hour=(uint) (tmp/10000);
          int minute=(uint) (tmp/100 % 100);
          int second=(uint) (tmp % 100);
          char buf[40];
          sprintf(buf, "%02d:%02d:%02d", hour, minute, second);
          out << buf;
        }
	break;
      case NdbDictionary::Column::Year:
        {
          uint year = 1900 + r.u_char_value();
          char buf[40];
          sprintf(buf, "%04d", year);
          out << buf;
        }
	break;
      case NdbDictionary::Column::Timestamp:
        {
          time_t time = r.u_32_value();
          out << (uint)time;
        }
	break;
      case NdbDictionary::Column::Blob:
      case NdbDictionary::Column::Text:
      {
        // user defined aRef() may not be aligned to Uint64
        NdbBlob::Head head;
        memcpy(&head, r.aRef(), sizeof(head));
        out << head.length << ":";
        const unsigned char* p = (const unsigned char*)r.aRef() + sizeof(head);
        if (r.arraySize() < sizeof(head))
          out << "***error***"; // really cannot happen
        else {
          unsigned n = r.arraySize() - sizeof(head);
          for (unsigned k = 0; k < n && k < head.length; k++) {
            if (r.getType() == NdbDictionary::Column::Blob)
              out.print("%02X", (int)p[k]);
            else
              out.print("%c", (int)p[k]);
          }
        }
        j = length;
      }
      break;
      case NdbDictionary::Column::Longvarchar:
        {
          unsigned len = uint2korr(r.aRef());
          ndbrecattr_print_string(out,"Longvarchar", r.aRef()+2,len);
          j = length;
        }
        break;
      unknown:
      default: /* no print functions for the rest, just print type */
	out << (int) r.getType();
	j = length;
	if (j > 1)
	  out << " " << j << " times";
	break;
      }
  }

  if (length > 1)
  {
    out << "]";
  }

  return out;
}

Int64
NdbRecAttr::int64_value() const 
{
  Int64 val;
  memcpy(&val,theRef,8);
  return val;
}

Uint64
NdbRecAttr::u_64_value() const
{
  Uint64 val;
  memcpy(&val,theRef,8);
  return val;
}

float
NdbRecAttr::float_value() const
{
  float val;
  memcpy(&val,theRef,sizeof(val));
  return val;
}

double
NdbRecAttr::double_value() const
{
  double val;
  memcpy(&val,theRef,sizeof(val));
  return val;
}
