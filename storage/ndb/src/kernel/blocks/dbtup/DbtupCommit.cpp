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
#include <signaldata/TupCommit.hpp>
#include "../dblqh/Dblqh.hpp"

#define ljam() { jamLine(5000 + __LINE__); }
#define ljamEntry() { jamEntryLine(5000 + __LINE__); }

void Dbtup::execTUP_DEALLOCREQ(Signal* signal)
{
  TablerecPtr regTabPtr;
  FragrecordPtr regFragPtr;
  Uint32 frag_page_id, frag_id;

  ljamEntry();

  frag_id= signal->theData[0];
  regTabPtr.i= signal->theData[1];
  frag_page_id= signal->theData[2];
  Uint32 page_index= signal->theData[3];

  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);
  
  getFragmentrec(regFragPtr, frag_id, regTabPtr.p);
  ndbassert(regFragPtr.p != NULL);
  
  if (! (((frag_page_id << MAX_TUPLES_BITS) + page_index) == ~ (Uint32) 0))
  {
    Local_key tmp;
    tmp.m_page_no= getRealpid(regFragPtr.p, frag_page_id); 
    tmp.m_page_idx= page_index;
    
    PagePtr pagePtr;
    Tuple_header* ptr= (Tuple_header*)get_ptr(&pagePtr, &tmp, regTabPtr.p);

    ndbassert(ptr->m_header_bits & Tuple_header::FREE);

    if (ptr->m_header_bits & Tuple_header::LCP_KEEP)
    {
      ndbassert(! (ptr->m_header_bits & Tuple_header::FREED));
      ptr->m_header_bits |= Tuple_header::FREED;
      return;
    }
    
    if (regTabPtr.p->m_attributes[MM].m_no_of_varsize)
    {
      ljam();
      free_var_rec(regFragPtr.p, regTabPtr.p, &tmp, pagePtr);
    } else {
      free_fix_rec(regFragPtr.p, regTabPtr.p, &tmp, (Fix_page*)pagePtr.p);
    }
  }
}

void Dbtup::execTUP_WRITELOG_REQ(Signal* signal)
{
  jamEntry();
  OperationrecPtr loopOpPtr;
  loopOpPtr.i= signal->theData[0];
  Uint32 gci= signal->theData[1];
  c_operation_pool.getPtr(loopOpPtr);
  while (loopOpPtr.p->prevActiveOp != RNIL) {
    ljam();
    loopOpPtr.i= loopOpPtr.p->prevActiveOp;
    c_operation_pool.getPtr(loopOpPtr);
  }
  do {
    ndbrequire(get_trans_state(loopOpPtr.p) == TRANS_STARTED);
    signal->theData[0]= loopOpPtr.p->userpointer;
    signal->theData[1]= gci;
    if (loopOpPtr.p->nextActiveOp == RNIL) {
      ljam();
      EXECUTE_DIRECT(DBLQH, GSN_LQH_WRITELOG_REQ, signal, 2);
      return;
    }
    ljam();
    EXECUTE_DIRECT(DBLQH, GSN_LQH_WRITELOG_REQ, signal, 2);
    jamEntry();
    loopOpPtr.i= loopOpPtr.p->nextActiveOp;
    c_operation_pool.getPtr(loopOpPtr);
  } while (true);
}

