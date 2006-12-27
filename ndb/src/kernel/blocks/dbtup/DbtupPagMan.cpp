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

#define ljam() { jamLine(16000 + __LINE__); }
#define ljamEntry() { jamEntryLine(16000 + __LINE__); }

/* ---------------------------------------------------------------- */
// 4) Page Memory Manager (buddy algorithm)
//
// The following data structures in Dbtup is used by the Page Memory
// Manager.
//
// cfreepageList[16]
// Pages with a header
//
// The cfreepageList is 16 free lists. Free list 0 contains chunks of
// pages with 2^0 (=1) pages in each chunk. Free list 1 chunks of 2^1
// (=2) pages in each chunk and so forth upto free list 15 which
// contains chunks of 2^15 (=32768) pages in each chunk.
// The cfreepageList array contains the pointer to the first chunk
// in each of those lists. The lists are doubly linked where the
// first page in each chunk contains the next and previous references
// in position ZPAGE_NEXT_CLUST_POS and ZPAGE_PREV_CLUST_POS in the
// page header.
// In addition the leading page and the last page in each chunk is marked
// with a state (=ZFREE_COMMON) in position ZPAGE_STATE_POS in page
// header. This state indicates that the page is the leading or last page
// in a chunk of free pages. Furthermore the leading and last page is
// also marked with a reference to the leading (=ZPAGE_FIRST_CLUST_POS)
// and the last page (=ZPAGE_LAST_CLUST_POS) in the chunk.
//
// The aim of these data structures is to enable a free area handling of
// free pages based on a buddy algorithm. When allocating pages it is
// performed in chunks of pages and the algorithm tries to make the
// chunks as large as possible.
// This manager is invoked when fragments lack internal page space to
// accomodate all the data they are requested to store. It is also
// invoked when fragments deallocate page space back to the free area.
//
// The following routines are part of the external interface:
// void
// allocConsPages(Uint32  noOfPagesToAllocate, #In
//                Uint32& noOfPagesAllocated,  #Out
//                Uint32& retPageRef)          #Out
// void
// returnCommonArea(Uint32 retPageRef,         #In
//                  Uint32 retNoPages)         #In
//
// allocConsPages tries to allocate noOfPagesToAllocate pages in one chunk.
// If this fails it delivers a chunk as large as possible. It returns the
// i-value of the first page in the chunk delivered, if zero pages returned
// this i-value is undefined. It also returns the size of the chunk actually
// delivered.
// 
// returnCommonArea is used when somebody is returning pages to the free area.
// It is used both from internal routines and external routines.
//
// The following routines are private routines used to support the
// above external interface:
// removeCommonArea()
// insertCommonArea()
// findFreeLeftNeighbours()
// findFreeRightNeighbours()
// Uint32
// nextHigherTwoLog(Uint32 input)
//
// nextHigherTwoLog is a support routine which is a mathematical function with
// an integer as input and an integer as output. It calculates the 2-log of
// (input + 1). If the 2-log of (input + 1) is larger than 15 then the routine
// will return 15. It is part of the external interface since it is also used
// by other similar memory management algorithms.
//
// External dependencies:
// None.
//
// Side Effects:
// Apart from the above mentioned data structures there are no more
// side effects other than through the subroutine parameters in the
// external interface.
//
/* ---------------------------------------------------------------- */

/* ---------------------------------------------------------------- */
/* CALCULATE THE 2-LOG + 1 OF TMP AND PUT RESULT INTO TBITS         */
/* ---------------------------------------------------------------- */
Uint32 Dbtup::nextHigherTwoLog(Uint32 input) 
{
  input = input | (input >> 8);
  input = input | (input >> 4);
  input = input | (input >> 2);
  input = input | (input >> 1);
  Uint32 output = (input & 0x5555) + ((input >> 1) & 0x5555);
  output = (output & 0x3333) + ((output >> 2) & 0x3333);
  output = output + (output >> 4);
  output = (output & 0xf) + ((output >> 8) & 0xf);
  return output;
}//nextHigherTwoLog()

