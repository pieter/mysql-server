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



#define DBACC_C
#include "Dbacc.hpp"

#define DEBUG(x) { ndbout << "ACC::" << x << endl; }

void Dbacc::initData() 
{
  cdirarraysize = ZDIRARRAY;
  coprecsize = ZOPRECSIZE;
  cpagesize = ZPAGESIZE;
  clcpConnectsize = ZLCP_CONNECTSIZE;
  ctablesize = ZTABLESIZE;
  cfragmentsize = ZFRAGMENTSIZE;
  crootfragmentsize = ZROOTFRAGMENTSIZE;
  cdirrangesize = ZDIRRANGESIZE;
  coverflowrecsize = ZOVERFLOWRECSIZE;
  cfsConnectsize = ZFS_CONNECTSIZE;
  cfsOpsize = ZFS_OPSIZE;
  cscanRecSize = ZSCAN_REC_SIZE;
  csrVersionRecSize = ZSR_VERSION_REC_SIZE;

  
  dirRange = 0;
  directoryarray = 0;
  fragmentrec = 0;
  fsConnectrec = 0;
  fsOprec = 0;
  lcpConnectrec = 0;
  operationrec = 0;
  overflowRecord = 0;
  page8 = 0;
  rootfragmentrec = 0;
  scanRec = 0;
  srVersionRec = 0;
  tabrec = 0;
  undopage = 0;

  // Records with constant sizes
}//Dbacc::initData()

void Dbacc::initRecords() 
{
  // Records with dynamic sizes
  dirRange = (DirRange*)allocRecord("DirRange",
				    sizeof(DirRange), 
				    cdirrangesize);

  directoryarray = (Directoryarray*)allocRecord("Directoryarray",
						sizeof(Directoryarray), 
						cdirarraysize);

  fragmentrec = (Fragmentrec*)allocRecord("Fragmentrec",
					  sizeof(Fragmentrec), 
					  cfragmentsize);

  fsConnectrec = (FsConnectrec*)allocRecord("FsConnectrec",
					    sizeof(FsConnectrec), 
					    cfsConnectsize);

  fsOprec = (FsOprec*)allocRecord("FsOprec",
				  sizeof(FsOprec), 
				  cfsOpsize);

  lcpConnectrec = (LcpConnectrec*)allocRecord("LcpConnectrec",
					      sizeof(LcpConnectrec),
					      clcpConnectsize);

  operationrec = (Operationrec*)allocRecord("Operationrec",
					    sizeof(Operationrec),
					    coprecsize);

  overflowRecord = (OverflowRecord*)allocRecord("OverflowRecord",
						sizeof(OverflowRecord),
						coverflowrecsize);

  page8 = (Page8*)allocRecord("Page8",
			      sizeof(Page8), 
			      cpagesize,
			      false);

  rootfragmentrec = (Rootfragmentrec*)allocRecord("Rootfragmentrec",
						  sizeof(Rootfragmentrec), 
						  crootfragmentsize);

  scanRec = (ScanRec*)allocRecord("ScanRec",
				  sizeof(ScanRec), 
				  cscanRecSize);

  srVersionRec = (SrVersionRec*)allocRecord("SrVersionRec",
					    sizeof(SrVersionRec), 
					    csrVersionRecSize);

  tabrec = (Tabrec*)allocRecord("Tabrec",
				sizeof(Tabrec),
				ctablesize);

  undopage = (Undopage*)allocRecord("Undopage",
				    sizeof(Undopage), 
				    cundopagesize,
				    false);
  
  // Initialize BAT for interface to file system

  NewVARIABLE* bat = allocateBat(3);
  bat[1].WA = &page8->word32[0];
  bat[1].nrr = cpagesize;
  bat[1].ClusterSize = sizeof(Page8);
  bat[1].bits.q = 11;
  bat[1].bits.v = 5;
  bat[2].WA = &undopage->undoword[0];
  bat[2].nrr = cundopagesize;
  bat[2].ClusterSize = sizeof(Undopage);
  bat[2].bits.q = 13;
  bat[2].bits.v = 5;
}//Dbacc::initRecords()