void Dbtup::removeActiveOpList(Operationrec*  const regOperPtr,
                               Tuple_header *tuple_ptr)
{
  OperationrecPtr raoOperPtr;

  /**
   * Release copy tuple
   */
  if(!regOperPtr->m_copy_tuple_location.isNull())
    c_undo_buffer.free_copy_tuple(&regOperPtr->m_copy_tuple_location);
  
  if (regOperPtr->op_struct.in_active_list) {
    regOperPtr->op_struct.in_active_list= false;
    if (regOperPtr->nextActiveOp != RNIL) {
      ljam();
      raoOperPtr.i= regOperPtr->nextActiveOp;
      c_operation_pool.getPtr(raoOperPtr);
      raoOperPtr.p->prevActiveOp= regOperPtr->prevActiveOp;
    } else {
      ljam();
      tuple_ptr->m_operation_ptr_i = regOperPtr->prevActiveOp;
    }
    if (regOperPtr->prevActiveOp != RNIL) {
      ljam();
      raoOperPtr.i= regOperPtr->prevActiveOp;
      c_operation_pool.getPtr(raoOperPtr);
      raoOperPtr.p->nextActiveOp= regOperPtr->nextActiveOp;
    }
    regOperPtr->prevActiveOp= RNIL;
    regOperPtr->nextActiveOp= RNIL;
  }
}

/* ---------------------------------------------------------------- */
/* INITIALIZATION OF ONE CONNECTION RECORD TO PREPARE FOR NEXT OP.  */
/* ---------------------------------------------------------------- */
void Dbtup::initOpConnection(Operationrec* regOperPtr)
{
  set_tuple_state(regOperPtr, TUPLE_ALREADY_ABORTED);
  set_trans_state(regOperPtr, TRANS_IDLE);
  regOperPtr->currentAttrinbufLen= 0;
  regOperPtr->op_struct.op_type= ZREAD;
  regOperPtr->op_struct.m_disk_preallocated= 0;
  regOperPtr->op_struct.m_load_diskpage_on_commit= 0;
  regOperPtr->op_struct.m_wait_log_buffer= 0;
  regOperPtr->m_undo_buffer_space= 0;
}

static
inline
bool
operator>(const Local_key& key1, const Local_key& key2)
{
  return key1.m_page_no > key2.m_page_no ||
    (key1.m_page_no == key2.m_page_no && key1.m_page_idx > key2.m_page_idx);
}

void
Dbtup::dealloc_tuple(Signal* signal,
		     Uint32 gci,
		     Page* page,
		     Tuple_header* ptr, 
		     Operationrec* regOperPtr, 
		     Fragrecord* regFragPtr, 
		     Tablerec* regTabPtr)
{
  Uint32 lcpScan_ptr_i= regFragPtr->m_lcp_scan_op;
  Uint32 lcp_keep_list = regFragPtr->m_lcp_keep_list;

  Uint32 bits = ptr->m_header_bits;
  Uint32 extra_bits = Tuple_header::FREED;
  if (bits & Tuple_header::DISK_PART)
  {
    Local_key disk;
    memcpy(&disk, ptr->get_disk_ref_ptr(regTabPtr), sizeof(disk));
    PagePtr tmpptr;
    tmpptr.i = m_pgman.m_ptr.i;
    tmpptr.p = reinterpret_cast<Page*>(m_pgman.m_ptr.p);
    disk_page_free(signal, regTabPtr, regFragPtr, 
		   &disk, tmpptr, gci);
  }
  
  if (! (bits & Tuple_header::LCP_SKIP) && lcpScan_ptr_i != RNIL)
  {
    ScanOpPtr scanOp;
    c_scanOpPool.getPtr(scanOp, lcpScan_ptr_i);
    Local_key rowid = regOperPtr->m_tuple_location;
    Local_key scanpos = scanOp.p->m_scanPos.m_key;
    rowid.m_page_no = page->frag_page_id;
    if (rowid > scanpos)
    {
      extra_bits = Tuple_header::LCP_KEEP; // Note REMOVE FREE
      ptr->m_operation_ptr_i = lcp_keep_list;
      regFragPtr->m_lcp_keep_list = rowid.ref();
    }
  }
  
  ptr->m_header_bits = bits | extra_bits;
  
  if (regTabPtr->m_bits & Tablerec::TR_RowGCI)
  {
    jam();
    * ptr->get_mm_gci(regTabPtr) = gci;
  }
}

