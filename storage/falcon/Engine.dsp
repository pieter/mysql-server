# Microsoft Developer Studio Project File - Name="Engine" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=Engine - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Engine.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Engine.mak" CFG="Engine - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Engine - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "Engine - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Engine - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /Zi /Ot /Oi /Op /Oy /I ".\Word" /I ".\Crypto" /I "." /I ".\Xml" /I "G:\java\jdk1.2\include" /I "G:\java\jdk1.2\include\win32" /I ".\TransformLib" /I ".\TomCryptLib" /I "\mssdk\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "ENGINE" /D "_WINDLL" /D "_AFXDLL" /YX /FD /Zm200 /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 ws2_32.lib iphlpapi.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"Release/NetfraEngine.dll" /libpath:"\mssdk\lib"

!ELSEIF  "$(CFG)" == "Engine - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "..\..\..\netfrastructure\Templates" /I "..\..\..\netfrastructure\JVM" /I "TransformLib" /I "..\..\..\netfrastructure\TomCryptLib" /I "." /I "..\..\..\netfrastructure\Xml" /I "..\..\..\netfrastructure\Word" /I "..\..\..\netfrastructure\ZLib" /I "..\..\..\netfrastructure\Pdf" /I "G:\java\jdk1.2\include" /I "G:\java\jdk1.2\include\win32" /I "\mssdk\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "NO_DUMMY_DECL" /D "ENGINE" /D "_WINDLL" /D "_AFXDLL" /D "MEM_DEBUG" /YX /FD /Zm200 /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 iphlpapi.lib ws2_32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /out:"..\..\..\netfrastructure\Engine\Debug\NetfraEngine.dll" /pdbtype:sept /libpath:"g:\mssdk\lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "Engine - Win32 Release"
# Name "Engine - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "*.cpp "
# Begin Source File

SOURCE=.\Agent.cpp
# End Source File
# Begin Source File

SOURCE=.\Alias.cpp
# End Source File
# Begin Source File

SOURCE=.\AppEvent.cpp
# End Source File
# Begin Source File

SOURCE=.\Application.cpp
# End Source File
# Begin Source File

SOURCE=.\Applications.cpp
# End Source File
# Begin Source File

SOURCE=.\AsciiBlob.cpp
# End Source File
# Begin Source File

SOURCE=.\BDB.cpp
# End Source File
# Begin Source File

SOURCE=.\BinaryBlob.cpp
# End Source File
# Begin Source File

SOURCE=.\Bitmap.cpp
# End Source File
# Begin Source File

SOURCE=.\Blob.cpp
# End Source File
# Begin Source File

SOURCE=.\BlobReference.cpp
# End Source File
# Begin Source File

SOURCE=.\Btn.cpp
# End Source File
# Begin Source File

SOURCE=.\Cache.cpp
# End Source File
# Begin Source File

SOURCE=.\Collation.cpp
# End Source File
# Begin Source File

SOURCE=.\CollationCaseless.cpp
# End Source File
# Begin Source File

SOURCE=.\CollationManager.cpp
# End Source File
# Begin Source File

SOURCE=.\CompiledStatement.cpp
# End Source File
# Begin Source File

SOURCE=.\Configuration.cpp
# End Source File
# Begin Source File

SOURCE=.\Connection.cpp
# End Source File
# Begin Source File

SOURCE=.\Context.cpp
# End Source File
# Begin Source File

SOURCE=.\ControlBreak.cpp
# End Source File
# Begin Source File

SOURCE=.\Coterie.cpp
# End Source File
# Begin Source File

SOURCE=.\CoterieRange.cpp
# End Source File
# Begin Source File

SOURCE=.\Database.cpp
# End Source File
# Begin Source File

SOURCE=.\DatabaseMetaData.cpp
# End Source File
# Begin Source File

SOURCE=.\DataOutputStream.cpp
# End Source File
# Begin Source File

SOURCE=.\DataOverflowPage.cpp
# End Source File
# Begin Source File

SOURCE=.\DataPage.cpp
# End Source File
# Begin Source File

SOURCE=.\DataResourceLocator.cpp
# End Source File
# Begin Source File

SOURCE=.\DateTime.cpp
# End Source File
# Begin Source File

SOURCE=.\Dbb.cpp
# End Source File
# Begin Source File

SOURCE=.\Debug.cpp
# End Source File
# Begin Source File

SOURCE=.\Decompress.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\DESKeyTransform.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\DESTransform.cpp
# End Source File
# Begin Source File

SOURCE=.\EditString.cpp
# End Source File
# Begin Source File

SOURCE=.\EncodedDataStream.cpp
# End Source File
# Begin Source File

SOURCE=.\EncodedRecord.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Engine\Engine.def
# End Source File
# Begin Source File

SOURCE=.\Error.cpp
# End Source File
# Begin Source File

SOURCE=.\Event.cpp
# End Source File
# Begin Source File

SOURCE=.\Field.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\FileTransform.cpp
# End Source File
# Begin Source File

SOURCE=.\Filter.cpp
# End Source File
# Begin Source File

SOURCE=.\FilterDifferences.cpp
# End Source File
# Begin Source File

SOURCE=.\FilterSet.cpp
# End Source File
# Begin Source File

SOURCE=.\FilterSetManager.cpp
# End Source File
# Begin Source File

SOURCE=.\FilterTree.cpp
# End Source File
# Begin Source File

SOURCE=.\ForeignKey.cpp
# End Source File
# Begin Source File

SOURCE=.\Format.cpp
# End Source File
# Begin Source File

SOURCE=.\Fsb.cpp
# End Source File
# Begin Source File

SOURCE=.\FsbExhaustive.cpp
# End Source File
# Begin Source File

SOURCE=.\FsbGroup.cpp
# End Source File
# Begin Source File

SOURCE=.\FsbInversion.cpp
# End Source File
# Begin Source File

SOURCE=.\FsbJoin.cpp
# End Source File
# Begin Source File

SOURCE=.\FsbOuterJoin.cpp
# End Source File
# Begin Source File

SOURCE=.\FsbSieve.cpp
# End Source File
# Begin Source File

SOURCE=.\FsbSort.cpp
# End Source File
# Begin Source File

SOURCE=.\FsbUnion.cpp
# End Source File
# Begin Source File

