# Microsoft Developer Studio Generated NMAKE File, Based on condor_collector.dsp
!IF "$(CFG)" == ""
CFG=condor_collector - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_collector - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_collector - Win32 Release" && "$(CFG)" !=\
 "condor_collector - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_collector.mak" CFG="condor_collector - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_collector - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "condor_collector - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_collector - Win32 Release"

OUTDIR=.\../src/condor_collector.V6
INTDIR=.\../src/condor_collector.V6
# Begin Custom Macros
OutDir=.\../src/condor_collector.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_collector.exe"

!ELSE 

ALL : "condor_io - Win32 Release" "condor_daemon_core - Win32 Release"\
 "condor_classad - Win32 Release" "condor_cpp_util - Win32 Release"\
 "condor_util_lib - Win32 Release" "$(OUTDIR)\condor_collector.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_util_lib - Win32 ReleaseCLEAN"\
 "condor_cpp_util - Win32 ReleaseCLEAN" "condor_classad - Win32 ReleaseCLEAN"\
 "condor_daemon_core - Win32 ReleaseCLEAN" "condor_io - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\collector.obj"
	-@erase "$(INTDIR)\collector_engine.obj"
	-@erase "$(INTDIR)\hashkey.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\totals.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\view_server.obj"
	-@erase "$(OUTDIR)\condor_collector.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=../src/condor_collector.V6/
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_collector.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ws2_32.lib /nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)\condor_collector.pdb" /machine:I386\
 /out:"$(OUTDIR)\condor_collector.exe" 
LINK32_OBJS= \
	"$(INTDIR)\collector.obj" \
	"$(INTDIR)\collector_engine.obj" \
	"$(INTDIR)\hashkey.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\totals.obj" \
	"$(INTDIR)\view_server.obj" \
	"..\src\condor_c++_util\condor_cpp_util.lib" \
	"..\src\condor_classad\condor_classad.lib" \
	"..\src\condor_daemon_core.V6\condor_daemon_core.lib" \
	"..\src\condor_io\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_collector.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"

OUTDIR=.\../src/condor_collector.V6
INTDIR=.\../src/condor_collector.V6
# Begin Custom Macros
OutDir=.\../src/condor_collector.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_collector.exe" "$(OUTDIR)\condor_collector.bsc"

!ELSE 

ALL : "condor_io - Win32 Debug" "condor_daemon_core - Win32 Debug"\
 "condor_classad - Win32 Debug" "condor_cpp_util - Win32 Debug"\
 "condor_util_lib - Win32 Debug" "$(OUTDIR)\condor_collector.exe"\
 "$(OUTDIR)\condor_collector.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_util_lib - Win32 DebugCLEAN"\
 "condor_cpp_util - Win32 DebugCLEAN" "condor_classad - Win32 DebugCLEAN"\
 "condor_daemon_core - Win32 DebugCLEAN" "condor_io - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\collector.obj"
	-@erase "$(INTDIR)\collector.sbr"
	-@erase "$(INTDIR)\collector_engine.obj"
	-@erase "$(INTDIR)\collector_engine.sbr"
	-@erase "$(INTDIR)\hashkey.obj"
	-@erase "$(INTDIR)\hashkey.sbr"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\main.sbr"
	-@erase "$(INTDIR)\totals.obj"
	-@erase "$(INTDIR)\totals.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\view_server.obj"
	-@erase "$(INTDIR)\view_server.sbr"
	-@erase "$(OUTDIR)\condor_collector.bsc"
	-@erase "$(OUTDIR)\condor_collector.exe"
	-@erase "$(OUTDIR)\condor_collector.ilk"
	-@erase "$(OUTDIR)\condor_collector.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS"\
 /FR"$(INTDIR)\\" /Fp"..\src\condor_c++_util/condor_common.pch"\
 /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=../src/condor_collector.V6/