void
Dbtup::commit_operation(Signal* signal,
			Uint32 gci,
			Tuple_header* tuple_ptr, 
			PagePtr pagePtr,
			Operationrec* regOperPtr, 
			Fragrecord* regFragPtr, 
			Tablerec* regTabPtr)
{
  ndbassert(regOperPtr->op_struct.op_type != ZDELETE);
  
  Uint32 lcpScan_ptr_i= regFragPtr->m_lcp_scan_op;
  Uint32 save= tuple_ptr->m_operation_ptr_i;
  Uint32 bits= tuple_ptr->m_header_bits;

  Tuple_header *disk_ptr= 0;
  Tuple_header *copy= (Tuple_header*)
    c_undo_buffer.get_ptr(&regOperPtr->m_copy_tuple_location);
  
  Uint32 copy_bits= copy->m_header_bits;

  Uint32 fixsize= regTabPtr->m_offsets[MM].m_fix_header_size;
  Uint32 mm_vars= regTabPtr->m_attributes[MM].m_no_of_varsize;
  if(mm_vars == 0)
  {
    memcpy(tuple_ptr, copy, 4*fixsize);
    disk_ptr= (Tuple_header*)(((Uint32*)copy)+fixsize);
  }
  else
  {
    /**
     * Var_part_ref is only stored in *allocated* tuple
     * so memcpy from copy, will over write it...
     * hence subtle copyout/assign...
     */
    Local_key tmp; 
    Var_part_ref *ref= tuple_ptr->get_var_part_ref_ptr(regTabPtr);
    ref->copyout(&tmp);

    memcpy(tuple_ptr, copy, 4*fixsize);
    ref->assign(&tmp);

    PagePtr vpagePtr;
    Uint32 *dst= get_ptr(&vpagePtr, *ref);
    Var_page* vpagePtrP = (Var_page*)vpagePtr.p;
    Uint32 *src= copy->get_end_of_fix_part_ptr(regTabPtr);
    Uint32 sz= ((mm_vars + 1) << 1) + (((Uint16*)src)[mm_vars]);
    ndbassert(4*vpagePtrP->get_entry_len(tmp.m_page_idx) >= sz);
    memcpy(dst, src, sz);

    copy_bits |= Tuple_header::CHAINED_ROW;
    
    if(copy_bits & Tuple_header::MM_SHRINK)
    {
      vpagePtrP->shrink_entry(tmp.m_page_idx, (sz + 3) >> 2);
      update_free_page_list(regFragPtr, vpagePtr);
    } 
    
    disk_ptr = (Tuple_header*)(((Uint32*)copy)+fixsize+((sz + 3) >> 2));
  }
  
  if (regTabPtr->m_no_of_disk_attributes &&
      (copy_bits & Tuple_header::DISK_INLINE))
  {
    Local_key key;
    memcpy(&key, copy->get_disk_ref_ptr(regTabPtr), sizeof(Local_key));
    Uint32 logfile_group_id= regFragPtr->m_logfile_group_id;

    PagePtr diskPagePtr = *(PagePtr*)&m_pgman.m_ptr;
    ndbassert(diskPagePtr.p->m_page_no == key.m_page_no);
    ndbassert(diskPagePtr.p->m_file_no == key.m_file_no);
    Uint32 sz, *dst;
    if(copy_bits & Tuple_header::DISK_ALLOC)
    {
      disk_page_alloc(signal, regTabPtr, regFragPtr, &key, diskPagePtr, gci);
    }
    
    if(regTabPtr->m_attributes[DD].m_no_of_varsize == 0)
    {
      sz= regTabPtr->m_offsets[DD].m_fix_header_size;
      dst= ((Fix_page*)diskPagePtr.p)->get_ptr(key.m_page_idx, sz);
    }
    else
    {
      dst= ((Var_page*)diskPagePtr.p)->get_ptr(key.m_page_idx);
      sz= ((Var_page*)diskPagePtr.p)->get_entry_len(key.m_page_idx);
    }
    
    if(! (copy_bits & Tuple_header::DISK_ALLOC))
    {
      disk_page_undo_update(diskPagePtr.p, 
			    &key, dst, sz, gci, logfile_group_id);
    }
    
    memcpy(dst, disk_ptr, 4*sz);
    memcpy(tuple_ptr->get_disk_ref_ptr(regTabPtr), &key, sizeof(Local_key));
    
    ndbassert(! (disk_ptr->m_header_bits & Tuple_header::FREE));
    copy_bits |= Tuple_header::DISK_PART;
  }
  
  if(lcpScan_ptr_i != RNIL && (bits & Tuple_header::ALLOC))
  {
    ScanOpPtr scanOp;
    c_scanOpPool.getPtr(scanOp, lcpScan_ptr_i);
    Local_key rowid = regOperPtr->m_tuple_location;
    Local_key scanpos = scanOp.p->m_scanPos.m_key;
    rowid.m_page_no = pagePtr.p->frag_page_id;
    if(rowid > scanpos)
    {
       copy_bits |= Tuple_header::LCP_SKIP;
    }
  }
  
  Uint32 clear= 
    Tuple_header::ALLOC | Tuple_header::FREE |
    Tuple_header::DISK_ALLOC | Tuple_header::DISK_INLINE | 
    Tuple_header::MM_SHRINK | Tuple_header::MM_GROWN;
  copy_bits &= ~(Uint32)clear;
  
  tuple_ptr->m_header_bits= copy_bits;
  tuple_ptr->m_operation_ptr_i= save;
  
  if (regTabPtr->m_bits & Tablerec::TR_RowGCI)
  {
    jam();
    * tuple_ptr->get_mm_gci(regTabPtr) = gci;
  }
  
  if (regTabPtr->m_bits & Tablerec::TR_Checksum) {
    jam();
    setChecksum(tuple_ptr, regTabPtr);
  }
}