SOURCE=.\Hdr.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\HexTransform.cpp
# End Source File
# Begin Source File

SOURCE=.\Image.cpp
# End Source File
# Begin Source File

SOURCE=.\ImageManager.cpp
# End Source File
# Begin Source File

SOURCE=.\Images.cpp
# End Source File
# Begin Source File

SOURCE=.\Index.cpp
# End Source File
# Begin Source File

SOURCE=.\IndexKey.cpp
# End Source File
# Begin Source File

SOURCE=.\IndexNode.cpp
# End Source File
# Begin Source File

SOURCE=.\IndexPage.cpp
# End Source File
# Begin Source File

SOURCE=.\IndexRootPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Inversion.cpp
# End Source File
# Begin Source File

SOURCE=.\InversionFilter.cpp
# End Source File
# Begin Source File

SOURCE=.\InversionPage.cpp
# End Source File
# Begin Source File

SOURCE=.\InversionWord.cpp
# End Source File
# Begin Source File

SOURCE=.\IO.cpp
# End Source File
# Begin Source File

SOURCE=.\JString.cpp
# End Source File
# Begin Source File

SOURCE=.\KeyGen.cpp
# End Source File
# Begin Source File

SOURCE=.\License.cpp
# End Source File
# Begin Source File

SOURCE=.\LicenseManager.cpp
# End Source File
# Begin Source File

SOURCE=.\LicenseProduct.cpp
# End Source File
# Begin Source File

SOURCE=.\LicenseToken.cpp
# End Source File
# Begin Source File

SOURCE=.\LinkedList.cpp
# End Source File
# Begin Source File

SOURCE=.\Log.cpp
# End Source File
# Begin Source File

SOURCE=.\LogStream.cpp
# End Source File
# Begin Source File

SOURCE=.\MACAddress.cpp
# End Source File
# Begin Source File

SOURCE=.\Manifest.cpp
# End Source File
# Begin Source File

SOURCE=.\ManifestClass.cpp
# End Source File
# Begin Source File

SOURCE=.\ManifestManager.cpp
# End Source File
# Begin Source File

SOURCE=.\MemMgr.cpp
# End Source File
# Begin Source File

SOURCE=.\MemoryResultSet.cpp
# End Source File
# Begin Source File

SOURCE=.\MemoryResultSetColumn.cpp
# End Source File
# Begin Source File

SOURCE=.\MemoryResultSetMetaData.cpp
# End Source File
# Begin Source File

SOURCE=.\MetaDataResultSet.cpp
# End Source File
# Begin Source File

SOURCE=.\Module.cpp
# End Source File
# Begin Source File

SOURCE=.\Mutex.cpp
# End Source File
# Begin Source File

SOURCE=.\NAlias.cpp
# End Source File
# Begin Source File

SOURCE=.\NBitmap.cpp
# End Source File
# Begin Source File

SOURCE=.\NBitSet.cpp
# End Source File
# Begin Source File

SOURCE=.\NCast.cpp
# End Source File
# Begin Source File

SOURCE=.\NConnectionVariable.cpp
# End Source File
# Begin Source File

SOURCE=.\NConstant.cpp
# End Source File
# Begin Source File

SOURCE=.\NDelete.cpp
# End Source File
# Begin Source File

SOURCE=.\NExists.cpp
# End Source File
# Begin Source File

SOURCE=.\NField.cpp
# End Source File
# Begin Source File

SOURCE=.\NInSelect.cpp
# End Source File
# Begin Source File

SOURCE=.\NInSelectBitmap.cpp
# End Source File
# Begin Source File

SOURCE=.\NInsert.cpp
# End Source File
# Begin Source File

SOURCE=.\NMatching.cpp
# End Source File
# Begin Source File

SOURCE=.\NNode.cpp
# End Source File
# Begin Source File

SOURCE=.\NParameter.cpp
# End Source File
# Begin Source File

SOURCE=.\NRecordNumber.cpp
# End Source File
# Begin Source File

SOURCE=.\NRepair.cpp
# End Source File
# Begin Source File

SOURCE=.\NReplace.cpp
# End Source File
# Begin Source File

SOURCE=.\NSelect.cpp
# End Source File
# Begin Source File

SOURCE=.\NSelectExpr.cpp
# End Source File
# Begin Source File

SOURCE=.\NSequence.cpp
# End Source File
# Begin Source File

SOURCE=.\NStat.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\NullTransform.cpp
# End Source File
# Begin Source File

SOURCE=.\NUpdate.cpp
# End Source File
# Begin Source File

SOURCE=.\OpSys.cpp
# End Source File
# Begin Source File

SOURCE=.\Page.cpp
# End Source File
# Begin Source File

SOURCE=.\PageInventoryPage.cpp
# End Source File
# Begin Source File

SOURCE=.\PagePrecedence.cpp
# End Source File
# Begin Source File

SOURCE=.\PageWriter.cpp
# End Source File
# Begin Source File

SOURCE=.\Parameter.cpp
# End Source File
# Begin Source File

SOURCE=.\Parameters.cpp
# End Source File
# Begin Source File

SOURCE=.\PreparedStatement.cpp
# End Source File
# Begin Source File

SOURCE=.\PrettyPrint.cpp
# End Source File
# Begin Source File

SOURCE=.\Privilege.cpp
# End Source File
# Begin Source File

SOURCE=.\PrivilegeObject.cpp
# End Source File
# Begin Source File

SOURCE=.\Protocol.cpp
# End Source File
# Begin Source File

SOURCE=.\PStatement.cpp
# End Source File
# Begin Source File

SOURCE=.\QueryString.cpp
# End Source File
# Begin Source File

SOURCE=.\Record.cpp
# End Source File
# Begin Source File

SOURCE=.\RecordGroup.cpp
# End Source File
# Begin Source File

SOURCE=.\RecordLeaf.cpp
# End Source File
# Begin Source File

SOURCE=.\RecordSection.cpp
# End Source File
# Begin Source File

SOURCE=.\RecordVersion.cpp
# End Source File
# Begin Source File

SOURCE=.\RecoveryPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Registry.cpp
# End Source File
# Begin Source File

SOURCE=.\Repository.cpp
# End Source File
# Begin Source File

