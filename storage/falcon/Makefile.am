# Copyright (C) 2006, 2007 MySQL AB
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

SUBDIRS= TransformLib

# Re-enable implicit templates and exceptions
CXXFLAGS= @CXXFLAGS@ -fimplicit-templates @FALCON_CXXFLAGS@

DEFS= @DEFS@ \
      -D_REENTRANT -D_PTHREADS -DENGINE -DSTORAGE_ENGINE -DNAMESPACE=Nfs

INCLUDES= -I$(top_srcdir)/include -I$(top_builddir)/include \
	  -I$(top_srcdir)/regex -I$(top_srcdir)/sql \
	  -I$(srcdir) -I$(srcdir)/TransformLib @ZLIB_INCLUDES@

falcon_headers= Agent.h Alias.h Application.h \
		AsciiBlob.h BDB.h \
		BigInt.h \
		BigInteger.h \
		BinaryBlob.h Bitmap.h Blob.h BlobReference.h Btn.h Cache.h \
		CmdGen.h \
		CollationCaseless.h \
		CollationUnknown.h \
		Collation.h \
		CollationManager.h \
		CompiledStatement.h Configuration.h Connection.h Context.h \
		Coterie.h CoterieRange.h \
		Database.h \
		DatabaseBackup.h \
		DatabaseClone.h \
		DatabaseCopy.h \
		DatabaseMetaData.h DataOutputStream.h DataOverflowPage.h \
		DataPage.h DataResourceLocator.h DateTime.h Dbb.h Debug.h \
		DeferredIndex.h \
		DeferredIndexWalker.h \
		DenseArray.h \
		Digest.h EditString.h EncodedDataStream.h \
		EncodedRecord.h Engine.h Error.h Event.h \
		Field.h FilterDifferences.h Filter.h FilterSet.h \
		FilterSetManager.h FilterTree.h ForeignKey.h Format.h \
		FsbExhaustive.h \
		FsbGroup.h \
		Fsb.h \
		FsbDerivedTable.h \
		FsbInversion.h \
		FsbJoin.h \
		FsbOuterJoin.h \
		FsbSieve.h \
		FsbSort.h \
		FsbUnion.h \
		GenOption.h \
		Gopher.h \
		ha_falcon.h \
		Hdr.h HdrState.h Image.h ImageManager.h \
		Images.h Index.h IndexKey.h \
		IndexNode.h \
		Index2Node.h \
		IndexPage.h \
		Index2Page.h \
		IndexRootPage.h \
		Index2RootPage.h \
		InfoTable.h \
		Interlock.h InversionFilter.h \
		Inversion.h InversionPage.h InversionWord.h IOx.h JString.h \
		LinkedList.h \
		Log.h \
		LogLock.h \
		LogStream.h \
		MemMgr.h \
		MemControl.h \
		MemFreeBlock.h \
		MemoryManager.h \
		MemoryResultSetColumn.h \
		MemoryResultSet.h MemoryResultSetMetaData.h \
		MetaDataResultSet.h Module.h MsgType.h Mutex.h \
		MySQLCollation.h \
		MySqlEnums.h \
		NAlias.h NBitmap.h NBitSet.h NCast.h \
		NConnectionVariable.h NConstant.h NDelete.h netfra_jni.h \
		NetfraVersion.h NExists.h NField.h NInSelectBitmap.h \
		NInSelect.h NInsert.h NMatching.h NNode.h nodes.h NParameter.h \
		NRecordNumber.h NRepair.h NReplace.h NSelectExpr.h NSelect.h \
		NSequence.h NStat.h NUpdate.h OpSys.h Page.h \
		PageInventoryPage.h PagePrecedence.h PageType.h PageWriter.h \
		Parameter.h Parameters.h PreparedStatement.h PrettyPrint.h \
		Privilege.h PrivilegeObject.h PrivType.h Properties.h \
		Protocol.h PStatement.h Queue.h QueryString.h RecordGroup.h \
		Record.h RecordLeaf.h \
		RecordLocatorPage.h \
		RecordSection.h \
		RecordScavenge.h \
		RecordVersion.h \
		RecoveryObjects.h \
		RecoveryPage.h \
		Registry.h Repository.h RepositoryManager.h RepositoryVolume.h \
		Resource.h ResultList.h ResultSet.h ResultSetMetaData.h Role.h \
		RoleModel.h RootPage.h Row.h RSet.h \
		SavePoint.h \
		ScaledBinary.h ScanDir.h \
		Scan.h ScanType.h Scavenger.h Scheduled.h ScheduleElement.h \
		Schedule.h Scheduler.h Schema.h Search.h SearchHit.h \
		SearchWords.h Section.h \
		SectionPage.h \
		SectionRootPage.h Sequence.h SequenceManager.h SequencePage.h \
		SequenceResultSet.h SerialLogAction.h SerialLogControl.h \
		SerialLogFile.h SerialLog.h SerialLogRecord.h \
		SerialLogTransaction.h SerialLogWindow.h \
		Server.h \
		Session.h SessionManager.h SessionQueue.h Socket.h Sort.h \
		SortMerge.h SortRecord.h SortRun.h SortStream.h \
		SparseArray.h \
		SQLError.h SQLException.h SQLParse.h \
		SRLBlobDelete.h \
		SRLBlobUpdate.h \
		SRLCheckpoint.h SRLCommit.h \
		SRLCreateIndex.h \
		SRLCreateSection.h \
		SRLCreateTableSpace.h \
		SRLData.h \
		SRLDataPage.h \
		SRLDelete.h \
		SRLDeleteIndex.h \
		SRLDropTable.h \
		SRLDropTableSpace.h \
		SRLFreePage.h \
		SRLIndexAdd.h \
		SRLIndexDelete.h \
		SRLIndexPage.h \
		SRLIndexUpdate.h \
		SRLInversionPage.h \
		SRLOverflowPages.h \
		SRLPrepare.h \
		SRLRecordLocator.h \
		SRLSectionLine.h \
		SRLSectionPage.h \
		SRLSectionPromotion.h \
		SRLRecordStub.h SRLRollback.h \
		SRLSequence.h \
		SRLSequencePage.h \
		SRLSwitchLog.h \
		SRLUpdateIndex.h \
		SRLUpdateBlob.h \
		SRLUpdateRecords.h \
		SRLVersion.h \
		SRLWordUpdate.h \
		Stack.h Statement.h StorageConnection.h \
		StorageDatabase.h \
		StorageHandler.h \
		StorageParameters.h \
		StorageTable.h \
		StorageTableShare.h \
		Stream.h \
		StreamSegment.h SymbolManager.h Sync.h SynchronizationObject.h \
		Synchronize.h \
		SyncObject.h \
		SyncTest.h \
		SyncWait.h \
		Syntax.h \
		TableAttachment.h \
		TableFilter.h \
		Table.h \
		TableSpace.h \
		TableSpaceManager.h \
		TextFormat.h Thread.h \
		ThreadQueue.h Threads.h \
		TimeStamp.h \
		Transaction.h \
		TransactionManager.h \
		Trigger.h \
		TriggerRecord.h \
		Types.h Unicode.h UnTable.h User.h UserRole.h \
		Validation.h ValueEx.h Value.h ValueSet.h Values.h View.h \
		WString.h
