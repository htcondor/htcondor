# Microsoft Developer Studio Generated NMAKE File, Based on condor_shadow.dsp
!IF "$(CFG)" == ""
CFG=condor_shadow - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_shadow - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_shadow - Win32 Release" && "$(CFG)" !=\
 "condor_shadow - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_shadow.mak" CFG="condor_shadow - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_shadow - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "condor_shadow - Win32 Debug" (based on\
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

!IF  "$(CFG)" == "condor_shadow - Win32 Release"

OUTDIR=.\..\src\condor_shadow.V6.1
INTDIR=.\..\src\condor_shadow.V6.1
# Begin Custom Macros
OutDir=.\..\src\condor_shadow.V6.1
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_shadow.exe"

!ELSE 

ALL : "$(OUTDIR)\condor_shadow.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\NTreceivers.obj"
	-@erase "$(INTDIR)\pseudo_ops.obj"
	-@erase "$(INTDIR)\shadow.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_shadow.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_shadow.V6.1/
CPP_SBRS=.
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_shadow.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib pdh.lib ws2_32.lib condor_c++_util\condor_common.obj /nologo\
 /subsystem:console /incremental:no /pdb:"$(OUTDIR)\condor_shadow.pdb"\
 /machine:I386 /out:"$(OUTDIR)\condor_shadow.exe" 
LINK32_OBJS= \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\NTreceivers.obj" \
	"$(INTDIR)\pseudo_ops.obj" \
	"$(INTDIR)\shadow.obj"

"$(OUTDIR)\condor_shadow.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_shadow - Win32 Debug"

OUTDIR=.\..\src\condor_shadow.V6.1
INTDIR=.\..\src\condor_shadow.V6.1
# Begin Custom Macros
OutDir=.\..\src\condor_shadow.V6.1
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_shadow.exe"

!ELSE 

ALL : "$(OUTDIR)\condor_shadow.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\NTreceivers.obj"
	-@erase "$(INTDIR)\pseudo_ops.obj"
	-@erase "$(INTDIR)\shadow.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_shadow.exe"
	-@erase "$(OUTDIR)\condor_shadow.ilk"
	-@erase "$(OUTDIR)\condor_shadow.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_shadow.V6.1/
CPP_SBRS=.
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_shadow.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib pdh.lib ws2_32.lib ../src/condor_c++_util/condor_common.obj\
 ../src/condor_util_lib/condor_common.obj /nologo /subsystem:console\
 /incremental:yes /pdb:"$(OUTDIR)\condor_shadow.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\condor_shadow.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\NTreceivers.obj" \
	"$(INTDIR)\pseudo_ops.obj" \
	"$(INTDIR)\shadow.obj"

"$(OUTDIR)\condor_shadow.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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


!IF "$(CFG)" == "condor_shadow - Win32 Release" || "$(CFG)" ==\
 "condor_shadow - Win32 Debug"
SOURCE=..\src\condor_shadow.V6.1\main.C
DEP_CPP_MAIN_=\
	"..\src\condor_c++_util\condor_event.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_c++_util\user_log.c++.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_shadow.V6.1\shadow.h"\
	"..\src\h\exit.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

"$(INTDIR)\main.obj" : $(SOURCE) $(DEP_CPP_MAIN_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_shadow.V6.1\NTreceivers.C
DEP_CPP_NTREC=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_shadow.V6.1\pseudo_ops.h"\
	"..\src\h\condor_sys.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	{$(INCLUDE)}"syscall_numbers.h"\
	

"$(INTDIR)\NTreceivers.obj" : $(SOURCE) $(DEP_CPP_NTREC) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_shadow.V6.1\pseudo_ops.C
DEP_CPP_PSEUD=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\condor_event.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_c++_util\user_log.c++.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_shadow.V6.1\pseudo_ops.h"\
	"..\src\condor_shadow.V6.1\shadow.h"\
	"..\src\h\exit.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_ast.h"\
	{$(INCLUDE)}"condor_astbase.h"\
	{$(INCLUDE)}"condor_attrlist.h"\
	{$(INCLUDE)}"condor_classad.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_exprtype.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

"$(INTDIR)\pseudo_ops.obj" : $(SOURCE) $(DEP_CPP_PSEUD) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_shadow.V6.1\shadow.C
DEP_CPP_SHADO=\
	"..\src\condor_c++_util\afs.h"\
	"..\src\condor_c++_util\condor_event.h"\
	"..\src\condor_c++_util\HashTable.h"\
	"..\src\condor_c++_util\list.h"\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_c++_util\user_log.c++.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_shadow.V6.1\shadow.h"\
	"..\src\h\exit.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
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
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

"$(INTDIR)\shadow.obj" : $(SOURCE) $(DEP_CPP_SHADO) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