SOURCE=.\RepositoryManager.cpp
# End Source File
# Begin Source File

SOURCE=.\RepositoryVolume.cpp
# End Source File
# Begin Source File

SOURCE=.\ResultList.cpp
# End Source File
# Begin Source File

SOURCE=.\ResultSet.cpp
# End Source File
# Begin Source File

SOURCE=.\ResultSetMetaData.cpp
# End Source File
# Begin Source File

SOURCE=.\Role.cpp
# End Source File
# Begin Source File

SOURCE=.\RoleModel.cpp
# End Source File
# Begin Source File

SOURCE=.\RootPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Row.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\RSATransform.cpp
# End Source File
# Begin Source File

SOURCE=.\RSet.cpp
# End Source File
# Begin Source File

SOURCE=.\Scan.cpp
# End Source File
# Begin Source File

SOURCE=.\ScanDir.cpp
# End Source File
# Begin Source File

SOURCE=.\Scavenger.cpp
# End Source File
# Begin Source File

SOURCE=.\Schedule.cpp
# End Source File
# Begin Source File

SOURCE=.\Scheduled.cpp
# End Source File
# Begin Source File

SOURCE=.\ScheduleElement.cpp
# End Source File
# Begin Source File

SOURCE=.\Scheduler.cpp
# End Source File
# Begin Source File

SOURCE=.\Schema.cpp
# End Source File
# Begin Source File

SOURCE=.\Search.cpp
# End Source File
# Begin Source File

SOURCE=.\SearchHit.cpp
# End Source File
# Begin Source File

SOURCE=.\SearchWords.cpp
# End Source File
# Begin Source File

SOURCE=.\Section.cpp
# End Source File
# Begin Source File

SOURCE=.\SectionPage.cpp
# End Source File
# Begin Source File

SOURCE=.\SectionRootPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Sequence.cpp
# End Source File
# Begin Source File

SOURCE=.\SequenceManager.cpp
# End Source File
# Begin Source File

SOURCE=.\SequenceResultSet.cpp
# End Source File
# Begin Source File

SOURCE=.\SerialLog.cpp
# End Source File
# Begin Source File

SOURCE=.\SerialLogAction.cpp
# End Source File
# Begin Source File

SOURCE=.\SerialLogControl.cpp
# End Source File
# Begin Source File

SOURCE=.\SerialLogFile.cpp
# End Source File
# Begin Source File

SOURCE=.\SerialLogRecord.cpp
# End Source File
# Begin Source File

SOURCE=.\SerialLogTransaction.cpp
# End Source File
# Begin Source File

SOURCE=.\SerialLogWindow.cpp
# End Source File
# Begin Source File

SOURCE=.\Server.cpp
# End Source File
# Begin Source File

SOURCE=.\Session.cpp
# End Source File
# Begin Source File

SOURCE=.\SessionManager.cpp
# End Source File
# Begin Source File

SOURCE=.\SessionQueue.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\SHATransform.cpp
# End Source File
# Begin Source File

SOURCE=.\Socket.cpp
# End Source File
# Begin Source File

SOURCE=.\Sort.cpp
# End Source File
# Begin Source File

SOURCE=.\SortMerge.cpp
# End Source File
# Begin Source File

SOURCE=.\SortRecord.cpp
# End Source File
# Begin Source File

SOURCE=.\SortRun.cpp
# End Source File
# Begin Source File

SOURCE=.\SortStream.cpp
# End Source File
# Begin Source File

SOURCE=.\SQLError.cpp
# End Source File
# Begin Source File

SOURCE=.\SQLParse.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLBlobUpdate.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLCheckpoint.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLCommit.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLCreateSection.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLData.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLDataPage.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLDelete.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLDropTable.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLFreePage.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLIndexAdd.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLIndexDelete.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLIndexUpdate.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLPrepare.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLRecordStub.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLRollback.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLSectionPage.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLSequence.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLSwitchLog.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLWordUpdate.cpp
# End Source File
# Begin Source File

SOURCE=.\Stack.cpp
# End Source File
# Begin Source File

SOURCE=.\Statement.cpp
# End Source File
# Begin Source File

SOURCE=.\Stream.cpp
# End Source File
# Begin Source File

SOURCE=.\StreamSegment.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\StreamTransform.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\StringTransform.cpp
# End Source File
# Begin Source File

SOURCE=.\SymbolManager.cpp
# End Source File
# Begin Source File

SOURCE=.\Sync.cpp
# End Source File
# Begin Source File

SOURCE=.\SynchronizationObject.h
# End Source File
# Begin Source File

SOURCE=.\Synchronize.cpp
# End Source File
# Begin Source File

SOURCE=.\SyncObject.cpp
# End Source File
# Begin Source File

SOURCE=.\SyncWait.cpp
# End Source File
# Begin Source File

SOURCE=.\Syntax.cpp
# End Source File
# Begin Source File

SOURCE=.\Table.cpp
# End Source File
# Begin Source File

SOURCE=.\TableAttachment.cpp
# End Source File
# Begin Source File

SOURCE=.\TableFilter.cpp
# End Source File
# Begin Source File

SOURCE=.\Thread.cpp
# End Source File
# Begin Source File

SOURCE=.\ThreadQueue.cpp
# End Source File
# Begin Source File

SOURCE=.\Threads.cpp
# End Source File
# Begin Source File

SOURCE=.\Timestamp.cpp
# End Source File
# Begin Source File

SOURCE=.\Transaction.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\TransformException.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\TransformUtil.cpp
# End Source File
# Begin Source File

SOURCE=.\Trigger.cpp
# End Source File
# Begin Source File

SOURCE=.\TriggerRecord.cpp
# End Source File
# Begin Source File

SOURCE=.\Unicode.cpp
# End Source File
# Begin Source File

SOURCE=.\UnTable.cpp
# End Source File
# Begin Source File

SOURCE=.\User.cpp
# End Source File
# Begin Source File

SOURCE=.\UserRole.cpp
# End Source File
# Begin Source File

SOURCE=.\Validation.cpp
# End Source File
# Begin Source File

SOURCE=.\Value.cpp
# End Source File
# Begin Source File

SOURCE=.\ValueEx.cpp
# End Source File
# Begin Source File

