# Microsoft Developer Studio Generated NMAKE File, Based on condor_startd.dsp
!IF "$(CFG)" == ""
CFG=condor_startd - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_startd - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_startd - Win32 Release" && "$(CFG)" !=\
 "condor_startd - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_startd.mak" CFG="condor_startd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_startd - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "condor_startd - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_startd - Win32 Release"

OUTDIR=.\..\src\condor_startd.V6
INTDIR=.\..\src\condor_startd.V6
# Begin Custom Macros
OutDir=.\..\src\condor_startd.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_startd.exe"

!ELSE 

ALL : "condor_startd_lib - Win32 Release" "condor_daemon_core - Win32 Release"\
 "condor_io - Win32 Release" "condor_classad - Win32 Release"\
 "condor_cpp_util - Win32 Release" "condor_util_lib - Win32 Release"\
 "$(OUTDIR)\condor_startd.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_util_lib - Win32 ReleaseCLEAN"\
 "condor_cpp_util - Win32 ReleaseCLEAN" "condor_classad - Win32 ReleaseCLEAN"\
 "condor_io - Win32 ReleaseCLEAN" "condor_daemon_core - Win32 ReleaseCLEAN"\
 "condor_startd_lib - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\calc.obj"
	-@erase "$(INTDIR)\command.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\Match.obj"
	-@erase "$(INTDIR)\Reqexp.obj"
	-@erase "$(INTDIR)\ResAttributes.obj"
	-@erase "$(INTDIR)\ResMgr.obj"
	-@erase "$(INTDIR)\Resource.obj"
	-@erase "$(INTDIR)\ResState.obj"
	-@erase "$(INTDIR)\Starter.obj"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_startd.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_startd.V6/
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_startd.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib pdh.lib ws2_32.lib condor_c++_util\condor_common.obj /nologo\
 /subsystem:console /incremental:no /pdb:"$(OUTDIR)\condor_startd.pdb"\
 /machine:I386 /out:"$(OUTDIR)\condor_startd.exe" 
LINK32_OBJS= \
	"$(INTDIR)\calc.obj" \
	"$(INTDIR)\command.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\Match.obj" \
	"$(INTDIR)\Reqexp.obj" \
	"$(INTDIR)\ResAttributes.obj" \
	"$(INTDIR)\ResMgr.obj" \
	"$(INTDIR)\Resource.obj" \
	"$(INTDIR)\ResState.obj" \
	"$(INTDIR)\Starter.obj" \
	"$(INTDIR)\util.obj" \
	"$(OUTDIR)\condor_startd_lib.lib" \
	"..\..\..\..\projects\kbdd\Release\kbdd.lib" \
	"..\src\condor_c++_util\condor_cpp_util.lib" \
	"..\src\condor_classad\condor_classad.lib" \
	"..\src\condor_daemon_core.V6\condor_daemon_core.lib" \
	"..\src\condor_io\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_startd.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"

OUTDIR=.\..\src\condor_startd.V6
INTDIR=.\..\src\condor_startd.V6
# Begin Custom Macros
OutDir=.\..\src\condor_startd.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_startd.exe" "$(OUTDIR)\condor_startd.bsc"

!ELSE 

