# Microsoft Developer Studio Project File - Name="myisammrg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=myisammrg - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "myisammrg.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "myisammrg.mak" CFG="myisammrg - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "myisammrg - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "myisammrg - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
RSC=rc.exe

!IF  "$(CFG)" == "myisammrg - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_release\myisammrg.lib"

!ELSEIF  "$(CFG)" == "myisammrg - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /Gf /I "../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /Fo".\Debug/" /Fd".\Debug/" /FD /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_Debug\myisammrg.lib"

!ENDIF

# Begin Target

# Name "myisammrg - Win32 Release"
# Name "myisammrg - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\myrg_close.c
# End Source File
# Begin Source File

SOURCE=.\myrg_create.c
# End Source File
# Begin Source File

SOURCE=.\myrg_delete.c
# End Source File
# Begin Source File

SOURCE=.\myrg_extra.c
# End Source File
# Begin Source File

SOURCE=.\myrg_info.c
# End Source File
# Begin Source File

SOURCE=.\myrg_locking.c
# End Source File
# Begin Source File

SOURCE=.\myrg_open.c
# End Source File
# Begin Source File

SOURCE=.\myrg_panic.c
# End Source File
# Begin Source File

SOURCE=.\myrg_queue.c
# End Source File
# Begin Source File

SOURCE=.\myrg_range.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rfirst.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rkey.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rlast.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rnext.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rprev.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rrnd.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rsame.c
# End Source File
# Begin Source File

SOURCE=.\myrg_static.c
# End Source File
# Begin Source File

SOURCE=.\myrg_update.c
# End Source File
# Begin Source File

SOURCE=.\myrg_write.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\mymrgdef.h
# End Source File
# End Group
# End Target
# End Project
