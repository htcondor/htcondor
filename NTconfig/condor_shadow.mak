# Microsoft Developer Studio Generated NMAKE File, Based on condor_shadow.dsp
!IF "$(CFG)" == ""
CFG=condor_shadow - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_shadow - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_shadow - Win32 Debug" && "$(CFG)" != "condor_shadow - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_shadow.mak" CFG="condor_shadow - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_shadow - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "condor_shadow - Win32 Release" (based on "Win32 (x86) Console Application")
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

!IF  "$(CFG)" == "condor_shadow - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_shadow.exe"

!ELSE 

ALL : "condor_sysapi - Win32 Debug" "condor_qmgmt - Win32 Debug" "condor_util_lib - Win32 Debug" "condor_io - Win32 Debug" "condor_daemon_core - Win32 Debug" "condor_cpp_util - Win32 Debug" "condor_classad - Win32 Debug" "$(OUTDIR)\condor_shadow.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_classad - Win32 DebugCLEAN" "condor_cpp_util - Win32 DebugCLEAN" "condor_daemon_core - Win32 DebugCLEAN" "condor_io - Win32 DebugCLEAN" "condor_util_lib - Win32 DebugCLEAN" "condor_qmgmt - Win32 DebugCLEAN" "condor_sysapi - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\baseshadow.obj"
	-@erase "$(INTDIR)\mpiresource.obj"
	-@erase "$(INTDIR)\mpishadow.obj"
	-@erase "$(INTDIR)\NTreceivers.obj"
	-@erase "$(INTDIR)\pseudo_ops.obj"
	-@erase "$(INTDIR)\remoteresource.obj"
	-@erase "$(INTDIR)\shadow.obj"
	-@erase "$(INTDIR)\shadow_user_policy.obj"
	-@erase "$(INTDIR)\shadow_v61_main.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_shadow.exe"
	-@erase "$(OUTDIR)\condor_shadow.ilk"
	-@erase "$(OUTDIR)\condor_shadow.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_shadow.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=../Debug/condor_common.obj ..\Debug\condor_common_c.obj kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib pdh.lib ws2_32.lib mswsock.lib netapi32.lib imagehlp.lib Crypt32.lib mpr.lib psapi.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\condor_shadow.pdb" /debug /machine:I386 /out:"$(OUTDIR)\condor_shadow.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\baseshadow.obj" \
	"$(INTDIR)\mpiresource.obj" \
	"$(INTDIR)\mpishadow.obj" \
	"$(INTDIR)\NTreceivers.obj" \
	"$(INTDIR)\pseudo_ops.obj" \
	"$(INTDIR)\remoteresource.obj" \
	"$(INTDIR)\shadow.obj" \
	"$(INTDIR)\shadow_user_policy.obj" \
	"$(INTDIR)\shadow_v61_main.obj" \
	"$(OUTDIR)\condor_classad.lib" \
	"$(OUTDIR)\condor_cpp_util.lib" \
	"$(OUTDIR)\condor_daemon_core.lib" \
	"$(OUTDIR)\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib" \
	"$(OUTDIR)\condor_qmgmt.lib" \
	"$(OUTDIR)\condor_sysapi.lib"

"$(OUTDIR)\condor_shadow.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_shadow - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_shadow.exe"

!ELSE 

ALL : "condor_sysapi - Win32 Release" "condor_qmgmt - Win32 Release" "condor_util_lib - Win32 Release" "condor_io - Win32 Release" "condor_daemon_core - Win32 Release" "condor_cpp_util - Win32 Release" "condor_classad - Win32 Release" "$(OUTDIR)\condor_shadow.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_classad - Win32 ReleaseCLEAN" "condor_cpp_util - Win32 ReleaseCLEAN" "condor_daemon_core - Win32 ReleaseCLEAN" "condor_io - Win32 ReleaseCLEAN" "condor_util_lib - Win32 ReleaseCLEAN" "condor_qmgmt - Win32 ReleaseCLEAN" "condor_sysapi - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\baseshadow.obj"
	-@erase "$(INTDIR)\mpiresource.obj"
	-@erase "$(INTDIR)\mpishadow.obj"
	-@erase "$(INTDIR)\NTreceivers.obj"
	-@erase "$(INTDIR)\pseudo_ops.obj"
	-@erase "$(INTDIR)\remoteresource.obj"
	-@erase "$(INTDIR)\shadow.obj"
	-@erase "$(INTDIR)\shadow_user_policy.obj"
	-@erase "$(INTDIR)\shadow_v61_main.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_shadow.exe"
	-@erase "$(OUTDIR)\condor_shadow.map"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_shadow.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=../Release/condor_common.obj ../Release/condor_common_c.obj kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib pdh.lib ws2_32.lib mswsock.lib netapi32.lib imagehlp.lib Crypt32.lib mpr.lib psapi.lib /nologo /subsystem:console /pdb:none /map:"$(INTDIR)\condor_shadow.map" /debug /machine:I386 /out:"$(OUTDIR)\condor_shadow.exe" 