SOURCE=.\Values.cpp
# End Source File
# Begin Source File

SOURCE=.\ValueSet.cpp
# End Source File
# Begin Source File

SOURCE=.\View.cpp
# End Source File
# Begin Source File

SOURCE=.\WString.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "*.h"
# Begin Group "TransformLib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\TransformLib\Base64Transform.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\BERDecode.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\BEREncode.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\BERException.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\BERItem.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\BERObject.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\ZLibTransform.cpp
# End Source File
# Begin Source File

SOURCE=.\TransformLib\ZLibTransform.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Agent.h
# End Source File
# Begin Source File

SOURCE=.\Alias.h
# End Source File
# Begin Source File

SOURCE=.\AppEvent.h
# End Source File
# Begin Source File

SOURCE=.\Application.h
# End Source File
# Begin Source File

SOURCE=.\Applications.h
# End Source File
# Begin Source File

SOURCE=.\AsciiBlob.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\Base64Transform.h
# End Source File
# Begin Source File

SOURCE=.\BDB.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\BERDecode.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\BEREncode.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\BERException.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\BERItem.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\BERObject.h
# End Source File
# Begin Source File

SOURCE=.\BigInt.h
# End Source File
# Begin Source File

SOURCE=.\BigInteger.h
# End Source File
# Begin Source File

SOURCE=.\BinaryBlob.h
# End Source File
# Begin Source File

SOURCE=.\Bitmap.h
# End Source File
# Begin Source File

SOURCE=.\Blob.h
# End Source File
# Begin Source File

SOURCE=.\BlobReference.h
# End Source File
# Begin Source File

SOURCE=.\Btn.h
# End Source File
# Begin Source File

SOURCE=.\Cache.h
# End Source File
# Begin Source File

SOURCE=.\Collation.h
# End Source File
# Begin Source File

SOURCE=.\CollationCaseless.h
# End Source File
# Begin Source File

SOURCE=.\CollationManager.h
# End Source File
# Begin Source File

SOURCE=.\CollationUnknown.h
# End Source File
# Begin Source File

SOURCE=.\CompiledStatement.h
# End Source File
# Begin Source File

SOURCE=.\Configuration.h
# End Source File
# Begin Source File

SOURCE=.\Connection.h
# End Source File
# Begin Source File

SOURCE=.\Context.h
# End Source File
# Begin Source File

SOURCE=.\ControlBreak.h
# End Source File
# Begin Source File

SOURCE=.\Coterie.h
# End Source File
# Begin Source File

SOURCE=.\CoterieRange.h
# End Source File
# Begin Source File

SOURCE=.\TomCryptLib\CryptLib.h
# End Source File
# Begin Source File

SOURCE=.\Database.h
# End Source File
# Begin Source File

SOURCE=.\DatabaseBackup.h
# End Source File
# Begin Source File

SOURCE=.\DatabaseClone.h
# End Source File
# Begin Source File

SOURCE=.\DatabaseCopy.h
# End Source File
# Begin Source File

SOURCE=.\DatabaseMetaData.h
# End Source File
# Begin Source File

SOURCE=.\DataOutputStream.h
# End Source File
# Begin Source File

SOURCE=.\DataOverflowPage.h
# End Source File
# Begin Source File

SOURCE=.\DataPage.h
# End Source File
# Begin Source File

SOURCE=.\DataResourceLocator.h
# End Source File
# Begin Source File

SOURCE=.\DateTime.h
# End Source File
# Begin Source File

SOURCE=.\Dbb.h
# End Source File
# Begin Source File

SOURCE=.\Debug.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\DecodeTransform.h
# End Source File
# Begin Source File

SOURCE=.\Decompress.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\DecryptTransform.h
# End Source File
# Begin Source File

SOURCE=.\DeferredIndex.h
# End Source File
# Begin Source File

SOURCE=.\DeferredIndexWalker.h
# End Source File
# Begin Source File

SOURCE=.\ZLib\deflate.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\DESKeyTransform.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\DESTransform.h
# End Source File
# Begin Source File

SOURCE=.\Document.h
# End Source File
# Begin Source File

SOURCE=.\EditString.h
# End Source File
# Begin Source File

SOURCE=.\EncodedDataStream.h
# End Source File
# Begin Source File

SOURCE=.\EncodedRecord.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\EncodeTransform.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\EncryptTransform.h
# End Source File
# Begin Source File

SOURCE=.\Engine.h
# End Source File
# Begin Source File

SOURCE=.\Error.h
# End Source File
# Begin Source File

SOURCE=.\Event.h
# End Source File
# Begin Source File

SOURCE=.\Field.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\FileTransform.h
# End Source File
# Begin Source File

SOURCE=.\Filter.h
# End Source File
# Begin Source File

SOURCE=.\FilterDifferences.h
# End Source File
# Begin Source File

SOURCE=.\FilterSet.h
# End Source File
# Begin Source File

SOURCE=.\FilterSetManager.h
# End Source File
# Begin Source File

SOURCE=.\FilterTree.h
# End Source File
# Begin Source File

SOURCE=.\ForeignKey.h
# End Source File
# Begin Source File

SOURCE=.\Format.h
# End Source File
# Begin Source File

SOURCE=.\Fsb.h
# End Source File
# Begin Source File

SOURCE=.\FsbDerivedTable.h
# End Source File
# Begin Source File

SOURCE=.\FsbExhaustive.h
# End Source File
# Begin Source File

SOURCE=.\FsbGroup.h
# End Source File
# Begin Source File

SOURCE=.\FsbInversion.h
# End Source File
# Begin Source File

SOURCE=.\FsbJoin.h
# End Source File
# Begin Source File

SOURCE=.\FsbOuterJoin.h
# End Source File
# Begin Source File

SOURCE=.\FsbSieve.h
# End Source File
# Begin Source File

SOURCE=.\FsbSort.h
# End Source File
# Begin Source File

SOURCE=.\FsbUnion.h
# End Source File
# Begin Source File

SOURCE=.\GenOption.h
# End Source File
# Begin Source File

SOURCE=.\Hdr.h
# End Source File
# Begin Source File

SOURCE=.\HdrState.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\HexTransform.h
# End Source File
# Begin Source File