CPP_SBRS=../src/condor_collector.V6/

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_collector.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\collector.sbr" \
	"$(INTDIR)\collector_engine.sbr" \
	"$(INTDIR)\hashkey.sbr" \
	"$(INTDIR)\main.sbr" \
	"$(INTDIR)\totals.sbr" \
	"$(INTDIR)\view_server.sbr"

"$(OUTDIR)\condor_collector.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ws2_32.lib ../src/condor_c++_util/condor_common.obj\
 ..\src\condor_util_lib/condor_common.obj /nologo /subsystem:console\
 /incremental:yes /pdb:"$(OUTDIR)\condor_collector.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\condor_collector.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\collector.obj" \
	"$(INTDIR)\collector_engine.obj" \
	"$(INTDIR)\hashkey.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\totals.obj" \
	"$(INTDIR)\view_server.obj" \
	"..\src\condor_c++_util\condor_cpp_util.lib" \
	"..\src\condor_classad\condor_classad.lib" \
	"..\src\condor_daemon_core.V6\condor_daemon_core.lib" \
	"..\src\condor_io\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_collector.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "condor_collector - Win32 Release" || "$(CFG)" ==\
 "condor_collector - Win32 Debug"

!IF  "$(CFG)" == "condor_collector - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"

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

!IF  "$(CFG)" == "condor_collector - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"

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

!IF  "$(CFG)" == "condor_collector - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"

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

!IF  "$(CFG)" == "condor_collector - Win32 Release"

"condor_daemon_core - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_daemon_core.mak\
 CFG="condor_daemon_core - Win32 Release" 
   cd "."

"condor_daemon_core - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_daemon_core.mak\
 CFG="condor_daemon_core - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"

"condor_daemon_core - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_daemon_core.mak\
 CFG="condor_daemon_core - Win32 Debug" 
   cd "."

"condor_daemon_core - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_daemon_core.mak\
 CFG="condor_daemon_core - Win32 Debug" RECURSE=1 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_collector - Win32 Release"

"condor_io - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Release" 
   cd "."

"condor_io - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_io.mak\
 CFG="condor_io - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"

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

SOURCE=..\src\condor_collector.V6\collector.C
DEP_CPP_COLLE=\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\MyString.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_collector.V6\collector.h"\
	"..\src\condor_collector.V6\collector_engine.h"\
	"..\src\condor_collector.V6\hashkey.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_parser.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\fdprintf.h"\
	"..\src\condor_status.V6\status_types.h"\
	"..\src\condor_status.V6\totals.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"auth_sock.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_expressions.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_fix_string.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_hpux_64bit_types.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_dux.h"\
	{$(INCLUDE)}"condor_sys_hpux.h"\
	{$(INCLUDE)}"condor_sys_irix.h"\
	{$(INCLUDE)}"condor_sys_linux.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_sys_solaris.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"expr.h"\
	{$(INCLUDE)}"fake_flock.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_COLLE=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_collector - Win32 Release"