void Dbtup::initializePage() 
{
  for (Uint32 i = 0; i < 16; i++) {
    cfreepageList[i] = RNIL;
  }//for
  PagePtr pagePtr;
  for (pagePtr.i = 0; pagePtr.i < cnoOfPage; pagePtr.i++) {
    ljam();
    refresh_watch_dog();
    ptrAss(pagePtr, page);
    pagePtr.p->pageWord[ZPAGE_PHYSICAL_INDEX] = pagePtr.i;
    pagePtr.p->pageWord[ZPAGE_NEXT_POS] = pagePtr.i + 1;
    pagePtr.p->pageWord[ZPAGE_NEXT_CLUST_POS] = RNIL;
    pagePtr.p->pageWord[ZPAGE_LAST_CLUST_POS] = RNIL;
    pagePtr.p->pageWord[ZPAGE_PREV_POS] = RNIL;
    pagePtr.p->pageWord[ZPAGE_STATE_POS] = ZFREE_COMMON;
  }//for
  pagePtr.i = cnoOfPage - 1;
  ptrAss(pagePtr, page);
  pagePtr.p->pageWord[ZPAGE_NEXT_POS] = RNIL;
  
  pagePtr.i = 0;
  ptrAss(pagePtr, page);
  pagePtr.p->pageWord[ZPAGE_STATE_POS] = ~ZFREE_COMMON;

  for(size_t j = 0; j<MAX_PARALLELL_TUP_SRREQ; j++){
    pagePtr.i = 1+j;
    ptrAss(pagePtr, page);
    pagePtr.p->pageWord[ZPAGE_STATE_POS] = ~ZFREE_COMMON;
  }
  
  Uint32 tmp = 1 + MAX_PARALLELL_TUP_SRREQ;
  returnCommonArea(tmp, cnoOfPage - tmp);
  cnoOfAllocatedPages = tmp; // Is updated by returnCommonArea
  c_sr_free_page_0 = ~0;
}//Dbtup::initializePage()

void Dbtup::allocConsPages(Uint32 noOfPagesToAllocate,
                           Uint32& noOfPagesAllocated,
                           Uint32& allocPageRef)
{
  if (noOfPagesToAllocate == 0){ 
    ljam();
    noOfPagesAllocated = 0;
    return;
  }//if

  Uint32 firstListToCheck = nextHigherTwoLog(noOfPagesToAllocate - 1);
  for (Uint32 i = firstListToCheck; i < 16; i++) {
    ljam();
    if (cfreepageList[i] != RNIL) {
      ljam();
/* ---------------------------------------------------------------- */
/*       PROPER AMOUNT OF PAGES WERE FOUND. NOW SPLIT THE FOUND     */
/*       AREA AND RETURN THE PART NOT NEEDED.                       */
/* ---------------------------------------------------------------- */
      noOfPagesAllocated = noOfPagesToAllocate;
      allocPageRef = cfreepageList[i];
      removeCommonArea(allocPageRef, i);
      Uint32 retNo = (1 << i) - noOfPagesToAllocate;
      Uint32 retPageRef = allocPageRef + noOfPagesToAllocate;
      returnCommonArea(retPageRef, retNo);
      return;
    }//if
  }//for
/* ---------------------------------------------------------------- */
/*       PROPER AMOUNT OF PAGES WERE NOT FOUND. FIND AS MUCH AS     */
/*       POSSIBLE.                                                  */
/* ---------------------------------------------------------------- */
  if (firstListToCheck)
  {
    ljam();
    for (Uint32 j = firstListToCheck - 1; (Uint32)~j; j--) {
      ljam();
      if (cfreepageList[j] != RNIL) {
	ljam();
/* ---------------------------------------------------------------- */
/*       SOME AREA WAS FOUND, ALLOCATE ALL OF IT.                   */
/* ---------------------------------------------------------------- */
	allocPageRef = cfreepageList[j];
	removeCommonArea(allocPageRef, j);
	noOfPagesAllocated = 1 << j;
	findFreeLeftNeighbours(allocPageRef, noOfPagesAllocated, 
			       noOfPagesToAllocate);
	findFreeRightNeighbours(allocPageRef, noOfPagesAllocated, 
				noOfPagesToAllocate);
	
	return;
      }//if
    }//for
  }
/* ---------------------------------------------------------------- */
/*       NO FREE AREA AT ALL EXISTED. RETURN ZERO PAGES             */
/* ---------------------------------------------------------------- */
  noOfPagesAllocated = 0;
  return;
}//allocConsPages()