SOURCE=.\Word\HtmlGen.h
# End Source File
# Begin Source File

SOURCE=.\Image.h
# End Source File
# Begin Source File

SOURCE=.\ImageManager.h
# End Source File
# Begin Source File

SOURCE=.\Images.h
# End Source File
# Begin Source File

SOURCE=.\Index.h
# End Source File
# Begin Source File

SOURCE=.\Index2Node.h
# End Source File
# Begin Source File

SOURCE=.\Index2RootPage.h
# End Source File
# Begin Source File

SOURCE=.\IndexKey.h
# End Source File
# Begin Source File

SOURCE=.\IndexLimits.h
# End Source File
# Begin Source File

SOURCE=.\IndexNode.h
# End Source File
# Begin Source File

SOURCE=.\IndexPage.h
# End Source File
# Begin Source File

SOURCE=.\IndexRootPage.h
# End Source File
# Begin Source File

SOURCE=.\Interlock.h
# End Source File
# Begin Source File

SOURCE=.\Inversion.h
# End Source File
# Begin Source File

SOURCE=.\InversionFilter.h
# End Source File
# Begin Source File

SOURCE=.\InversionPage.h
# End Source File
# Begin Source File

SOURCE=.\InversionWord.h
# End Source File
# Begin Source File

SOURCE=.\IOx.h
# End Source File
# Begin Source File

SOURCE=.\Java.h
# End Source File
# Begin Source File

SOURCE=.\JavaArchive.h
# End Source File
# Begin Source File

SOURCE=.\JavaArchiveFile.h
# End Source File
# Begin Source File

SOURCE=.\JavaBreakpoint.h
# End Source File
# Begin Source File

SOURCE=.\JavaClass.h
# End Source File
# Begin Source File

SOURCE=.\JString.h
# End Source File
# Begin Source File

SOURCE=.\KeyGen.h
# End Source File
# Begin Source File

SOURCE=.\License.h
# End Source File
# Begin Source File

SOURCE=.\LicenseManager.h
# End Source File
# Begin Source File

SOURCE=.\LicenseProduct.h
# End Source File
# Begin Source File

SOURCE=.\LicenseToken.h
# End Source File
# Begin Source File

SOURCE=.\LinkedList.h
# End Source File
# Begin Source File

SOURCE=.\Log.h
# End Source File
# Begin Source File

SOURCE=.\LogLock.h
# End Source File
# Begin Source File

SOURCE=.\LogStream.h
# End Source File
# Begin Source File

SOURCE=.\MACAddress.h
# End Source File
# Begin Source File

SOURCE=.\Manifest.h
# End Source File
# Begin Source File

SOURCE=.\ManifestClass.h
# End Source File
# Begin Source File

SOURCE=.\ManifestManager.h
# End Source File
# Begin Source File

SOURCE=.\MemMgr.h
# End Source File
# Begin Source File

SOURCE=.\MemoryManager.h
# End Source File
# Begin Source File

SOURCE=.\MemoryResultSet.h
# End Source File
# Begin Source File

SOURCE=.\MemoryResultSetColumn.h
# End Source File
# Begin Source File

SOURCE=.\MemoryResultSetMetaData.h
# End Source File
# Begin Source File

SOURCE=.\MetaDataResultSet.h
# End Source File
# Begin Source File

SOURCE=.\Module.h
# End Source File
# Begin Source File

SOURCE=.\MsgType.h
# End Source File
# Begin Source File

SOURCE=.\Mutex.h
# End Source File
# Begin Source File

SOURCE=.\NAlias.h
# End Source File
# Begin Source File

SOURCE=.\NBitmap.h
# End Source File
# Begin Source File

SOURCE=.\NBitSet.h
# End Source File
# Begin Source File

SOURCE=.\NCast.h
# End Source File
# Begin Source File

SOURCE=.\NConnectionVariable.h
# End Source File
# Begin Source File

SOURCE=.\NConstant.h
# End Source File
# Begin Source File

SOURCE=.\NDelete.h
# End Source File
# Begin Source File

SOURCE=.\netfra_jni.h
# End Source File
# Begin Source File

SOURCE=.\NetfraVersion.h
# End Source File
# Begin Source File

SOURCE=.\NExists.h
# End Source File
# Begin Source File

SOURCE=.\NField.h
# End Source File
# Begin Source File

SOURCE=.\NInSelect.h
# End Source File
# Begin Source File

SOURCE=.\NInSelectBitmap.h
# End Source File
# Begin Source File

SOURCE=.\NInsert.h
# End Source File
# Begin Source File

SOURCE=.\NMatching.h
# End Source File
# Begin Source File

SOURCE=.\NNode.h
# End Source File
# Begin Source File

SOURCE=.\nodes.h
# End Source File
# Begin Source File

SOURCE=.\NParameter.h
# End Source File
# Begin Source File

SOURCE=.\NRecordNumber.h
# End Source File
# Begin Source File

SOURCE=.\NRepair.h
# End Source File
# Begin Source File

SOURCE=.\NReplace.h
# End Source File
# Begin Source File

SOURCE=.\NSelect.h
# End Source File
# Begin Source File

SOURCE=.\NSelectExpr.h
# End Source File
# Begin Source File

SOURCE=.\NSequence.h
# End Source File
# Begin Source File

SOURCE=.\NStat.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\NullTransform.h
# End Source File
# Begin Source File

SOURCE=.\NUpdate.h
# End Source File
# Begin Source File

SOURCE=.\OpSys.h
# End Source File
# Begin Source File

SOURCE=.\Page.h
# End Source File
# Begin Source File

SOURCE=.\PageInventoryPage.h
# End Source File
# Begin Source File

SOURCE=.\PagePrecedence.h
# End Source File
# Begin Source File

SOURCE=.\PageType.h
# End Source File
# Begin Source File

SOURCE=.\PageWriter.h
# End Source File
# Begin Source File

SOURCE=.\Parameter.h
# End Source File
# Begin Source File

SOURCE=.\Parameters.h
# End Source File
# Begin Source File

SOURCE=.\PreparedStatement.h
# End Source File
# Begin Source File

SOURCE=.\PrettyPrint.h
# End Source File
# Begin Source File

SOURCE=.\Privilege.h
# End Source File
# Begin Source File

