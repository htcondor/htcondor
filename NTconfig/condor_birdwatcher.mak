# Microsoft Developer Studio Generated NMAKE File, Based on condor_birdwatcher.dsp
!IF "$(CFG)" == ""
CFG=condor_birdwatcher - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_birdwatcher - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_birdwatcher - Win32 Release" && "$(CFG)" != "condor_birdwatcher - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_birdwatcher.mak" CFG="condor_birdwatcher - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_birdwatcher - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "condor_birdwatcher - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_birdwatcher - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_birdwatcher.exe"


CLEAN :
	-@erase "$(INTDIR)\birdwatcher.obj"
	-@erase "$(INTDIR)\birdwatcher.res"
	-@erase "$(INTDIR)\birdWatcherDlg.obj"
	-@erase "$(INTDIR)\condor_birdwatcher.pch"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\SystrayManager.obj"
	-@erase "$(INTDIR)\SystrayMinimize.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\WindowsMessageReceiver.obj"
	-@erase "$(OUTDIR)\condor_birdwatcher.exe"
	-@erase "$(OUTDIR)\condor_birdwatcher.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\condor_birdwatcher.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\birdwatcher.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_birdwatcher.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=/nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\condor_birdwatcher.pdb" /machine:I386 /out:"$(OUTDIR)\condor_birdwatcher.exe" 
LINK32_OBJS= \
	"$(INTDIR)\birdwatcher.obj" \
	"$(INTDIR)\birdWatcherDlg.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\SystrayManager.obj" \
	"$(INTDIR)\SystrayMinimize.obj" \
	"$(INTDIR)\WindowsMessageReceiver.obj" \
	"$(INTDIR)\birdwatcher.res"

"$(OUTDIR)\condor_birdwatcher.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_birdwatcher - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_birdwatcher.exe"


CLEAN :
	-@erase "$(INTDIR)\birdwatcher.obj"
	-@erase "$(INTDIR)\birdwatcher.res"
	-@erase "$(INTDIR)\birdWatcherDlg.obj"
	-@erase "$(INTDIR)\condor_birdwatcher.pch"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\SystrayManager.obj"
	-@erase "$(INTDIR)\SystrayMinimize.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\WindowsMessageReceiver.obj"
	-@erase "$(OUTDIR)\condor_birdwatcher.exe"
	-@erase "$(OUTDIR)\condor_birdwatcher.ilk"
	-@erase "$(OUTDIR)\condor_birdwatcher.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\condor_birdwatcher.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\birdwatcher.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_birdwatcher.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=/nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\condor_birdwatcher.pdb" /debug /machine:I386 /out:"$(OUTDIR)\condor_birdwatcher.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\birdwatcher.obj" \
	"$(INTDIR)\birdWatcherDlg.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\SystrayManager.obj" \
	"$(INTDIR)\SystrayMinimize.obj" \
	"$(INTDIR)\WindowsMessageReceiver.obj" \
	"$(INTDIR)\birdwatcher.res"

"$(OUTDIR)\condor_birdwatcher.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_birdwatcher.dep")
!INCLUDE "condor_birdwatcher.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_birdwatcher.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_birdwatcher - Win32 Release" || "$(CFG)" == "condor_birdwatcher - Win32 Debug"
SOURCE=..\src\condor_birdwatcher\birdwatcher.cpp

"$(INTDIR)\birdwatcher.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_birdwatcher.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_birdwatcher\birdwatcher.rc

!IF  "$(CFG)" == "condor_birdwatcher - Win32 Release"


"$(INTDIR)\birdwatcher.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x409 /fo"$(INTDIR)\birdwatcher.res" /i "\condor-v66\src\condor_birdwatcher" /d "NDEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_birdwatcher - Win32 Debug"


"$(INTDIR)\birdwatcher.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x409 /fo"$(INTDIR)\birdwatcher.res" /i "\condor-v66\src\condor_birdwatcher" /d "_DEBUG" $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_birdwatcher\birdWatcherDlg.cpp

"$(INTDIR)\birdWatcherDlg.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_birdwatcher.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_birdwatcher\StdAfx.cpp

!IF  "$(CFG)" == "condor_birdwatcher - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\condor_birdwatcher.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\condor_birdwatcher.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_birdwatcher - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Fp"$(INTDIR)\condor_birdwatcher.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\condor_birdwatcher.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_birdwatcher\SystrayManager.cpp

"$(INTDIR)\SystrayManager.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_birdwatcher.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_birdwatcher\SystrayMinimize.cpp

"$(INTDIR)\SystrayMinimize.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_birdwatcher.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_birdwatcher\WindowsMessageReceiver.cpp

"$(INTDIR)\WindowsMessageReceiver.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_birdwatcher.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

