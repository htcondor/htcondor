# Microsoft Developer Studio Generated NMAKE File, Based on condor_reschedule.dsp
!IF "$(CFG)" == ""
CFG=condor_reschedule - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_reschedule - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_reschedule - Win32 Release" && "$(CFG)" !=\
 "condor_reschedule - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_reschedule.mak" CFG="condor_reschedule - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_reschedule - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "condor_reschedule - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_reschedule - Win32 Release"

OUTDIR=.\..\src\condor_tools
INTDIR=.\..\src\condor_tools
# Begin Custom Macros
OutDir=.\..\src\condor_tools
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_reschedule.exe"

!ELSE 

ALL : "condor_classad - Win32 Release" "condor_io - Win32 Release"\
 "condor_util_lib - Win32 Release" "condor_cpp_util - Win32 Release"\
 "$(OUTDIR)\condor_reschedule.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_cpp_util - Win32 ReleaseCLEAN"\
 "condor_util_lib - Win32 ReleaseCLEAN" "condor_io - Win32 ReleaseCLEAN"\
 "condor_classad - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\reschedule.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_reschedule.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_tools/
CPP_SBRS=.
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_reschedule.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ws2_32.lib /nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)\condor_reschedule.pdb" /machine:I386\
 /out:"$(OUTDIR)\condor_reschedule.exe" 
LINK32_OBJS= \
	"$(INTDIR)\reschedule.obj" \
	"..\src\condor_c++_util\condor_cpp_util.lib" \
	"..\src\condor_classad\condor_classad.lib" \
	"..\src\condor_io\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_reschedule.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_reschedule - Win32 Debug"

OUTDIR=.\..\src\condor_tools
INTDIR=.\..\src\condor_tools
# Begin Custom Macros
OutDir=.\..\src\condor_tools
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_reschedule.exe" "$(OUTDIR)\condor_reschedule.bsc"

!ELSE 

ALL : "condor_classad - Win32 Debug" "condor_io - Win32 Debug"\
 "condor_util_lib - Win32 Debug" "condor_cpp_util - Win32 Debug"\
 "$(OUTDIR)\condor_reschedule.exe" "$(OUTDIR)\condor_reschedule.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_cpp_util - Win32 DebugCLEAN"\
 "condor_util_lib - Win32 DebugCLEAN" "condor_io - Win32 DebugCLEAN"\
 "condor_classad - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\reschedule.obj"
	-@erase "$(INTDIR)\reschedule.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_reschedule.bsc"
	-@erase "$(OUTDIR)\condor_reschedule.exe"
	-@erase "$(OUTDIR)\condor_reschedule.ilk"
	-@erase "$(OUTDIR)\condor_reschedule.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS"\
 /FR"$(INTDIR)\\" /Fp"..\src\condor_c++_util/condor_common.pch"\
 /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_tools/
CPP_SBRS=..\src\condor_tools/
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_reschedule.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\reschedule.sbr"

"$(OUTDIR)\condor_reschedule.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ws2_32.lib /nologo /subsystem:console /incremental:yes\
 /pdb:"$(OUTDIR)\condor_reschedule.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\condor_reschedule.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\reschedule.obj" \
	"..\src\condor_c++_util\condor_cpp_util.lib" \
	"..\src\condor_classad\condor_classad.lib" \
	"..\src\condor_io\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_reschedule.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(CFG)" == "condor_reschedule - Win32 Release" || "$(CFG)" ==\
 "condor_reschedule - Win32 Debug"

!IF  "$(CFG)" == "condor_reschedule - Win32 Release"

"condor_cpp_util - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak\
 CFG="condor_cpp_util - Win32 Release" 
   cd "."

"condor_cpp_util - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_cpp_util.mak\
 CFG="condor_cpp_util - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_reschedule - Win32 Debug"

"condor_cpp_util - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak\
 CFG="condor_cpp_util - Win32 Debug" 
   cd "."

"condor_cpp_util - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_cpp_util.mak\
 CFG="condor_cpp_util - Win32 Debug" RECURSE=1 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_reschedule - Win32 Release"

"condor_util_lib - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak\
 CFG="condor_util_lib - Win32 Release" 
   cd "."

"condor_util_lib - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_util_lib.mak\
 CFG="condor_util_lib - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_reschedule - Win32 Debug"

"condor_util_lib - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak\
 CFG="condor_util_lib - Win32 Debug" 
   cd "."

"condor_util_lib - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_util_lib.mak\
 CFG="condor_util_lib - Win32 Debug" RECURSE=1 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_reschedule - Win32 Release"

"condor_io - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Release" 
   cd "."

"condor_io - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_io.mak\
 CFG="condor_io - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_reschedule - Win32 Debug"

"condor_io - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Debug" 
   cd "."

"condor_io - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_io.mak CFG="condor_io - Win32 Debug"\
 RECURSE=1 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_reschedule - Win32 Release"

"condor_classad - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_classad.mak\
 CFG="condor_classad - Win32 Release" 
   cd "."

"condor_classad - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_classad.mak\
 CFG="condor_classad - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_reschedule - Win32 Debug"

"condor_classad - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_classad.mak\
 CFG="condor_classad - Win32 Debug" 
   cd "."

"condor_classad - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_classad.mak\
 CFG="condor_classad - Win32 Debug" RECURSE=1 
   cd "."

!ENDIF 

SOURCE=..\src\condor_tools\reschedule.C

!IF  "$(CFG)" == "condor_reschedule - Win32 Release"


"$(INTDIR)\reschedule.obj" : $(SOURCE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_reschedule - Win32 Debug"


"$(INTDIR)\reschedule.obj"	"$(INTDIR)\reschedule.sbr" : $(SOURCE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