LINK32_OBJS= \
	"$(INTDIR)\baseshadow.obj" \
	"$(INTDIR)\mpiresource.obj" \
	"$(INTDIR)\mpishadow.obj" \
	"$(INTDIR)\NTreceivers.obj" \
	"$(INTDIR)\pseudo_ops.obj" \
	"$(INTDIR)\remoteresource.obj" \
	"$(INTDIR)\shadow.obj" \
	"$(INTDIR)\shadow_user_policy.obj" \
	"$(INTDIR)\shadow_v61_main.obj" \
	"$(OUTDIR)\condor_classad.lib" \
	"$(OUTDIR)\condor_cpp_util.lib" \
	"$(OUTDIR)\condor_daemon_core.lib" \
	"$(OUTDIR)\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib" \
	"$(OUTDIR)\condor_qmgmt.lib" \
	"$(OUTDIR)\condor_sysapi.lib"

"$(OUTDIR)\condor_shadow.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

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


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_shadow.dep")
!INCLUDE "condor_shadow.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_shadow.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_shadow - Win32 Debug" || "$(CFG)" == "condor_shadow - Win32 Release"

!IF  "$(CFG)" == "condor_shadow - Win32 Debug"

"condor_classad - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_classad.mak CFG="condor_classad - Win32 Debug" 
   cd "."

"condor_classad - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_classad.mak CFG="condor_classad - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_shadow - Win32 Release"

"condor_classad - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_classad.mak CFG="condor_classad - Win32 Release" 
   cd "."

"condor_classad - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_classad.mak CFG="condor_classad - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_shadow - Win32 Debug"

"condor_cpp_util - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Debug" 
   cd "."

"condor_cpp_util - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_shadow - Win32 Release"

"condor_cpp_util - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Release" 
   cd "."

"condor_cpp_util - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_shadow - Win32 Debug"

"condor_daemon_core - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_daemon_core.mak CFG="condor_daemon_core - Win32 Debug" 
   cd "."

"condor_daemon_core - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_daemon_core.mak CFG="condor_daemon_core - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_shadow - Win32 Release"

"condor_daemon_core - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_daemon_core.mak CFG="condor_daemon_core - Win32 Release" 
   cd "."

"condor_daemon_core - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_daemon_core.mak CFG="condor_daemon_core - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_shadow - Win32 Debug"

"condor_io - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Debug" 
   cd "."

"condor_io - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_shadow - Win32 Release"

"condor_io - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Release" 
   cd "."

"condor_io - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_shadow - Win32 Debug"

"condor_util_lib - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Debug" 
   cd "."

"condor_util_lib - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_shadow - Win32 Release"

"condor_util_lib - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Release" 
   cd "."

"condor_util_lib - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_shadow - Win32 Debug"

"condor_qmgmt - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_qmgmt.mak CFG="condor_qmgmt - Win32 Debug" 
   cd "."

"condor_qmgmt - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_qmgmt.mak CFG="condor_qmgmt - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_shadow - Win32 Release"

"condor_qmgmt - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_qmgmt.mak CFG="condor_qmgmt - Win32 Release" 
   cd "."

"condor_qmgmt - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_qmgmt.mak CFG="condor_qmgmt - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_shadow - Win32 Debug"

"condor_sysapi - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_sysapi.mak CFG="condor_sysapi - Win32 Debug" 
   cd "."

"condor_sysapi - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_sysapi.mak CFG="condor_sysapi - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_shadow - Win32 Release"

"condor_sysapi - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_sysapi.mak CFG="condor_sysapi - Win32 Release" 
   cd "."

"condor_sysapi - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_sysapi.mak CFG="condor_sysapi - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

SOURCE=..\src\condor_shadow.V6.1\baseshadow.C

"$(INTDIR)\baseshadow.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_shadow.V6.1\mpiresource.C

"$(INTDIR)\mpiresource.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_shadow.V6.1\mpishadow.C

"$(INTDIR)\mpishadow.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_shadow.V6.1\NTreceivers.C

"$(INTDIR)\NTreceivers.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_shadow.V6.1\pseudo_ops.C

"$(INTDIR)\pseudo_ops.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_shadow.V6.1\remoteresource.C

"$(INTDIR)\remoteresource.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_shadow.V6.1\shadow.C

"$(INTDIR)\shadow.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_shadow.V6.1\shadow_user_policy.C

"$(INTDIR)\shadow_user_policy.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_shadow.V6.1\shadow_v61_main.C

"$(INTDIR)\shadow_v61_main.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