void
Dbtup::disk_page_commit_callback(Signal* signal, 
				 Uint32 opPtrI, Uint32 page_id)
{
  Uint32 hash_value;
  Uint32 gci;
  OperationrecPtr regOperPtr;

  ljamEntry();
  
  c_operation_pool.getPtr(regOperPtr, opPtrI);
  c_lqh->get_op_info(regOperPtr.p->userpointer, &hash_value, &gci);

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();
  
  tupCommitReq->opPtr= opPtrI;
  tupCommitReq->hashValue= hash_value;
  tupCommitReq->gci= gci;

  regOperPtr.p->op_struct.m_load_diskpage_on_commit= 0;
  regOperPtr.p->m_commit_disk_callback_page= page_id;
  m_global_page_pool.getPtr(m_pgman.m_ptr, page_id);
  
  {
    PagePtr tmp;
    tmp.i = m_pgman.m_ptr.i;
    tmp.p = reinterpret_cast<Page*>(m_pgman.m_ptr.p);
    disk_page_set_dirty(tmp);
  }
  
  execTUP_COMMITREQ(signal);
  if(signal->theData[0] == 0)
    c_lqh->tupcommit_conf_callback(signal, regOperPtr.p->userpointer);
}

void
Dbtup::disk_page_log_buffer_callback(Signal* signal, 
				     Uint32 opPtrI,
				     Uint32 unused)
{
  Uint32 hash_value;
  Uint32 gci;
  OperationrecPtr regOperPtr;

  ljamEntry();
  
  c_operation_pool.getPtr(regOperPtr, opPtrI);
  c_lqh->get_op_info(regOperPtr.p->userpointer, &hash_value, &gci);

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();
  
  tupCommitReq->opPtr= opPtrI;
  tupCommitReq->hashValue= hash_value;
  tupCommitReq->gci= gci;

  Uint32 page= regOperPtr.p->m_commit_disk_callback_page;
  ndbassert(regOperPtr.p->op_struct.m_load_diskpage_on_commit == 0);
  regOperPtr.p->op_struct.m_wait_log_buffer= 0;
  m_global_page_pool.getPtr(m_pgman.m_ptr, page);
  
  execTUP_COMMITREQ(signal);
  ndbassert(signal->theData[0] == 0);
  
  c_lqh->tupcommit_conf_callback(signal, regOperPtr.p->userpointer);
}