SOURCE=.\PrivilegeObject.h
# End Source File
# Begin Source File

SOURCE=.\PrivType.h
# End Source File
# Begin Source File

SOURCE=.\Properties.h
# End Source File
# Begin Source File

SOURCE=.\Protocol.h
# End Source File
# Begin Source File

SOURCE=.\PStatement.h
# End Source File
# Begin Source File

SOURCE=.\QueryString.h
# End Source File
# Begin Source File

SOURCE=.\Queue.h
# End Source File
# Begin Source File

SOURCE=.\Record.h
# End Source File
# Begin Source File

SOURCE=.\RecordGroup.h
# End Source File
# Begin Source File

SOURCE=.\RecordLeaf.h
# End Source File
# Begin Source File

SOURCE=.\RecordLocatorPage.h
# End Source File
# Begin Source File

SOURCE=.\RecordSection.h
# End Source File
# Begin Source File

SOURCE=.\RecordVersion.h
# End Source File
# Begin Source File

SOURCE=.\RecoveryObjects.h
# End Source File
# Begin Source File

SOURCE=.\RecoveryPage.h
# End Source File
# Begin Source File

SOURCE=.\Registry.h
# End Source File
# Begin Source File

SOURCE=.\Repository.h
# End Source File
# Begin Source File

SOURCE=.\RepositoryManager.h
# End Source File
# Begin Source File

SOURCE=.\RepositoryVolume.h
# End Source File
# Begin Source File

SOURCE=.\ResultList.h
# End Source File
# Begin Source File

SOURCE=.\ResultSet.h
# End Source File
# Begin Source File

SOURCE=.\ResultSetMetaData.h
# End Source File
# Begin Source File

SOURCE=.\Role.h
# End Source File
# Begin Source File

SOURCE=.\RoleModel.h
# End Source File
# Begin Source File

SOURCE=.\RootPage.h
# End Source File
# Begin Source File

SOURCE=.\Row.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\RSATransform.h
# End Source File
# Begin Source File

SOURCE=.\RSet.h
# End Source File
# Begin Source File

SOURCE=.\SavePoint.h
# End Source File
# Begin Source File

SOURCE=.\Scan.h
# End Source File
# Begin Source File

SOURCE=.\ScanDir.h
# End Source File
# Begin Source File

SOURCE=.\ScanType.h
# End Source File
# Begin Source File

SOURCE=.\Scavenger.h
# End Source File
# Begin Source File

SOURCE=.\Schedule.h
# End Source File
# Begin Source File

SOURCE=.\Scheduled.h
# End Source File
# Begin Source File

SOURCE=.\ScheduleElement.h
# End Source File
# Begin Source File

SOURCE=.\Scheduler.h
# End Source File
# Begin Source File

SOURCE=.\Schema.h
# End Source File
# Begin Source File

SOURCE=.\Search.h
# End Source File
# Begin Source File

SOURCE=.\SearchHit.h
# End Source File
# Begin Source File

SOURCE=.\SearchWords.h
# End Source File
# Begin Source File

SOURCE=.\Section.h
# End Source File
# Begin Source File

SOURCE=.\SectionPage.h
# End Source File
# Begin Source File

SOURCE=.\SectionRootPage.h
# End Source File
# Begin Source File

SOURCE=.\Sequence.h
# End Source File
# Begin Source File

SOURCE=.\SequenceManager.h
# End Source File
# Begin Source File

SOURCE=.\SequencePage.h
# End Source File
# Begin Source File

SOURCE=.\SequenceResultSet.h
# End Source File
# Begin Source File

SOURCE=.\SerialLog.h
# End Source File
# Begin Source File

SOURCE=.\SerialLogAction.h
# End Source File
# Begin Source File

SOURCE=.\SerialLogControl.h
# End Source File
# Begin Source File

SOURCE=.\SerialLogFile.h
# End Source File
# Begin Source File

SOURCE=.\SerialLogRecord.h
# End Source File
# Begin Source File

SOURCE=.\SerialLogTransaction.h
# End Source File
# Begin Source File

SOURCE=.\SerialLogWindow.h
# End Source File
# Begin Source File

SOURCE=.\Server.h
# End Source File
# Begin Source File

SOURCE=.\Session.h
# End Source File
# Begin Source File

SOURCE=.\SessionManager.h
# End Source File
# Begin Source File

SOURCE=.\SessionQueue.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\SHATransform.h
# End Source File
# Begin Source File

SOURCE=.\Socket.h
# End Source File
# Begin Source File

SOURCE=.\Sort.h
# End Source File
# Begin Source File

SOURCE=.\SortMerge.h
# End Source File
# Begin Source File

SOURCE=.\SortRecord.h
# End Source File
# Begin Source File

SOURCE=.\SortRun.h
# End Source File
# Begin Source File

SOURCE=.\SortStream.h
# End Source File
# Begin Source File

SOURCE=.\SQLError.h
# End Source File
# Begin Source File

SOURCE=.\SQLException.h
# End Source File
# Begin Source File

SOURCE=.\SQLParse.h
# End Source File
# Begin Source File

SOURCE=.\SRLBlobUpdate.h
# End Source File
# Begin Source File

SOURCE=.\SRLCheckpoint.h
# End Source File
# Begin Source File

SOURCE=.\SRLCommit.h
# End Source File
# Begin Source File

SOURCE=.\SRLCreateIndex.h
# End Source File
# Begin Source File

SOURCE=.\SRLCreateSection.h
# End Source File
# Begin Source File

SOURCE=.\SRLCreateTableSpace.h
# End Source File
# Begin Source File

SOURCE=.\SRLData.h
# End Source File
# Begin Source File

SOURCE=.\SRLDataPage.h
# End Source File
# Begin Source File

SOURCE=.\SRLDelete.h
# End Source File
# Begin Source File

SOURCE=.\SRLDeleteIndex.h
# End Source File
# Begin Source File

SOURCE=.\SRLDropTable.h
# End Source File
# Begin Source File

SOURCE=.\SRLDropTableSpace.h
# End Source File
# Begin Source File

SOURCE=.\SRLFreePage.h
# End Source File
# Begin Source File

SOURCE=.\SRLIndexAdd.h
# End Source File
# Begin Source File

