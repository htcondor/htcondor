# Microsoft Developer Studio Generated NMAKE File, Based on condor_procapi_test.dsp
!IF "$(CFG)" == ""
CFG=condor_procapi_test - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_procapi_test - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_procapi_test - Win32 Release" && "$(CFG)" !=\
 "condor_procapi_test - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_procapi_test.mak"\
 CFG="condor_procapi_test - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_procapi_test - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "condor_procapi_test - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_procapi_test - Win32 Release"

OUTDIR=.\..\src\condor_procapi
INTDIR=.\..\src\condor_procapi
# Begin Custom Macros
OutDir=.\..\src\condor_procapi
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_procapi_test.exe"

!ELSE 

ALL : "condor_procapi - Win32 Release" "$(OUTDIR)\condor_procapi_test.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_procapi - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\testprocapi.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_procapi_test.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_procapi/
CPP_SBRS=.

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_procapi_test.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib\
 comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib\
 odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)\condor_procapi_test.pdb" /machine:I386\
 /out:"$(OUTDIR)\condor_procapi_test.exe" 
LINK32_OBJS= \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\testprocapi.obj" \
	"$(OUTDIR)\condor_procapi.lib"

"$(OUTDIR)\condor_procapi_test.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_procapi_test - Win32 Debug"

OUTDIR=.\..\src\condor_procapi
INTDIR=.\..\src\condor_procapi
# Begin Custom Macros
OutDir=.\..\src\condor_procapi
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_procapi_test.exe"

!ELSE 

ALL : "condor_procapi - Win32 Debug" "$(OUTDIR)\condor_procapi_test.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_procapi - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\ntsysinfo.obj"
	-@erase "$(INTDIR)\testprocapi.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_procapi_test.exe"
	-@erase "$(OUTDIR)\condor_procapi_test.ilk"
	-@erase "$(OUTDIR)\condor_procapi_test.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_procapi/
CPP_SBRS=.

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_procapi_test.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ws2_32.lib netapi32.lib ../src/condor_c++_util/condor_common.obj\
 ..\src\condor_util_lib/condor_common.obj /nologo /subsystem:console\
 /incremental:yes /pdb:"$(OUTDIR)\condor_procapi_test.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\condor_procapi_test.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\ntsysinfo.obj" \
	"$(INTDIR)\testprocapi.obj" \
	"$(OUTDIR)\condor_procapi.lib"

"$(OUTDIR)\condor_procapi_test.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "condor_procapi_test - Win32 Release" || "$(CFG)" ==\
 "condor_procapi_test - Win32 Debug"

!IF  "$(CFG)" == "condor_procapi_test - Win32 Release"

"condor_procapi - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_procapi.mak\
 CFG="condor_procapi - Win32 Release" 
   cd "."

"condor_procapi - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_procapi.mak\
 CFG="condor_procapi - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_procapi_test - Win32 Debug"

"condor_procapi - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_procapi.mak\
 CFG="condor_procapi - Win32 Debug" 
   cd "."

"condor_procapi - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_procapi.mak\
 CFG="condor_procapi - Win32 Debug" RECURSE=1 
   cd "."

!ENDIF 

SOURCE="..\src\condor_c++_util\ntsysinfo.C"
DEP_CPP_NTSYS=\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\ntsysinfo.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_fix_string.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_hpux_64bit_types.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_dux.h"\
	"..\src\condor_includes\condor_sys_hpux.h"\
	"..\src\condor_includes\condor_sys_irix.h"\
	"..\src\condor_includes\condor_sys_linux.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_sys_solaris.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\condor_sysapi\sysapi.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_NTSYS=\
	"..\src\condor_includes\gssapi.h"\
	

"$(INTDIR)\ntsysinfo.obj" : $(SOURCE) $(DEP_CPP_NTSYS) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_procapi\testprocapi.C
DEP_CPP_TESTP=\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\ntsysinfo.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_fix_string.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_hpux_64bit_types.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_dux.h"\
	"..\src\condor_includes\condor_sys_hpux.h"\
	"..\src\condor_includes\condor_sys_irix.h"\
	"..\src\condor_includes\condor_sys_linux.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_sys_solaris.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_procapi\procapi.h"\
	"..\src\condor_sysapi\sysapi.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_TESTP=\
	"..\src\condor_includes\gssapi.h"\
	

"$(INTDIR)\testprocapi.obj" : $(SOURCE) $(DEP_CPP_TESTP) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

