# Microsoft Developer Studio Generated NMAKE File, Based on condor_schedd.dsp
!IF "$(CFG)" == ""
CFG=condor_schedd - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_schedd - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_schedd - Win32 Release" && "$(CFG)" !=\
 "condor_schedd - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_schedd.mak" CFG="condor_schedd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_schedd - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "condor_schedd - Win32 Debug" (based on\
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

!IF  "$(CFG)" == "condor_schedd - Win32 Release"

OUTDIR=.\../src/condor_schedd.V6
INTDIR=.\../src/condor_schedd.V6
# Begin Custom Macros
OutDir=.\../src/condor_schedd.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_schedd.exe"

!ELSE 

ALL : "condor_ckpt_server_api - Win32 Release" "condor_io - Win32 Release"\
 "condor_daemon_core - Win32 Release" "condor_classad - Win32 Release"\
 "condor_cpp_util - Win32 Release" "condor_util_lib - Win32 Release"\
 "$(OUTDIR)\condor_schedd.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_util_lib - Win32 ReleaseCLEAN"\
 "condor_cpp_util - Win32 ReleaseCLEAN" "condor_classad - Win32 ReleaseCLEAN"\
 "condor_daemon_core - Win32 ReleaseCLEAN" "condor_io - Win32 ReleaseCLEAN"\
 "condor_ckpt_server_api - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\qmgmt.obj"
	-@erase "$(INTDIR)\qmgmt_common.obj"
	-@erase "$(INTDIR)\qmgmt_receivers.obj"
	-@erase "$(INTDIR)\schedd.obj"
	-@erase "$(INTDIR)\schedd_main.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_schedd.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=../src/condor_schedd.V6/
CPP_SBRS=.
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_schedd.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ws2_32.lib /nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)\condor_schedd.pdb" /machine:I386\
 /out:"$(OUTDIR)\condor_schedd.exe" 
LINK32_OBJS= \
	"$(INTDIR)\qmgmt.obj" \
	"$(INTDIR)\qmgmt_common.obj" \
	"$(INTDIR)\qmgmt_receivers.obj" \
	"$(INTDIR)\schedd.obj" \
	"$(INTDIR)\schedd_main.obj" \
	"..\src\condor_c++_util\condor_cpp_util.lib" \
	"..\src\condor_ckpt_server\condor_ckpt_server_api.lib" \
	"..\src\condor_classad\condor_classad.lib" \
	"..\src\condor_daemon_core.V6\condor_daemon_core.lib" \
	"..\src\condor_io\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_schedd.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_schedd - Win32 Debug"

OUTDIR=.\../src/condor_schedd.V6
INTDIR=.\../src/condor_schedd.V6
# Begin Custom Macros
OutDir=.\../src/condor_schedd.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_schedd.exe"

!ELSE 

ALL : "condor_ckpt_server_api - Win32 Debug" "condor_io - Win32 Debug"\
 "condor_daemon_core - Win32 Debug" "condor_classad - Win32 Debug"\
 "condor_cpp_util - Win32 Debug" "condor_util_lib - Win32 Debug"\
 "$(OUTDIR)\condor_schedd.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_util_lib - Win32 DebugCLEAN"\
 "condor_cpp_util - Win32 DebugCLEAN" "condor_classad - Win32 DebugCLEAN"\
 "condor_daemon_core - Win32 DebugCLEAN" "condor_io - Win32 DebugCLEAN"\
 "condor_ckpt_server_api - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\qmgmt.obj"
	-@erase "$(INTDIR)\qmgmt_common.obj"
	-@erase "$(INTDIR)\qmgmt_receivers.obj"
	-@erase "$(INTDIR)\schedd.obj"
	-@erase "$(INTDIR)\schedd_main.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_schedd.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=../src/condor_schedd.V6/
CPP_SBRS=.
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_schedd.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ws2_32.lib ../src/condor_c++_util/condor_common.obj\
 ..\src\condor_util_lib/condor_common.obj /nologo /subsystem:console\
 /incremental:yes /pdb:"$(OUTDIR)\condor_schedd.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\condor_schedd.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\qmgmt.obj" \
	"$(INTDIR)\qmgmt_common.obj" \
	"$(INTDIR)\qmgmt_receivers.obj" \
	"$(INTDIR)\schedd.obj" \
	"$(INTDIR)\schedd_main.obj" \
	"..\src\condor_c++_util\condor_cpp_util.lib" \
	"..\src\condor_ckpt_server\condor_ckpt_server_api.lib" \
	"..\src\condor_classad\condor_classad.lib" \
	"..\src\condor_daemon_core.V6\condor_daemon_core.lib" \
	"..\src\condor_io\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_schedd.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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