SOURCE=.\SRLIndexDelete.h
# End Source File
# Begin Source File

SOURCE=.\SRLIndexPage.h
# End Source File
# Begin Source File

SOURCE=.\SRLIndexUpdate.h
# End Source File
# Begin Source File

SOURCE=.\SRLInversionPage.h
# End Source File
# Begin Source File

SOURCE=.\SRLOverflowPages.h
# End Source File
# Begin Source File

SOURCE=.\SRLPrepare.h
# End Source File
# Begin Source File

SOURCE=.\SRLRecordLocator.h
# End Source File
# Begin Source File

SOURCE=.\SRLRecordStub.h
# End Source File
# Begin Source File

SOURCE=.\SRLRollback.h
# End Source File
# Begin Source File

SOURCE=.\SRLSectionLine.h
# End Source File
# Begin Source File

SOURCE=.\SRLSectionPage.h
# End Source File
# Begin Source File

SOURCE=.\SRLSectionPromotion.h
# End Source File
# Begin Source File

SOURCE=.\SRLSequence.h
# End Source File
# Begin Source File

SOURCE=.\SRLSequencePage.h
# End Source File
# Begin Source File

SOURCE=.\SRLSwitchLog.h
# End Source File
# Begin Source File

SOURCE=.\SRLUpdateIndex.h
# End Source File
# Begin Source File

SOURCE=.\SRLUpdateRecords.h
# End Source File
# Begin Source File

SOURCE=.\SRLVersion.h
# End Source File
# Begin Source File

SOURCE=.\SRLWordUpdate.h
# End Source File
# Begin Source File

SOURCE=.\Stack.h
# End Source File
# Begin Source File

SOURCE=.\Statement.h
# End Source File
# Begin Source File

SOURCE=.\Stream.h
# End Source File
# Begin Source File

SOURCE=.\StreamSegment.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\StreamTransform.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\StringTransform.h
# End Source File
# Begin Source File

SOURCE=.\SymbolManager.h
# End Source File
# Begin Source File

SOURCE=.\Sync.h
# End Source File
# Begin Source File

SOURCE=.\Synchronize.h
# End Source File
# Begin Source File

SOURCE=.\SyncObject.h
# End Source File
# Begin Source File

SOURCE=.\SyncWait.h
# End Source File
# Begin Source File

SOURCE=.\Syntax.h
# End Source File
# Begin Source File

SOURCE=.\Table.h
# End Source File
# Begin Source File

SOURCE=.\TableAttachment.h
# End Source File
# Begin Source File

SOURCE=.\TableFilter.h
# End Source File
# Begin Source File

SOURCE=.\TableSpace.h
# End Source File
# Begin Source File

SOURCE=.\TableSpaceManager.h
# End Source File
# Begin Source File

SOURCE=.\TextFormat.h
# End Source File
# Begin Source File

SOURCE=.\Thread.h
# End Source File
# Begin Source File

SOURCE=.\ThreadQueue.h
# End Source File
# Begin Source File

SOURCE=.\Threads.h
# End Source File
# Begin Source File

SOURCE=.\Timestamp.h
# End Source File
# Begin Source File

SOURCE=.\Transaction.h
# End Source File
# Begin Source File

SOURCE=.\TransactionManager.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\Transform.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\TransformException.h
# End Source File
# Begin Source File

SOURCE=.\TransformLib\TransformUtil.h
# End Source File
# Begin Source File

SOURCE=.\Trigger.h
# End Source File
# Begin Source File

SOURCE=.\TriggerRecord.h
# End Source File
# Begin Source File

SOURCE=.\Types.h
# End Source File
# Begin Source File

SOURCE=.\Unicode.h
# End Source File
# Begin Source File

SOURCE=.\UnTable.h
# End Source File
# Begin Source File

SOURCE=.\User.h
# End Source File
# Begin Source File

SOURCE=.\UserRole.h
# End Source File
# Begin Source File

SOURCE=.\Validation.h
# End Source File
# Begin Source File

SOURCE=.\Value.h
# End Source File
# Begin Source File

SOURCE=.\ValueEx.h
# End Source File
# Begin Source File

SOURCE=.\Values.h
# End Source File
# Begin Source File

SOURCE=.\ValueSet.h
# End Source File
# Begin Source File

SOURCE=.\View.h
# End Source File
# Begin Source File

SOURCE=.\WString.h
# End Source File
# End Group
# Begin Group "JVM"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\Java.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\Java.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaArchive.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaArchive.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaArchiveFile.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaArchiveFile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaBreakpoint.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaBreakpoint.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaClass.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaClass.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaCluster.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaCluster.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaCodes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaConnection.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaConnection.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDbArchive.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDbArchive.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDebugger.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDebugger.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDir.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDir.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDirectory.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDirectory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDispatch.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDispatch.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDispatchArgument.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDispatchArgument.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDispatchCharacter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDispatchCharacter.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDispatchFunction.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDispatchFunction.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaDType.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaEnv.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaEnv.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaGenNative.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaGenNative.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaLibrary.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaLibrary.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaLoad.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaLoad.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaLoadArchive.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaLoadArchive.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaLoadDb.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaLoadDb.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaLoadFile.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaLoadFile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaMethod.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaMethod.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaNative.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaNative.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaNativeMethods.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaNativeMethods.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaPrimitive.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaPrimitive.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaRef.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaRef.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaStall.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaStall.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaString.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaThread.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaThread.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaType.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaType.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaVM.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaVM.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaWait.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaWait.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaWordHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\JVM\JavaWordHandler.h
# End Source File
# End Group
# Begin Group "Templates"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\Template.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\Template.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateAggregate.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateAggregate.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateContext.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateContext.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateCriterion.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateCriterion.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateEval.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateEval.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateManager.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateManager.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateParse.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateParse.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\Templates.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\Templates.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateVariable.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Templates\TemplateVariable.h
# End Source File
# End Group
# Begin Group "Word"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\HtmlFont.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\HtmlFont.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\HtmlGen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\HtmlGen.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\MetaFile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\MetaProperties.h
# End Source File
# Begin Source File