ALL : "condor_startd_lib - Win32 Debug" "condor_daemon_core - Win32 Debug"\
 "condor_io - Win32 Debug" "condor_classad - Win32 Debug"\
 "condor_cpp_util - Win32 Debug" "condor_util_lib - Win32 Debug"\
 "$(OUTDIR)\condor_startd.exe" "$(OUTDIR)\condor_startd.bsc"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_util_lib - Win32 DebugCLEAN"\
 "condor_cpp_util - Win32 DebugCLEAN" "condor_classad - Win32 DebugCLEAN"\
 "condor_io - Win32 DebugCLEAN" "condor_daemon_core - Win32 DebugCLEAN"\
 "condor_startd_lib - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\calc.obj"
	-@erase "$(INTDIR)\calc.sbr"
	-@erase "$(INTDIR)\command.obj"
	-@erase "$(INTDIR)\command.sbr"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\main.sbr"
	-@erase "$(INTDIR)\Match.obj"
	-@erase "$(INTDIR)\Match.sbr"
	-@erase "$(INTDIR)\Reqexp.obj"
	-@erase "$(INTDIR)\Reqexp.sbr"
	-@erase "$(INTDIR)\ResAttributes.obj"
	-@erase "$(INTDIR)\ResAttributes.sbr"
	-@erase "$(INTDIR)\ResMgr.obj"
	-@erase "$(INTDIR)\ResMgr.sbr"
	-@erase "$(INTDIR)\Resource.obj"
	-@erase "$(INTDIR)\Resource.sbr"
	-@erase "$(INTDIR)\ResState.obj"
	-@erase "$(INTDIR)\ResState.sbr"
	-@erase "$(INTDIR)\Starter.obj"
	-@erase "$(INTDIR)\Starter.sbr"
	-@erase "$(INTDIR)\util.obj"
	-@erase "$(INTDIR)\util.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_startd.bsc"
	-@erase "$(OUTDIR)\condor_startd.exe"
	-@erase "$(OUTDIR)\condor_startd.ilk"
	-@erase "$(OUTDIR)\condor_startd.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS"\
 /FR"$(INTDIR)\\" /Fp"..\src\condor_c++_util/condor_common.pch"\
 /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_startd.V6/
CPP_SBRS=..\src\condor_startd.V6/

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_startd.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\calc.sbr" \
	"$(INTDIR)\command.sbr" \
	"$(INTDIR)\main.sbr" \
	"$(INTDIR)\Match.sbr" \
	"$(INTDIR)\Reqexp.sbr" \
	"$(INTDIR)\ResAttributes.sbr" \
	"$(INTDIR)\ResMgr.sbr" \
	"$(INTDIR)\Resource.sbr" \
	"$(INTDIR)\ResState.sbr" \
	"$(INTDIR)\Starter.sbr" \
	"$(INTDIR)\util.sbr"

"$(OUTDIR)\condor_startd.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib pdh.lib ws2_32.lib ../src/condor_c++_util/condor_common.obj\
 ../src/condor_util_lib/condor_common.obj /nologo /subsystem:console\
 /incremental:yes /pdb:"$(OUTDIR)\condor_startd.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\condor_startd.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\calc.obj" \
	"$(INTDIR)\command.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\Match.obj" \
	"$(INTDIR)\Reqexp.obj" \
	"$(INTDIR)\ResAttributes.obj" \
	"$(INTDIR)\ResMgr.obj" \
	"$(INTDIR)\Resource.obj" \
	"$(INTDIR)\ResState.obj" \
	"$(INTDIR)\Starter.obj" \
	"$(INTDIR)\util.obj" \
	"$(OUTDIR)\condor_startd_lib.lib" \
	"..\..\..\..\projects\kbdd\Release\kbdd.lib" \
	"..\src\condor_c++_util\condor_cpp_util.lib" \
	"..\src\condor_classad\condor_classad.lib" \
	"..\src\condor_daemon_core.V6\condor_daemon_core.lib" \
	"..\src\condor_io\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_startd.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "condor_startd - Win32 Release" || "$(CFG)" ==\
 "condor_startd - Win32 Debug"

!IF  "$(CFG)" == "condor_startd - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"

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

!IF  "$(CFG)" == "condor_startd - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"

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

!IF  "$(CFG)" == "condor_startd - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"

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

!IF  "$(CFG)" == "condor_startd - Win32 Release"

"condor_io - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Release" 
   cd "."

"condor_io - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_io.mak\
 CFG="condor_io - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"

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

!IF  "$(CFG)" == "condor_startd - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"

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

!IF  "$(CFG)" == "condor_startd - Win32 Release"

"condor_startd_lib - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_startd_lib.mak\
 CFG="condor_startd_lib - Win32 Release" 
   cd "."