falcon_sources= Agent.cpp Alias.cpp \
		AsciiBlob.cpp \
		BDB.cpp \
		BigInt.cpp \
		BigInteger.cpp \
		BinaryBlob.cpp Bitmap.cpp Blob.cpp \
		BlobReference.cpp Btn.cpp Cache.cpp CmdGen.cpp \
		CollationCaseless.cpp \
		CollationUnknown.cpp \
		Collation.cpp \
		CollationManager.cpp \
		CompiledStatement.cpp Configuration.cpp \
		Connection.cpp Context.cpp Coterie.cpp \
		CoterieRange.cpp \
		Database.cpp \
		DatabaseBackup.cpp \
		DatabaseClone.cpp \
		DatabaseCopy.cpp \
		DatabaseMetaData.cpp \
		DataOutputStream.cpp DataOverflowPage.cpp DataPage.cpp \
		DateTime.cpp Dbb.cpp Debug.cpp \
		DeferredIndex.cpp \
		DeferredIndexWalker.cpp \
		Digest.cpp EditString.cpp EncodedDataStream.cpp \
		EncodedRecord.cpp Error.cpp Event.cpp \
		Field.cpp Filter.cpp FilterDifferences.cpp FilterSet.cpp \
		FilterSetManager.cpp FilterTree.cpp ForeignKey.cpp Format.cpp \
		Fsb.cpp \
		FsbDerivedTable.cpp \
		FsbExhaustive.cpp \
		FsbGroup.cpp \
		FsbInversion.cpp \
		FsbJoin.cpp \
		FsbOuterJoin.cpp \
		FsbSieve.cpp \
		FsbSort.cpp \
		FsbUnion.cpp \
		Gopher.cpp \
		ha_falcon.cpp \
		Hdr.cpp Image.cpp \
		ImageManager.cpp Images.cpp Index.cpp IndexKey.cpp \
		IndexNode.cpp \
		Index2Node.cpp \
		IndexPage.cpp \
		Index2Page.cpp \
		IndexRootPage.cpp \
		Index2RootPage.cpp \
		InfoTable.cpp \
		Inversion.cpp \
		InversionFilter.cpp InversionPage.cpp InversionWord.cpp IO.cpp \
		JString.cpp \
		LinkedList.cpp \
		Log.cpp \
		LogLock.cpp \
		LogStream.cpp \
		MemControl.cpp \
		MemFreeBlock.cpp \
		MemMgr.cpp \
		MemoryResultSetColumn.cpp \
		MemoryResultSet.cpp MemoryResultSetMetaData.cpp \
		MetaDataResultSet.cpp Module.cpp \
		Mutex.cpp \
		MySQLCollation.cpp \
		NAlias.cpp \
		NBitmap.cpp NBitSet.cpp NCast.cpp NConnectionVariable.cpp \
		NConstant.cpp NDelete.cpp NExists.cpp NField.cpp \
		NInSelectBitmap.cpp NInSelect.cpp NInsert.cpp NMatching.cpp \
		NNode.cpp NParameter.cpp NRecordNumber.cpp NRepair.cpp \
		NReplace.cpp NSelect.cpp NSelectExpr.cpp NSequence.cpp \
		NStat.cpp NUpdate.cpp OpSys.cpp Page.cpp \
		PageInventoryPage.cpp PagePrecedence.cpp PageWriter.cpp \
		Parameter.cpp Parameters.cpp PreparedStatement.cpp \
		PrettyPrint.cpp Privilege.cpp PrivilegeObject.cpp Protocol.cpp \
		PStatement.cpp QueryString.cpp Record.cpp \
		RecordGroup.cpp RecordLeaf.cpp \
		RecordLocatorPage.cpp \
		RecordSection.cpp \
		RecordScavenge.cpp \
		RecordVersion.cpp \
		RecoveryObjects.cpp \
		RecoveryPage.cpp \
		Registry.cpp \
		Repository.cpp \
		RepositoryManager.cpp RepositoryVolume.cpp ResultList.cpp \
		ResultSet.cpp ResultSetMetaData.cpp Role.cpp RoleModel.cpp \
		RootPage.cpp Row.cpp RSet.cpp \
		SavePoint.cpp \
		ScaledBinary.cpp Scan.cpp \
		ScanDir.cpp Scavenger.cpp Schedule.cpp Scheduled.cpp \
		ScheduleElement.cpp Scheduler.cpp Schema.cpp Search.cpp \
		SearchHit.cpp SearchWords.cpp Section.cpp \
		SectionPage.cpp SectionRootPage.cpp Sequence.cpp \
		SequenceManager.cpp SequenceResultSet.cpp \
		SerialLogAction.cpp SerialLogControl.cpp SerialLog.cpp \
		SerialLogFile.cpp SerialLogRecord.cpp SerialLogTransaction.cpp \
		SerialLogWindow.cpp \
		Server.cpp \
		Socket.cpp Sort.cpp \
		SortMerge.cpp SortRecord.cpp SortRun.cpp SortStream.cpp \
		SQLError.cpp SQLParse.cpp \
		SRLBlobDelete.cpp \
		SRLBlobUpdate.cpp \
		SRLCheckpoint.cpp \
		SRLCommit.cpp \
		SRLCreateIndex.cpp \
		SRLCreateSection.cpp \
		SRLCreateTableSpace.cpp \
		SRLData.cpp \
		SRLDataPage.cpp \
		SRLDelete.cpp \
		SRLDeleteIndex.cpp \
		SRLDropTable.cpp \
		SRLDropTableSpace.cpp \
		SRLFreePage.cpp \
		SRLIndexAdd.cpp \
		SRLIndexDelete.cpp \
		SRLIndexPage.cpp \
		SRLIndexUpdate.cpp \
		SRLInversionPage.cpp \
		SRLOverflowPages.cpp \
		SRLPrepare.cpp \
		SRLRecordStub.cpp \
		SRLRollback.cpp \
		SRLRecordLocator.cpp \
		SRLSectionLine.cpp \
		SRLSectionPage.cpp \
		SRLSectionPromotion.cpp \
		SRLSequence.cpp \
		SRLSequencePage.cpp \
		SRLSwitchLog.cpp \
		SRLUpdateIndex.cpp \
		SRLUpdateBlob.cpp \
		SRLUpdateRecords.cpp \
		SRLVersion.cpp \
		SRLWordUpdate.cpp \
		Stack.cpp \
		Statement.cpp StorageConnection.cpp \
		StorageDatabase.cpp \
		StorageHandler.cpp \
		StorageTable.cpp StorageTableShare.cpp Stream.cpp \
		StreamSegment.cpp SymbolManager.cpp Sync.cpp Synchronize.cpp \
		SyncObject.cpp\
		SyncTest.cpp\
		SyncWait.cpp\
		Syntax.cpp\
		TableAttachment.cpp \
		Table.cpp \
		TableSpace.cpp \
		TableSpaceManager.cpp \
		TableFilter.cpp \
		Thread.cpp \
		ThreadQueue.cpp \
		Threads.cpp \
		TimeStamp.cpp \
		Transaction.cpp \
		TransactionManager.cpp \
		Trigger.cpp \
		Unicode.cpp UnTable.cpp User.cpp \
		UserRole.cpp Validation.cpp Value.cpp ValueEx.cpp Values.cpp \
		ValueSet.cpp View.cpp WString.cpp