Dbacc::Dbacc(const class Configuration & conf):
  SimulatedBlock(DBACC, conf),
  c_tup(0)
{
  Uint32 log_page_size= 0;
  BLOCK_CONSTRUCTOR(Dbacc);

  const ndb_mgm_configuration_iterator * p = conf.getOwnConfigIterator();
  ndbrequire(p != 0);

  ndb_mgm_get_int_parameter(p, CFG_DB_UNDO_INDEX_BUFFER,  
			    &log_page_size);

  /**
   * Always set page size in half MBytes
   */
  cundopagesize= (log_page_size / sizeof(Undopage));
  Uint32 mega_byte_part= cundopagesize & 15;
  if (mega_byte_part != 0) {
    jam();
    cundopagesize+= (16 - mega_byte_part);
  }

  // Transit signals
  addRecSignal(GSN_DUMP_STATE_ORD, &Dbacc::execDUMP_STATE_ORD);
  addRecSignal(GSN_DEBUG_SIG, &Dbacc::execDEBUG_SIG);
  addRecSignal(GSN_CONTINUEB, &Dbacc::execCONTINUEB);
  addRecSignal(GSN_ACC_CHECK_SCAN, &Dbacc::execACC_CHECK_SCAN);
  addRecSignal(GSN_EXPANDCHECK2, &Dbacc::execEXPANDCHECK2);
  addRecSignal(GSN_SHRINKCHECK2, &Dbacc::execSHRINKCHECK2);
  addRecSignal(GSN_ACC_OVER_REC, &Dbacc::execACC_OVER_REC);
  addRecSignal(GSN_ACC_SAVE_PAGES, &Dbacc::execACC_SAVE_PAGES);
  addRecSignal(GSN_NEXTOPERATION, &Dbacc::execNEXTOPERATION);
  addRecSignal(GSN_READ_PSUEDO_REQ, &Dbacc::execREAD_PSUEDO_REQ);

  // Received signals
  addRecSignal(GSN_STTOR, &Dbacc::execSTTOR);
  addRecSignal(GSN_SR_FRAGIDREQ, &Dbacc::execSR_FRAGIDREQ);
  addRecSignal(GSN_LCP_FRAGIDREQ, &Dbacc::execLCP_FRAGIDREQ);
  addRecSignal(GSN_LCP_HOLDOPREQ, &Dbacc::execLCP_HOLDOPREQ);
  addRecSignal(GSN_END_LCPREQ, &Dbacc::execEND_LCPREQ);
  addRecSignal(GSN_ACC_LCPREQ, &Dbacc::execACC_LCPREQ);
  addRecSignal(GSN_START_RECREQ, &Dbacc::execSTART_RECREQ);
  addRecSignal(GSN_ACC_CONTOPREQ, &Dbacc::execACC_CONTOPREQ);
  addRecSignal(GSN_ACCKEYREQ, &Dbacc::execACCKEYREQ);
  addRecSignal(GSN_ACCSEIZEREQ, &Dbacc::execACCSEIZEREQ);
  addRecSignal(GSN_ACCFRAGREQ, &Dbacc::execACCFRAGREQ);
  addRecSignal(GSN_ACC_SRREQ, &Dbacc::execACC_SRREQ);
  addRecSignal(GSN_NEXT_SCANREQ, &Dbacc::execNEXT_SCANREQ);
  addRecSignal(GSN_ACC_ABORTREQ, &Dbacc::execACC_ABORTREQ);
  addRecSignal(GSN_ACC_SCANREQ, &Dbacc::execACC_SCANREQ);
  addRecSignal(GSN_ACCMINUPDATE, &Dbacc::execACCMINUPDATE);
  addRecSignal(GSN_ACC_COMMITREQ, &Dbacc::execACC_COMMITREQ);
  addRecSignal(GSN_ACC_TO_REQ, &Dbacc::execACC_TO_REQ);
  addRecSignal(GSN_ACC_LOCKREQ, &Dbacc::execACC_LOCKREQ);
  addRecSignal(GSN_FSOPENCONF, &Dbacc::execFSOPENCONF);
  addRecSignal(GSN_FSOPENREF, &Dbacc::execFSOPENREF);
  addRecSignal(GSN_FSCLOSECONF, &Dbacc::execFSCLOSECONF);
  addRecSignal(GSN_FSCLOSEREF, &Dbacc::execFSCLOSEREF);
  addRecSignal(GSN_FSWRITECONF, &Dbacc::execFSWRITECONF);
  addRecSignal(GSN_FSWRITEREF, &Dbacc::execFSWRITEREF);
  addRecSignal(GSN_FSREADCONF, &Dbacc::execFSREADCONF);
  addRecSignal(GSN_FSREADREF, &Dbacc::execFSREADREF);
  addRecSignal(GSN_NDB_STTOR, &Dbacc::execNDB_STTOR);
  addRecSignal(GSN_DROP_TAB_REQ, &Dbacc::execDROP_TAB_REQ);
  addRecSignal(GSN_FSREMOVECONF, &Dbacc::execFSREMOVECONF);
  addRecSignal(GSN_FSREMOVEREF, &Dbacc::execFSREMOVEREF);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbacc::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_SET_VAR_REQ,  &Dbacc::execSET_VAR_REQ);

  initData();
}//Dbacc::Dbacc()

Dbacc::~Dbacc() 
{
  deallocRecord((void **)&dirRange, "DirRange",
		sizeof(DirRange), 
		cdirrangesize);
  
  deallocRecord((void **)&directoryarray, "Directoryarray",
		sizeof(Directoryarray), 
		cdirarraysize);
  
  deallocRecord((void **)&fragmentrec, "Fragmentrec",
		sizeof(Fragmentrec), 
		cfragmentsize);
  
  deallocRecord((void **)&fsConnectrec, "FsConnectrec",
		sizeof(FsConnectrec), 
		cfsConnectsize);
  
  deallocRecord((void **)&fsOprec, "FsOprec",
		sizeof(FsOprec), 
		cfsOpsize);
  
  deallocRecord((void **)&lcpConnectrec, "LcpConnectrec",
		sizeof(LcpConnectrec),
		clcpConnectsize);
  
  deallocRecord((void **)&operationrec, "Operationrec",
		sizeof(Operationrec),
		coprecsize);
  
  deallocRecord((void **)&overflowRecord, "OverflowRecord",
		sizeof(OverflowRecord),
		coverflowrecsize);

  deallocRecord((void **)&page8, "Page8",
		sizeof(Page8), 
		cpagesize);
  
  deallocRecord((void **)&rootfragmentrec, "Rootfragmentrec",
		sizeof(Rootfragmentrec), 
		crootfragmentsize);
  
  deallocRecord((void **)&scanRec, "ScanRec",
		sizeof(ScanRec), 
		cscanRecSize);
  
  deallocRecord((void **)&srVersionRec, "SrVersionRec",
		sizeof(SrVersionRec), 
		csrVersionRecSize);
  
  deallocRecord((void **)&tabrec, "Tabrec",
		sizeof(Tabrec),
		ctablesize);
  
  deallocRecord((void **)&undopage, "Undopage",
		sizeof(Undopage), 
		cundopagesize);

}//Dbacc::~Dbacc()

BLOCK_FUNCTIONS(Dbacc)