"condor_startd_lib - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_startd_lib.mak\
 CFG="condor_startd_lib - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"

"condor_startd_lib - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_startd_lib.mak\
 CFG="condor_startd_lib - Win32 Debug" 
   cd "."

"condor_startd_lib - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_startd_lib.mak\
 CFG="condor_startd_lib - Win32 Debug" RECURSE=1 
   cd "."

!ENDIF 

SOURCE=..\src\condor_startd.V6\calc.C
DEP_CPP_CALC_=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\directory.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_startd.V6\calc.h"\
	"..\src\condor_startd.V6\command.h"\
	"..\src\condor_startd.V6\Match.h"\
	"..\src\condor_startd.V6\Reqexp.h"\
	"..\src\condor_startd.V6\ResAttributes.h"\
	"..\src\condor_startd.V6\ResMgr.h"\
	"..\src\condor_startd.V6\Resource.h"\
	"..\src\condor_startd.V6\ResState.h"\
	"..\src\condor_startd.V6\startd.h"\
	"..\src\condor_startd.V6\Starter.h"\
	"..\src\condor_startd.V6\util.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
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
	
NODEP_CPP_CALC_=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_startd - Win32 Release"


"$(INTDIR)\calc.obj" : $(SOURCE) $(DEP_CPP_CALC_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"


"$(INTDIR)\calc.obj"	"$(INTDIR)\calc.sbr" : $(SOURCE) $(DEP_CPP_CALC_)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_startd.V6\command.C
DEP_CPP_COMMA=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_startd.V6\calc.h"\
	"..\src\condor_startd.V6\command.h"\
	"..\src\condor_startd.V6\Match.h"\
	"..\src\condor_startd.V6\Reqexp.h"\
	"..\src\condor_startd.V6\ResAttributes.h"\
	"..\src\condor_startd.V6\ResMgr.h"\
	"..\src\condor_startd.V6\Resource.h"\
	"..\src\condor_startd.V6\ResState.h"\
	"..\src\condor_startd.V6\startd.h"\
	"..\src\condor_startd.V6\Starter.h"\
	"..\src\condor_startd.V6\util.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
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
	
NODEP_CPP_COMMA=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_startd - Win32 Release"


"$(INTDIR)\command.obj" : $(SOURCE) $(DEP_CPP_COMMA) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"


"$(INTDIR)\command.obj"	"$(INTDIR)\command.sbr" : $(SOURCE) $(DEP_CPP_COMMA)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_startd.V6\main.C
DEP_CPP_MAIN_=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_startd.V6\calc.h"\
	"..\src\condor_startd.V6\command.h"\
	"..\src\condor_startd.V6\Match.h"\
	"..\src\condor_startd.V6\Reqexp.h"\
	"..\src\condor_startd.V6\ResAttributes.h"\
	"..\src\condor_startd.V6\ResMgr.h"\
	"..\src\condor_startd.V6\Resource.h"\
	"..\src\condor_startd.V6\ResState.h"\
	"..\src\condor_startd.V6\startd.h"\
	"..\src\condor_startd.V6\Starter.h"\
	"..\src\condor_startd.V6\util.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
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
	
NODEP_CPP_MAIN_=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_startd - Win32 Release"


"$(INTDIR)\main.obj" : $(SOURCE) $(DEP_CPP_MAIN_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"


"$(INTDIR)\main.obj"	"$(INTDIR)\main.sbr" : $(SOURCE) $(DEP_CPP_MAIN_)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_startd.V6\Match.C
DEP_CPP_MATCH=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_startd.V6\calc.h"\
	"..\src\condor_startd.V6\command.h"\
	"..\src\condor_startd.V6\Match.h"\
	"..\src\condor_startd.V6\Reqexp.h"\
	"..\src\condor_startd.V6\ResAttributes.h"\
	"..\src\condor_startd.V6\ResMgr.h"\
	"..\src\condor_startd.V6\Resource.h"\
	"..\src\condor_startd.V6\ResState.h"\
	"..\src\condor_startd.V6\startd.h"\
	"..\src\condor_startd.V6\Starter.h"\
	"..\src\condor_startd.V6\util.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
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
	
NODEP_CPP_MATCH=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_startd - Win32 Release"


"$(INTDIR)\Match.obj" : $(SOURCE) $(DEP_CPP_MATCH) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"


"$(INTDIR)\Match.obj"	"$(INTDIR)\Match.sbr" : $(SOURCE) $(DEP_CPP_MATCH)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_startd.V6\Reqexp.C
DEP_CPP_REQEX=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_startd.V6\calc.h"\
	"..\src\condor_startd.V6\command.h"\
	"..\src\condor_startd.V6\Match.h"\
	"..\src\condor_startd.V6\Reqexp.h"\
	"..\src\condor_startd.V6\ResAttributes.h"\
	"..\src\condor_startd.V6\ResMgr.h"\
	"..\src\condor_startd.V6\Resource.h"\
	"..\src\condor_startd.V6\ResState.h"\
	"..\src\condor_startd.V6\startd.h"\
	"..\src\condor_startd.V6\Starter.h"\
	"..\src\condor_startd.V6\util.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
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
	
NODEP_CPP_REQEX=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_startd - Win32 Release"


"$(INTDIR)\Reqexp.obj" : $(SOURCE) $(DEP_CPP_REQEX) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"


"$(INTDIR)\Reqexp.obj"	"$(INTDIR)\Reqexp.sbr" : $(SOURCE) $(DEP_CPP_REQEX)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_startd.V6\ResAttributes.C
DEP_CPP_RESAT=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_startd.V6\calc.h"\
	"..\src\condor_startd.V6\command.h"\
	"..\src\condor_startd.V6\Match.h"\
	"..\src\condor_startd.V6\Reqexp.h"\
	"..\src\condor_startd.V6\ResAttributes.h"\
	"..\src\condor_startd.V6\ResMgr.h"\
	"..\src\condor_startd.V6\Resource.h"\
	"..\src\condor_startd.V6\ResState.h"\
	"..\src\condor_startd.V6\startd.h"\
	"..\src\condor_startd.V6\Starter.h"\
	"..\src\condor_startd.V6\util.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
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
	
NODEP_CPP_RESAT=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_startd - Win32 Release"


"$(INTDIR)\ResAttributes.obj" : $(SOURCE) $(DEP_CPP_RESAT) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"


"$(INTDIR)\ResAttributes.obj"	"$(INTDIR)\ResAttributes.sbr" : $(SOURCE)\
 $(DEP_CPP_RESAT) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_startd.V6\ResMgr.C
DEP_CPP_RESMG=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_startd.V6\calc.h"\
	"..\src\condor_startd.V6\command.h"\
	"..\src\condor_startd.V6\Match.h"\
	"..\src\condor_startd.V6\Reqexp.h"\
	"..\src\condor_startd.V6\ResAttributes.h"\
	"..\src\condor_startd.V6\ResMgr.h"\
	"..\src\condor_startd.V6\Resource.h"\
	"..\src\condor_startd.V6\ResState.h"\
	"..\src\condor_startd.V6\startd.h"\
	"..\src\condor_startd.V6\Starter.h"\
	"..\src\condor_startd.V6\util.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
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
	
NODEP_CPP_RESMG=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_startd - Win32 Release"


"$(INTDIR)\ResMgr.obj" : $(SOURCE) $(DEP_CPP_RESMG) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"


"$(INTDIR)\ResMgr.obj"	"$(INTDIR)\ResMgr.sbr" : $(SOURCE) $(DEP_CPP_RESMG)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_startd.V6\Resource.C
DEP_CPP_RESOU=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_startd.V6\calc.h"\
	"..\src\condor_startd.V6\command.h"\
	"..\src\condor_startd.V6\Match.h"\
	"..\src\condor_startd.V6\Reqexp.h"\
	"..\src\condor_startd.V6\ResAttributes.h"\
	"..\src\condor_startd.V6\ResMgr.h"\
	"..\src\condor_startd.V6\Resource.h"\
	"..\src\condor_startd.V6\ResState.h"\
	"..\src\condor_startd.V6\startd.h"\
	"..\src\condor_startd.V6\Starter.h"\
	"..\src\condor_startd.V6\util.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
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
	
NODEP_CPP_RESOU=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_startd - Win32 Release"


"$(INTDIR)\Resource.obj" : $(SOURCE) $(DEP_CPP_RESOU) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"


"$(INTDIR)\Resource.obj"	"$(INTDIR)\Resource.sbr" : $(SOURCE) $(DEP_CPP_RESOU)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_startd.V6\ResState.C
DEP_CPP_RESST=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_startd.V6\calc.h"\
	"..\src\condor_startd.V6\command.h"\
	"..\src\condor_startd.V6\Match.h"\
	"..\src\condor_startd.V6\Reqexp.h"\
	"..\src\condor_startd.V6\ResAttributes.h"\
	"..\src\condor_startd.V6\ResMgr.h"\
	"..\src\condor_startd.V6\Resource.h"\
	"..\src\condor_startd.V6\ResState.h"\
	"..\src\condor_startd.V6\startd.h"\
	"..\src\condor_startd.V6\Starter.h"\
	"..\src\condor_startd.V6\util.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
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
	
NODEP_CPP_RESST=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_startd - Win32 Release"


"$(INTDIR)\ResState.obj" : $(SOURCE) $(DEP_CPP_RESST) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"


"$(INTDIR)\ResState.obj"	"$(INTDIR)\ResState.sbr" : $(SOURCE) $(DEP_CPP_RESST)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_startd.V6\Starter.C
DEP_CPP_START=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_startd.V6\calc.h"\
	"..\src\condor_startd.V6\command.h"\
	"..\src\condor_startd.V6\Match.h"\
	"..\src\condor_startd.V6\Reqexp.h"\
	"..\src\condor_startd.V6\ResAttributes.h"\
	"..\src\condor_startd.V6\ResMgr.h"\
	"..\src\condor_startd.V6\Resource.h"\
	"..\src\condor_startd.V6\ResState.h"\
	"..\src\condor_startd.V6\startd.h"\
	"..\src\condor_startd.V6\Starter.h"\
	"..\src\condor_startd.V6\util.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
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
	
NODEP_CPP_START=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_startd - Win32 Release"


"$(INTDIR)\Starter.obj" : $(SOURCE) $(DEP_CPP_START) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"


"$(INTDIR)\Starter.obj"	"$(INTDIR)\Starter.sbr" : $(SOURCE) $(DEP_CPP_START)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_startd.V6\util.C
DEP_CPP_UTIL_=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_state.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_startd.V6\calc.h"\
	"..\src\condor_startd.V6\command.h"\
	"..\src\condor_startd.V6\Match.h"\
	"..\src\condor_startd.V6\Reqexp.h"\
	"..\src\condor_startd.V6\ResAttributes.h"\
	"..\src\condor_startd.V6\ResMgr.h"\
	"..\src\condor_startd.V6\Resource.h"\
	"..\src\condor_startd.V6\ResState.h"\
	"..\src\condor_startd.V6\startd.h"\
	"..\src\condor_startd.V6\Starter.h"\
	"..\src\condor_startd.V6\util.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\sig_install.h"\
	"..\src\h\startup.h"\
	"..\src\h\util_lib_proto.h"\
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
	
NODEP_CPP_UTIL_=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_startd - Win32 Release"


"$(INTDIR)\util.obj" : $(SOURCE) $(DEP_CPP_UTIL_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"


"$(INTDIR)\util.obj"	"$(INTDIR)\util.sbr" : $(SOURCE) $(DEP_CPP_UTIL_)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