noinst_HEADERS=		$(falcon_headers)

EXTRA_LTLIBRARIES=	ha_falcon.la
pkglib_LTLIBRARIES=	@plugin_falcon_shared_target@
ha_falcon_la_LDFLAGS=	-module -rpath $(pkglibdir) -lstdc++
ha_falcon_la_CXXFLAGS=	$(AM_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_falcon_la_CFLAGS=	$(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
ha_falcon_la_LIBADD=	TransformLib/libtransform.la
ha_falcon_la_SOURCES=	$(falcon_sources)

EXTRA_LIBRARIES=	libfalcon.a libhafalcon.a
noinst_LIBRARIES=	@plugin_falcon_static_target@
libfalcon_a_LIBADD=	libhafalcon.a TransformLib/libtransform.a
libfalcon_a_SOURCES=
libhafalcon_a_CXXFLAGS=	$(AM_CXXFLAGS)
libhafalcon_a_CFLAGS=	$(AM_CFLAGS)
libhafalcon_a_SOURCES=	$(falcon_sources)

libfalcon.a:		$(libfalcon_a_LIBADD)
		-rm -f $@
		if test "$(host_os)" = "netware" ; \
		then \
		  $(libfalcon_a_AR) $@ $(libfalcon_a_LIBADD) ; \
		else \
		  for arc in $(libfalcon_a_LIBADD); do \
		     arpath=`echo $$arc|sed 's|[^/]*$$||'`; \
		     $(AR) t $$arc|sed "s|^|$$arpath|"; \
		  done | sort -u | xargs $(AR) cq $@ ; \
		  $(RANLIB) $@	; \
		fi


EXTRA_DIST=		CMakeLists.txt plug.in

# Don't update the files from bitkeeper
%::SCCS/s.%