void Dbtup::returnCommonArea(Uint32 retPageRef, Uint32 retNo) 
{
  do {
    ljam();
    if (retNo == 0) {
      ljam();
      return;
    }//if
    Uint32 list = nextHigherTwoLog(retNo) - 1;
    retNo -= (1 << list);
    insertCommonArea(retPageRef, list);
    retPageRef += (1 << list);
  } while (1);
}//Dbtup::returnCommonArea()

void Dbtup::findFreeLeftNeighbours(Uint32& allocPageRef,
                                   Uint32& noPagesAllocated,
                                   Uint32  noOfPagesToAllocate)
{
  PagePtr pageFirstPtr, pageLastPtr;
  Uint32 remainAllocate = noOfPagesToAllocate - noPagesAllocated;
  while (allocPageRef > 0) {
    ljam();
    pageLastPtr.i = allocPageRef - 1;
    ptrCheckGuard(pageLastPtr, cnoOfPage, page);
    if (pageLastPtr.p->pageWord[ZPAGE_STATE_POS] != ZFREE_COMMON) {
      ljam();
      return;
    } else {
      ljam();
      pageFirstPtr.i = pageLastPtr.p->pageWord[ZPAGE_FIRST_CLUST_POS];
      ndbrequire(pageFirstPtr.i != RNIL);
      Uint32 list = nextHigherTwoLog(pageLastPtr.i - pageFirstPtr.i);
      removeCommonArea(pageFirstPtr.i, list);
      Uint32 listSize = 1 << list;
      if (listSize > remainAllocate) {
        ljam();
        Uint32 retNo = listSize - remainAllocate;
        returnCommonArea(pageFirstPtr.i, retNo);
        allocPageRef = pageFirstPtr.i + retNo;
        noPagesAllocated = noOfPagesToAllocate;
        return;
      } else {
        ljam();
        allocPageRef = pageFirstPtr.i;
        noPagesAllocated += listSize;
        remainAllocate -= listSize;
      }//if
    }//if
  }//while
}//Dbtup::findFreeLeftNeighbours()

void Dbtup::findFreeRightNeighbours(Uint32& allocPageRef,
                                    Uint32& noPagesAllocated,
                                    Uint32  noOfPagesToAllocate)
{
  PagePtr pageFirstPtr, pageLastPtr;
  Uint32 remainAllocate = noOfPagesToAllocate - noPagesAllocated;
  if (remainAllocate == 0) {
    ljam();
    return;
  }//if
  while ((allocPageRef + noPagesAllocated) < cnoOfPage) {
    ljam();
    pageFirstPtr.i = allocPageRef + noPagesAllocated;
    ptrCheckGuard(pageFirstPtr, cnoOfPage, page);
    if (pageFirstPtr.p->pageWord[ZPAGE_STATE_POS] != ZFREE_COMMON) {
      ljam();
      return;
    } else {
      ljam();
      pageLastPtr.i = pageFirstPtr.p->pageWord[ZPAGE_LAST_CLUST_POS];
      ndbrequire(pageLastPtr.i != RNIL);
      Uint32 list = nextHigherTwoLog(pageLastPtr.i - pageFirstPtr.i);
      removeCommonArea(pageFirstPtr.i, list);
      Uint32 listSize = 1 << list;
      if (listSize > remainAllocate) {
        ljam();
        Uint32 retPageRef = pageFirstPtr.i + remainAllocate;
        Uint32 retNo = listSize - remainAllocate;
        returnCommonArea(retPageRef, retNo);
        noPagesAllocated += remainAllocate;
        return;
      } else {
        ljam();
        noPagesAllocated += listSize;
        remainAllocate -= listSize;
      }//if
    }//if
  }//while
}//Dbtup::findFreeRightNeighbours()