!IF "$(CFG)" == "condor_schedd - Win32 Release" || "$(CFG)" ==\
 "condor_schedd - Win32 Debug"

!IF  "$(CFG)" == "condor_schedd - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_schedd - Win32 Debug"

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

!IF  "$(CFG)" == "condor_schedd - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_schedd - Win32 Debug"

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

!IF  "$(CFG)" == "condor_schedd - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_schedd - Win32 Debug"

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

!IF  "$(CFG)" == "condor_schedd - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_schedd - Win32 Debug"

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

!IF  "$(CFG)" == "condor_schedd - Win32 Release"

"condor_io - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Release" 
   cd "."

"condor_io - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_io.mak\
 CFG="condor_io - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_schedd - Win32 Debug"

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

!IF  "$(CFG)" == "condor_schedd - Win32 Release"

"condor_ckpt_server_api - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_ckpt_server_api.mak\
 CFG="condor_ckpt_server_api - Win32 Release" 
   cd "."

"condor_ckpt_server_api - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_ckpt_server_api.mak\
 CFG="condor_ckpt_server_api - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_schedd - Win32 Debug"

"condor_ckpt_server_api - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_ckpt_server_api.mak\
 CFG="condor_ckpt_server_api - Win32 Debug" 
   cd "."

"condor_ckpt_server_api - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_ckpt_server_api.mak\
 CFG="condor_ckpt_server_api - Win32 Debug" RECURSE=1 
   cd "."

!ENDIF 

SOURCE=..\src\condor_schedd.V6\qmgmt.C
DEP_CPP_QMGMT=\
	"..\src\condor_c++_util\classad_collection.h"\
	"..\src\condor_c++_util\classad_collection_types.h"\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\classad_log.h"\
	"..\src\condor_c++_util\condor_updown.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\log.h"\
	"..\src\condor_c++_util\log_transaction.h"\
	"..\src\condor_c++_util\mystring.h"\
	"..\src\condor_c++_util\set.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\condor_schedd.V6\prio_rec.h"\
	"..\src\condor_schedd.V6\qmgmt.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\qmgmt.obj" : $(SOURCE) $(DEP_CPP_QMGMT) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_schedd.V6\qmgmt_common.C
DEP_CPP_QMGMT_=\
	"..\src\condor_c++_util\mystring.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\h\proc.h"\
	

"$(INTDIR)\qmgmt_common.obj" : $(SOURCE) $(DEP_CPP_QMGMT_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_schedd.V6\qmgmt_receivers.C
DEP_CPP_QMGMT_R=\
	"..\src\condor_c++_util\mystring.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\condor_schedd.V6\qmgmt_constants.h"\
	"..\src\condor_syscall_lib\syscall_param_sizes.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\qmgmt_receivers.obj" : $(SOURCE) $(DEP_CPP_QMGMT_R) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_schedd.V6\schedd.C
DEP_CPP_SCHED=\
	"..\src\condor_c++_util\access.h"\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\condor_event.h"\
	"..\src\condor_c++_util\condor_updown.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\environ.h"\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\get_daemon_addr.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\mystring.h"\
	"..\src\condor_c++_util\ntsysinfo.h"\
	"..\src\condor_c++_util\Queue.h"\
	"..\src\condor_c++_util\renice_self.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_c++_util\user_log.c++.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\condor_schedd.V6\prio_rec.h"\
	"..\src\condor_schedd.V6\scheduler.h"\
	"..\src\h\exit.h"\
	"..\src\h\expr.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\schedd.obj" : $(SOURCE) $(DEP_CPP_SCHED) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_schedd.V6\schedd_main.C
DEP_CPP_SCHEDD=\
	"..\src\condor_c++_util\classad_hashtable.h"\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\extArray.h"\
	"..\src\condor_c++_util\get_daemon_addr.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\mystring.h"\
	"..\src\condor_c++_util\ntsysinfo.h"\
	"..\src\condor_c++_util\Queue.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_adtypes.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_attrlist.h"\
	"..\src\condor_includes\condor_classad.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\condor_schedd.V6\prio_rec.h"\
	"..\src\condor_schedd.V6\scheduler.h"\
	"..\src\h\clib.h"\
	"..\src\h\exit.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\schedd_main.obj" : $(SOURCE) $(DEP_CPP_SCHEDD) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