void
Dbtup::fix_commit_order(OperationrecPtr opPtr)
{
  ndbassert(!opPtr.p->is_first_operation());
  OperationrecPtr firstPtr = opPtr;
  while(firstPtr.p->prevActiveOp != RNIL)
  {
    firstPtr.i = firstPtr.p->prevActiveOp;
    c_operation_pool.getPtr(firstPtr);    
  }

  ndbout_c("fix_commit_order (swapping %d and %d)",
	   opPtr.i, firstPtr.i);
  
  /**
   * Swap data between first and curr
   */
  Uint32 prev= opPtr.p->prevActiveOp;
  Uint32 next= opPtr.p->nextActiveOp;
  Uint32 seco= firstPtr.p->nextActiveOp;

  Operationrec tmp = *opPtr.p;
  * opPtr.p = * firstPtr.p;
  * firstPtr.p = tmp;

  c_operation_pool.getPtr(seco)->prevActiveOp = opPtr.i;
  c_operation_pool.getPtr(prev)->nextActiveOp = firstPtr.i;
  if(next != RNIL)
    c_operation_pool.getPtr(next)->prevActiveOp = firstPtr.i;
}

/* ----------------------------------------------------------------- */
/* --------------- COMMIT THIS PART OF A TRANSACTION --------------- */
/* ----------------------------------------------------------------- */
void Dbtup::execTUP_COMMITREQ(Signal* signal) 
{
  FragrecordPtr regFragPtr;
  OperationrecPtr regOperPtr;
  TablerecPtr regTabPtr;
  KeyReqStruct req_struct;
  TransState trans_state;
  Uint32 no_of_fragrec, no_of_tablerec, hash_value, gci;

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();

  regOperPtr.i= tupCommitReq->opPtr;
  ljamEntry();

  c_operation_pool.getPtr(regOperPtr);
  if(!regOperPtr.p->is_first_operation())
  {
    /**
     * Out of order commit   XXX check effect on triggers
     */
    fix_commit_order(regOperPtr);
  }
  ndbassert(regOperPtr.p->is_first_operation());
  
  regFragPtr.i= regOperPtr.p->fragmentPtr;
  trans_state= get_trans_state(regOperPtr.p);

  no_of_fragrec= cnoOfFragrec;

  ndbrequire(trans_state == TRANS_STARTED);
  ptrCheckGuard(regFragPtr, no_of_fragrec, fragrecord);

  no_of_tablerec= cnoOfTablerec;
  regTabPtr.i= regFragPtr.p->fragTableId;
  hash_value= tupCommitReq->hashValue;
  gci= tupCommitReq->gci;

  req_struct.signal= signal;
  req_struct.hash_value= hash_value;
  req_struct.gci= gci;

  ptrCheckGuard(regTabPtr, no_of_tablerec, tablerec);

  PagePtr page;
  Tuple_header* tuple_ptr= (Tuple_header*)
    get_ptr(&page, &regOperPtr.p->m_tuple_location, regTabPtr.p);
  
  bool get_page = false;
  if(regOperPtr.p->op_struct.m_load_diskpage_on_commit)
  {
    Page_cache_client::Request req;
    ndbassert(regOperPtr.p->is_first_operation() && 
	      regOperPtr.p->is_last_operation());

    /**
     * Check for page
     */
    if(!regOperPtr.p->m_copy_tuple_location.isNull())
    {
      Tuple_header* tmp= (Tuple_header*)
	c_undo_buffer.get_ptr(&regOperPtr.p->m_copy_tuple_location);
      
      memcpy(&req.m_page, 
	     tmp->get_disk_ref_ptr(regTabPtr.p), sizeof(Local_key));

      if (unlikely(regOperPtr.p->op_struct.op_type == ZDELETE &&
		   tmp->m_header_bits & Tuple_header::DISK_ALLOC))
      {
	jam();
	/**
	 * Insert+Delete
	 */
	regOperPtr.p->op_struct.m_load_diskpage_on_commit = 0;
	regOperPtr.p->op_struct.m_wait_log_buffer = 0;	
	disk_page_abort_prealloc(signal, regFragPtr.p, 
				 &req.m_page, req.m_page.m_page_idx);
	
	c_lgman->free_log_space(regFragPtr.p->m_logfile_group_id, 
				regOperPtr.p->m_undo_buffer_space);
	ndbout_c("insert+delete");
	goto skip_disk;
      }
    } 
    else
    {
      // initial delete
      ndbassert(regOperPtr.p->op_struct.op_type == ZDELETE);
      memcpy(&req.m_page, 
	     tuple_ptr->get_disk_ref_ptr(regTabPtr.p), sizeof(Local_key));
      
      ndbassert(tuple_ptr->m_header_bits & Tuple_header::DISK_PART);
    }
    req.m_callback.m_callbackData= regOperPtr.i;
    req.m_callback.m_callbackFunction = 
      safe_cast(&Dbtup::disk_page_commit_callback);

    /*
     * Consider commit to be correlated.  Otherwise pk op + commit makes
     * the page hot.   XXX move to TUP which knows better.
     */
    int flags= regOperPtr.p->op_struct.op_type |
      Page_cache_client::COMMIT_REQ | Page_cache_client::CORR_REQ;
    int res= m_pgman.get_page(signal, req, flags);
    switch(res){
    case 0:
      /**
       * Timeslice
       */
      signal->theData[0] = 1;
      return;
    case -1:
      ndbrequire("NOT YET IMPLEMENTED" == 0);
      break;
    }
    get_page = true;

    {
      PagePtr tmpptr;
      tmpptr.i = m_pgman.m_ptr.i;
      tmpptr.p = reinterpret_cast<Page*>(m_pgman.m_ptr.p);
      disk_page_set_dirty(tmpptr);
    }
    
    regOperPtr.p->m_commit_disk_callback_page= res;
    regOperPtr.p->op_struct.m_load_diskpage_on_commit= 0;
  } 
  
  if(regOperPtr.p->op_struct.m_wait_log_buffer)
  {
    ndbassert(regOperPtr.p->is_first_operation() && 
	      regOperPtr.p->is_last_operation());
    
    Callback cb;
    cb.m_callbackData= regOperPtr.i;
    cb.m_callbackFunction = 
      safe_cast(&Dbtup::disk_page_log_buffer_callback);
    Uint32 sz= regOperPtr.p->m_undo_buffer_space;
    
    Logfile_client lgman(this, c_lgman, regFragPtr.p->m_logfile_group_id);
    int res= lgman.get_log_buffer(signal, sz, &cb);
    switch(res){
    case 0:
      signal->theData[0] = 1;
      return;
    case -1:
      ndbrequire("NOT YET IMPLEMENTED" == 0);
      break;
    }
  }
  
  if(!tuple_ptr)
  {
    tuple_ptr = (Tuple_header*)
      get_ptr(&page, &regOperPtr.p->m_tuple_location,regTabPtr.p);
  }
skip_disk:
  req_struct.m_tuple_ptr = tuple_ptr;
  
  if(get_tuple_state(regOperPtr.p) == TUPLE_PREPARED)
  {
    /**
     * Execute all tux triggers at first commit
     *   since previous tuple is otherwise removed...
     *   btw...is this a "good" solution??
     *   
     *   why can't we instead remove "own version" (when approriate ofcourse)
     */
    if (!regTabPtr.p->tuxCustomTriggers.isEmpty()) {
      ljam();
      OperationrecPtr loopPtr= regOperPtr;
      while(loopPtr.i != RNIL)
      {
	c_operation_pool.getPtr(loopPtr);
	executeTuxCommitTriggers(signal,
				 loopPtr.p,
				 regFragPtr.p,
				 regTabPtr.p);
	set_tuple_state(loopPtr.p, TUPLE_TO_BE_COMMITTED);
	loopPtr.i = loopPtr.p->nextActiveOp;
      }
    }
  }
  
  if(regOperPtr.p->is_last_operation())
  {
    /**
     * Perform "real" commit
     */
    set_change_mask_info(&req_struct, regOperPtr.p);
    checkDetachedTriggers(&req_struct, regOperPtr.p, regTabPtr.p);
    
    if(regOperPtr.p->op_struct.op_type != ZDELETE)
    {
      commit_operation(signal, gci, tuple_ptr, page,
		       regOperPtr.p, regFragPtr.p, regTabPtr.p); 
      removeActiveOpList(regOperPtr.p, tuple_ptr);
    }
    else
    {
      removeActiveOpList(regOperPtr.p, tuple_ptr);
      if (get_page)
	ndbassert(tuple_ptr->m_header_bits & Tuple_header::DISK_PART);
      dealloc_tuple(signal, gci, page.p, tuple_ptr, 
		    regOperPtr.p, regFragPtr.p, regTabPtr.p); 
    }
  } 
  else
  {
    removeActiveOpList(regOperPtr.p, tuple_ptr);   
  }
  
  initOpConnection(regOperPtr.p);
  signal->theData[0] = 0;
}