void Dbtup::insertCommonArea(Uint32 insPageRef, Uint32 insList) 
{
  cnoOfAllocatedPages -= (1 << insList);
  PagePtr pageLastPtr, pageInsPtr;

  pageInsPtr.i = insPageRef;
  ptrCheckGuard(pageInsPtr, cnoOfPage, page);
  ndbrequire(insList < 16);
  pageLastPtr.i = (pageInsPtr.i + (1 << insList)) - 1;

  pageInsPtr.p->pageWord[ZPAGE_NEXT_CLUST_POS] = cfreepageList[insList];
  pageInsPtr.p->pageWord[ZPAGE_PREV_CLUST_POS] = RNIL;
  pageInsPtr.p->pageWord[ZPAGE_LAST_CLUST_POS] = pageLastPtr.i;
  cfreepageList[insList] = pageInsPtr.i;

  ptrCheckGuard(pageLastPtr, cnoOfPage, page);
  pageLastPtr.p->pageWord[ZPAGE_FIRST_CLUST_POS] = pageInsPtr.i;
  pageLastPtr.p->pageWord[ZPAGE_NEXT_POS] = RNIL;
}//Dbtup::insertCommonArea()

void Dbtup::removeCommonArea(Uint32 remPageRef, Uint32 list) 
{
  cnoOfAllocatedPages += (1 << list);  
  PagePtr pagePrevPtr, pageNextPtr, pageLastPtr, pageSearchPtr, remPagePtr;

  remPagePtr.i = remPageRef;
  ptrCheckGuard(remPagePtr, cnoOfPage, page);
  ndbrequire(list < 16);
  if (cfreepageList[list] == remPagePtr.i) {
    ljam();
    cfreepageList[list] = remPagePtr.p->pageWord[ZPAGE_NEXT_CLUST_POS];
    pageNextPtr.i = cfreepageList[list];
    if (pageNextPtr.i != RNIL) {
      ljam();
      ptrCheckGuard(pageNextPtr, cnoOfPage, page);
      pageNextPtr.p->pageWord[ZPAGE_PREV_CLUST_POS] = RNIL;
    }//if
  } else {
    pageSearchPtr.i = cfreepageList[list];
    while (true) {
      ljam();
      ptrCheckGuard(pageSearchPtr, cnoOfPage, page);
      pagePrevPtr = pageSearchPtr;
      pageSearchPtr.i = pageSearchPtr.p->pageWord[ZPAGE_NEXT_CLUST_POS];
      if (pageSearchPtr.i == remPagePtr.i) {
        ljam();
        break;
      }//if
    }//while
    pageNextPtr.i = remPagePtr.p->pageWord[ZPAGE_NEXT_CLUST_POS];
    pagePrevPtr.p->pageWord[ZPAGE_NEXT_CLUST_POS] = pageNextPtr.i;
    if (pageNextPtr.i != RNIL) {
      ljam();
      ptrCheckGuard(pageNextPtr, cnoOfPage, page);
      pageNextPtr.p->pageWord[ZPAGE_PREV_CLUST_POS] = pagePrevPtr.i;
    }//if
  }//if
  remPagePtr.p->pageWord[ZPAGE_NEXT_CLUST_POS] = RNIL;
  remPagePtr.p->pageWord[ZPAGE_LAST_CLUST_POS] = RNIL;
  remPagePtr.p->pageWord[ZPAGE_PREV_CLUST_POS] = RNIL;

  pageLastPtr.i = (remPagePtr.i + (1 << list)) - 1;
  ptrCheckGuard(pageLastPtr, cnoOfPage, page);
  pageLastPtr.p->pageWord[ZPAGE_FIRST_CLUST_POS] = RNIL;
}//Dbtup::removeCommonArea()