SOURCE=.\Word\MSWord.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\MSWord.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordBinTable.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordBinTable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordBlip.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordBlip.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordBlipImg.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordBlipImg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordBlockList.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordBlockList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordBookmark.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordBookmark.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordContainer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordContainer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordDepot.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordDepot.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordDoc.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordDoc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordDrawings.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordDrawings.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordException.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordException.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordField.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordField.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordFontInfo.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordFontInfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordFontX.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordFontX.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordGen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordGen.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordGraphic.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordGraphic.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordImage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordImage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordInputStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordInputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordLFO_LVL.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordLFO_LVL.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordList.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordList.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordListLevel.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordListLevel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordListOverride.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordListOverride.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordOption.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordOption.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordOptions.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordOptions.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordPicture.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordPicture.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordPropertySet.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordPropertySet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordStyle.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordStyle.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordTable.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordTable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordTableStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordTableStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordTestHandler.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordTestHandler.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordTextBlock.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Word\WordTextBlock.h
# End Source File
# End Group
# Begin Group "Pdf"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\Pdf.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\Pdf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfArray.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfArray.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfBaseFonts.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfBoolean.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfBoolean.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfDict.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfDict.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfDictionary.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfDictionary.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfDictionaryItem.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfDictionaryItem.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfException.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfException.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfFont.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfFont.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfGen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfGen.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfIndirectObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfIndirectObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfItem.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfItem.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfLZW.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfLZW.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfMatrix.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfMatrix.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfName.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfName.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfNull.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfNull.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfNumber.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfNumber.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfObj.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfObj.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfObject.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfObject.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfPage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfPage.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfPath.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfPath.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfPS.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfPS.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfReference.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfReference.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfSection.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfSection.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfSegment.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfSegment.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfSource.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfSource.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfString.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfString.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfText.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfText.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfToken.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfToken.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfTransformation.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfTransformation.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfXRef.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PdfXRef.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PSToken.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Pdf\PSToken.h
# End Source File
# End Group
# Begin Group "TomCryptLib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_argchk.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_cipher_descriptor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_cipher_is_valid.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_find_hash.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_find_hash_id.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_find_prng.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_hash_descriptor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_hash_is_valid.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_prng_descriptor.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_prng_is_valid.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_register_cipher.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_register_hash.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\crypt_register_prng.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\CryptLib.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\CryptLib.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\ctr_encrypt.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\ctr_start.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\der_decode_integer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\der_encode_integer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\der_get_multi_integer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\der_length_integer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\der_put_multi_integer.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\des.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\error_to_string.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\hash_memory.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\ltc_tommath.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\mpi.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\mpi_to_ltc_error.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\pkcs_1_mgf1.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\pkcs_1_oaep_decode.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\pkcs_1_oaep_encode.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\rand_prime.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\rsa_export.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\rsa_exptmod.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\rsa_free.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\rsa_import.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\rsa_make_key.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\sha1.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\sha256.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tomcrypt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tomcrypt_argchk.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tomcrypt_cfg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tomcrypt_cipher.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tomcrypt_custom.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tomcrypt_hash.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tomcrypt_macros.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tomcrypt_misc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tomcrypt_pk.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tomcrypt_pkcs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tomcrypt_prng.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tommath_class.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\tommath_superclass.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\yarrow.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\TomCryptLib\zeromem.cpp
# End Source File
# End Group
# Begin Group "Xml"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\netfrastructure\Xml\AdminException.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Xml\AdminException.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Xml\Element.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Xml\Element.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Xml\InputFile.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Xml\InputStream.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Xml\InputStream.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Xml\Lex.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Xml\Lex.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Xml\XMLParse.cpp
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\Xml\XMLParse.h
# End Source File
# End Group
# Begin Group "ZLib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\adler32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\compress.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\crc32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\deflate.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\deflate.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\example.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\gzio.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\infblock.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\infblock.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\infcodes.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\infcodes.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\inffast.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\inffast.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\inffixed.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\inflate.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\inftrees.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\inftrees.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\infutil.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\infutil.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\trees.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\trees.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\uncompr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\zconf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\zlib.h
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\zutil.c
# End Source File
# Begin Source File

SOURCE=..\..\..\netfrastructure\ZLib\zutil.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\BigInt.cpp
# End Source File
# Begin Source File

SOURCE=.\BigInteger.cpp
# End Source File
# Begin Source File

SOURCE=.\CollationUnknown.cpp
# End Source File
# Begin Source File

SOURCE=.\DatabaseBackup.cpp
# End Source File
# Begin Source File

SOURCE=.\DatabaseClone.cpp
# End Source File
# Begin Source File

SOURCE=.\DatabaseCopy.cpp
# End Source File
# Begin Source File

SOURCE=.\DeferredIndex.cpp
# End Source File
# Begin Source File

SOURCE=.\DeferredIndexWalker.cpp
# End Source File
# Begin Source File

SOURCE=.\FsbDerivedTable.cpp
# End Source File
# Begin Source File

SOURCE=.\Index2Node.cpp
# End Source File
# Begin Source File

SOURCE=.\Index2Page.cpp
# End Source File
# Begin Source File

SOURCE=.\Index2RootPage.cpp
# End Source File
# Begin Source File

SOURCE=.\LogLock.cpp
# End Source File
# Begin Source File

SOURCE=.\RecordLocatorPage.cpp
# End Source File
# Begin Source File

SOURCE=.\RecoveryObjects.cpp
# End Source File
# Begin Source File

SOURCE=.\SavePoint.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLCreateIndex.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLCreateTableSpace.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLDeleteIndex.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLDropTableSpace.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLIndexPage.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLInversionPage.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLOverflowPages.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLRecordLocator.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLSectionLine.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLSectionPromotion.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLSequencePage.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLUpdateIndex.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLUpdateRecords.cpp
# End Source File
# Begin Source File

SOURCE=.\SRLVersion.cpp
# End Source File
# Begin Source File

SOURCE=.\TableSpace.cpp
# End Source File
# Begin Source File

SOURCE=.\TableSpaceManager.cpp
# End Source File
# Begin Source File

SOURCE=.\TransactionManager.cpp
# End Source File
# End Target
# End Project
