# Microsoft Developer Studio Generated NMAKE File, Based on condor_master.dsp
!IF "$(CFG)" == ""
CFG=condor_master - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_master - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_master - Win32 Release" && "$(CFG)" !=\
 "condor_master - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_master.mak" CFG="condor_master - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_master - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "condor_master - Win32 Debug" (based on\
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

!IF  "$(CFG)" == "condor_master - Win32 Release"

OUTDIR=.\..\src\condor_master.V6
INTDIR=.\..\src\condor_master.V6
# Begin Custom Macros
OutDir=.\..\src\condor_master.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_master.exe"

!ELSE 

ALL : "condor_io - Win32 Release" "condor_daemon_core - Win32 Release"\
 "condor_classad - Win32 Release" "condor_cpp_util - Win32 Release"\
 "condor_util_lib - Win32 Release" "$(OUTDIR)\condor_master.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_util_lib - Win32 ReleaseCLEAN"\
 "condor_cpp_util - Win32 ReleaseCLEAN" "condor_classad - Win32 ReleaseCLEAN"\
 "condor_daemon_core - Win32 ReleaseCLEAN" "condor_io - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\daemon.obj"
	-@erase "$(INTDIR)\master.obj"
	-@erase "$(INTDIR)\service.WIN32.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_master.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_master.V6/
CPP_SBRS=.
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_master.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ws2_32.lib /nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)\condor_master.pdb" /machine:I386\
 /out:"$(OUTDIR)\condor_master.exe" 
LINK32_OBJS= \
	"$(INTDIR)\daemon.obj" \
	"$(INTDIR)\master.obj" \
	"$(INTDIR)\service.WIN32.obj" \
	"..\src\condor_c++_util\condor_cpp_util.lib" \
	"..\src\condor_classad\condor_classad.lib" \
	"..\src\condor_daemon_core.V6\condor_daemon_core.lib" \
	"..\src\condor_io\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_master.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_master - Win32 Debug"

OUTDIR=.\..\src\condor_master.V6
INTDIR=.\..\src\condor_master.V6
# Begin Custom Macros
OutDir=.\..\src\condor_master.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_master.exe"

!ELSE 

ALL : "condor_io - Win32 Debug" "condor_daemon_core - Win32 Debug"\
 "condor_classad - Win32 Debug" "condor_cpp_util - Win32 Debug"\
 "condor_util_lib - Win32 Debug" "$(OUTDIR)\condor_master.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_util_lib - Win32 DebugCLEAN"\
 "condor_cpp_util - Win32 DebugCLEAN" "condor_classad - Win32 DebugCLEAN"\
 "condor_daemon_core - Win32 DebugCLEAN" "condor_io - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\daemon.obj"
	-@erase "$(INTDIR)\master.obj"
	-@erase "$(INTDIR)\service.WIN32.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_master.exe"
	-@erase "$(OUTDIR)\condor_master.ilk"
	-@erase "$(OUTDIR)\condor_master.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_master.V6/
CPP_SBRS=.
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_master.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ws2_32.lib ../src/condor_c++_util/condor_common.obj\
 ..\src\condor_util_lib/condor_common.obj /nologo /subsystem:console\
 /incremental:yes /pdb:"$(OUTDIR)\condor_master.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\condor_master.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\daemon.obj" \
	"$(INTDIR)\master.obj" \
	"$(INTDIR)\service.WIN32.obj" \
	"..\src\condor_c++_util\condor_cpp_util.lib" \
	"..\src\condor_classad\condor_classad.lib" \
	"..\src\condor_daemon_core.V6\condor_daemon_core.lib" \
	"..\src\condor_io\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib"

"$(OUTDIR)\condor_master.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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


!IF "$(CFG)" == "condor_master - Win32 Release" || "$(CFG)" ==\
 "condor_master - Win32 Debug"

!IF  "$(CFG)" == "condor_master - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_master - Win32 Debug"

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

!IF  "$(CFG)" == "condor_master - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_master - Win32 Debug"

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

!IF  "$(CFG)" == "condor_master - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_master - Win32 Debug"

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

!IF  "$(CFG)" == "condor_master - Win32 Release"

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

!ELSEIF  "$(CFG)" == "condor_master - Win32 Debug"

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

!IF  "$(CFG)" == "condor_master - Win32 Release"

"condor_io - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Release" 
   cd "."

"condor_io - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\condor_io.mak\
 CFG="condor_io - Win32 Release" RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "condor_master - Win32 Debug"

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

SOURCE=..\src\condor_master.V6\daemon.C
DEP_CPP_DAEMO=\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_ast.h"\
	"..\src\condor_includes\condor_astbase.h"\
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
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\condor_master.V6\master.h"\
	"..\src\h\exit.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\daemon.obj" : $(SOURCE) $(DEP_CPP_DAEMO) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_master.V6\master.C
DEP_CPP_MASTE=\
	"..\src\condor_c++_util\daemon_types.h"\
	"..\src\condor_c++_util\get_daemon_addr.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\my_hostname.h"\
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
	"..\src\condor_includes\condor_collector.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_exprtype.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\condor_master.V6\master.h"\
	"..\src\h\cctp.h"\
	"..\src\h\cctp_msg.h"\
	"..\src\h\exit.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\master.obj" : $(SOURCE) $(DEP_CPP_MASTE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_master.V6\service.WIN32.C
DEP_CPP_SERVI=\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
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
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\exit.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\service.WIN32.obj" : $(SOURCE) $(DEP_CPP_SERVI) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