"$(INTDIR)\collector.obj" : $(SOURCE) $(DEP_CPP_COLLE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"


"$(INTDIR)\collector.obj"	"$(INTDIR)\collector.sbr" : $(SOURCE)\
 $(DEP_CPP_COLLE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_collector.V6\collector_engine.C
DEP_CPP_COLLEC=\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_c++_util\url_condor.h"\
	"..\src\condor_collector.V6\collector_engine.h"\
	"..\src\condor_collector.V6\hashkey.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_parser.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\fdprintf.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"auth_sock.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_expressions.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_fix_string.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_hpux_64bit_types.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_dux.h"\
	{$(INCLUDE)}"condor_sys_hpux.h"\
	{$(INCLUDE)}"condor_sys_irix.h"\
	{$(INCLUDE)}"condor_sys_linux.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_sys_solaris.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"expr.h"\
	{$(INCLUDE)}"fake_flock.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_COLLEC=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_collector - Win32 Release"


"$(INTDIR)\collector_engine.obj" : $(SOURCE) $(DEP_CPP_COLLEC) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"


"$(INTDIR)\collector_engine.obj"	"$(INTDIR)\collector_engine.sbr" : $(SOURCE)\
 $(DEP_CPP_COLLEC) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_collector.V6\hashkey.C
DEP_CPP_HASHK=\
	"..\src\condor_collector.V6\hashkey.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_fix_string.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_hpux_64bit_types.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_dux.h"\
	{$(INCLUDE)}"condor_sys_hpux.h"\
	{$(INCLUDE)}"condor_sys_irix.h"\
	{$(INCLUDE)}"condor_sys_linux.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_sys_solaris.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"stream.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

!IF  "$(CFG)" == "condor_collector - Win32 Release"


"$(INTDIR)\hashkey.obj" : $(SOURCE) $(DEP_CPP_HASHK) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"


"$(INTDIR)\hashkey.obj"	"$(INTDIR)\hashkey.sbr" : $(SOURCE) $(DEP_CPP_HASHK)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_collector.V6\main.C
DEP_CPP_MAIN_=\
	"..\src\condor_c++_util\MyString.h"\
	"..\src\condor_c++_util\Set.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_collector.V6\collector.h"\
	"..\src\condor_collector.V6\collector_engine.h"\
	"..\src\condor_collector.V6\hashkey.h"\
	"..\src\condor_collector.V6\view_server.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_status.V6\status_types.h"\
	"..\src\condor_status.V6\totals.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"auth_sock.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_expressions.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_fix_string.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_hpux_64bit_types.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_dux.h"\
	{$(INCLUDE)}"condor_sys_hpux.h"\
	{$(INCLUDE)}"condor_sys_irix.h"\
	{$(INCLUDE)}"condor_sys_linux.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_sys_solaris.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_MAIN_=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_collector - Win32 Release"


"$(INTDIR)\main.obj" : $(SOURCE) $(DEP_CPP_MAIN_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"


"$(INTDIR)\main.obj"	"$(INTDIR)\main.sbr" : $(SOURCE) $(DEP_CPP_MAIN_)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_status.V6\totals.C
DEP_CPP_TOTAL=\
	"..\src\condor_c++_util\MyString.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_status.V6\status_types.h"\
	"..\src\condor_status.V6\totals.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_fix_string.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_hpux_64bit_types.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_dux.h"\
	{$(INCLUDE)}"condor_sys_hpux.h"\
	{$(INCLUDE)}"condor_sys_irix.h"\
	{$(INCLUDE)}"condor_sys_linux.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_sys_solaris.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"stream.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

!IF  "$(CFG)" == "condor_collector - Win32 Release"


"$(INTDIR)\totals.obj" : $(SOURCE) $(DEP_CPP_TOTAL) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"


"$(INTDIR)\totals.obj"	"$(INTDIR)\totals.sbr" : $(SOURCE) $(DEP_CPP_TOTAL)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_collector.V6\view_server.C
DEP_CPP_VIEW_=\
	"..\src\condor_c++_util\MyString.h"\
	"..\src\condor_c++_util\Set.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_collector.V6\collector.h"\
	"..\src\condor_collector.V6\collector_engine.h"\
	"..\src\condor_collector.V6\hashkey.h"\
	"..\src\condor_collector.V6\view_server.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_status.V6\status_types.h"\
	"..\src\condor_status.V6\totals.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"auth_sock.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_expressions.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_fix_string.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_hpux_64bit_types.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_dux.h"\
	{$(INCLUDE)}"condor_sys_hpux.h"\
	{$(INCLUDE)}"condor_sys_irix.h"\
	{$(INCLUDE)}"condor_sys_linux.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_sys_solaris.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	{$(INCLUDE)}"HashTable.h"\
	{$(INCLUDE)}"list.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_VIEW_=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_collector - Win32 Release"


"$(INTDIR)\view_server.obj" : $(SOURCE) $(DEP_CPP_VIEW_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_collector - Win32 Debug"


"$(INTDIR)\view_server.obj"	"$(INTDIR)\view_server.sbr" : $(SOURCE)\
 $(DEP_CPP_VIEW_) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