void
Dbtup::set_change_mask_info(KeyReqStruct * const req_struct,
                            Operationrec * const regOperPtr)
{
  ChangeMaskState state = get_change_mask_state(regOperPtr);
  if (state == USE_SAVED_CHANGE_MASK) {
    ljam();
    req_struct->changeMask.setWord(0, regOperPtr->saved_change_mask[0]);
    req_struct->changeMask.setWord(1, regOperPtr->saved_change_mask[1]);
  } else if (state == RECALCULATE_CHANGE_MASK) {
    ljam();
    // Recompute change mask, for now set all bits
    req_struct->changeMask.set();
  } else if (state == SET_ALL_MASK) {
    ljam();
    req_struct->changeMask.set();
  } else {
    ljam();
    ndbrequire(state == DELETE_CHANGES);
    req_struct->changeMask.set();
  }
}

void
Dbtup::calculateChangeMask(Page* const pagePtr,
                           Tablerec* const regTabPtr,
                           KeyReqStruct * const req_struct)
{
  OperationrecPtr loopOpPtr;
  Uint32 saved_word1= 0;
  Uint32 saved_word2= 0;
  loopOpPtr.i= req_struct->m_tuple_ptr->m_operation_ptr_i;
  do {
    c_operation_pool.getPtr(loopOpPtr);
    ndbrequire(loopOpPtr.p->op_struct.op_type == ZUPDATE);
    ChangeMaskState change_mask= get_change_mask_state(loopOpPtr.p);
    if (change_mask == USE_SAVED_CHANGE_MASK) {
      ljam();
      saved_word1|= loopOpPtr.p->saved_change_mask[0];
      saved_word2|= loopOpPtr.p->saved_change_mask[1];
    } else if (change_mask == RECALCULATE_CHANGE_MASK) {
      ljam();
      //Recompute change mask, for now set all bits
      req_struct->changeMask.set();
      return;
    } else {
      ndbrequire(change_mask == SET_ALL_MASK);
      ljam();
      req_struct->changeMask.set();
      return;
    }
    loopOpPtr.i= loopOpPtr.p->prevActiveOp;
  } while (loopOpPtr.i != RNIL);
  req_struct->changeMask.setWord(0, saved_word1);
  req_struct->changeMask.setWord(1, saved_word2);
}
