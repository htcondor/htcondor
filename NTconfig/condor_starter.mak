# Microsoft Developer Studio Generated NMAKE File, Based on condor_starter.dsp
!IF "$(CFG)" == ""
CFG=condor_starter - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_starter - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_starter - Win32 Release" && "$(CFG)" !=\
 "condor_starter - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_starter.mak" CFG="condor_starter - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_starter - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "condor_starter - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_starter - Win32 Release"

OUTDIR=.\..\src\condor_starter.V6.1
INTDIR=.\..\src\condor_starter.V6.1
# Begin Custom Macros
OutDir=.\..\src\condor_starter.V6.1
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_starter.exe"

!ELSE 

ALL : "$(OUTDIR)\condor_starter.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\NTsenders.obj"
	-@erase "$(INTDIR)\resource_limits.WIN32.obj"
	-@erase "$(INTDIR)\starter.obj"
	-@erase "$(INTDIR)\user_proc.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_starter.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_starter.V6.1/
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_starter.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib pdh.lib ws2_32.lib condor_c++_util\condor_common.obj /nologo\
 /subsystem:console /incremental:no /pdb:"$(OUTDIR)\condor_starter.pdb"\
 /machine:I386 /out:"$(OUTDIR)\condor_starter.exe" 
LINK32_OBJS= \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\NTsenders.obj" \
	"$(INTDIR)\resource_limits.WIN32.obj" \
	"$(INTDIR)\starter.obj" \
	"$(INTDIR)\user_proc.obj"

"$(OUTDIR)\condor_starter.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_starter - Win32 Debug"

OUTDIR=.\..\src\condor_starter.V6.1
INTDIR=.\..\src\condor_starter.V6.1
# Begin Custom Macros
OutDir=.\..\src\condor_starter.V6.1
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_starter.exe" "$(OUTDIR)\condor_starter.bsc"

!ELSE 

ALL : "$(OUTDIR)\condor_starter.exe" "$(OUTDIR)\condor_starter.bsc"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\main.sbr"
	-@erase "$(INTDIR)\NTsenders.obj"
	-@erase "$(INTDIR)\NTsenders.sbr"
	-@erase "$(INTDIR)\resource_limits.WIN32.obj"
	-@erase "$(INTDIR)\resource_limits.WIN32.sbr"
	-@erase "$(INTDIR)\starter.obj"
	-@erase "$(INTDIR)\starter.sbr"
	-@erase "$(INTDIR)\user_proc.obj"
	-@erase "$(INTDIR)\user_proc.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_starter.bsc"
	-@erase "$(OUTDIR)\condor_starter.exe"
	-@erase "$(OUTDIR)\condor_starter.ilk"
	-@erase "$(OUTDIR)\condor_starter.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS"\
 /FR"$(INTDIR)\\" /Fp"..\src\condor_c++_util/condor_common.pch"\
 /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_starter.V6.1/
CPP_SBRS=..\src\condor_starter.V6.1/

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_starter.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\main.sbr" \
	"$(INTDIR)\NTsenders.sbr" \
	"$(INTDIR)\resource_limits.WIN32.sbr" \
	"$(INTDIR)\starter.sbr" \
	"$(INTDIR)\user_proc.sbr"

"$(OUTDIR)\condor_starter.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib pdh.lib ws2_32.lib ../src/condor_c++_util/condor_common.obj\
 ../src/condor_util_lib/condor_common.obj /nologo /subsystem:console\
 /incremental:yes /pdb:"$(OUTDIR)\condor_starter.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)\condor_starter.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\NTsenders.obj" \
	"$(INTDIR)\resource_limits.WIN32.obj" \
	"$(INTDIR)\starter.obj" \
	"$(INTDIR)\user_proc.obj"

"$(OUTDIR)\condor_starter.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "condor_starter - Win32 Release" || "$(CFG)" ==\
 "condor_starter - Win32 Debug"
SOURCE=..\src\condor_starter.V6.1\main.C
DEP_CPP_MAIN_=\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_starter.V6.1\starter.h"\
	"..\src\condor_starter.V6.1\user_proc.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"auth_sock.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
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
	

!IF  "$(CFG)" == "condor_starter - Win32 Release"


"$(INTDIR)\main.obj" : $(SOURCE) $(DEP_CPP_MAIN_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_starter - Win32 Debug"


"$(INTDIR)\main.obj"	"$(INTDIR)\main.sbr" : $(SOURCE) $(DEP_CPP_MAIN_)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_starter.V6.1\NTsenders.C
DEP_CPP_NTSEN=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\condor_sys.h"\
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
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	{$(INCLUDE)}"syscall.aix.h"\
	
NODEP_CPP_NTSEN=\
	"..\src\condor_includes\globus_gss_assist.h"\
	"..\src\h\syscall_numbers.h"\
	

!IF  "$(CFG)" == "condor_starter - Win32 Release"


"$(INTDIR)\NTsenders.obj" : $(SOURCE) $(DEP_CPP_NTSEN) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_starter - Win32 Debug"


"$(INTDIR)\NTsenders.obj"	"$(INTDIR)\NTsenders.sbr" : $(SOURCE)\
 $(DEP_CPP_NTSEN) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_starter.V6.1\resource_limits.WIN32.C

!IF  "$(CFG)" == "condor_starter - Win32 Release"


"$(INTDIR)\resource_limits.WIN32.obj" : $(SOURCE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_starter - Win32 Debug"


"$(INTDIR)\resource_limits.WIN32.obj"	"$(INTDIR)\resource_limits.WIN32.sbr" : \
$(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_starter.V6.1\starter.C
DEP_CPP_START=\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_syscall_mode.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_starter.V6.1\starter.h"\
	"..\src\condor_starter.V6.1\user_proc.h"\
	"..\src\h\condor_sys.h"\
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
	{$(INCLUDE)}"syscall.aix.h"\
	
NODEP_CPP_START=\
	"..\src\condor_includes\globus_gss_assist.h"\
	"..\src\h\syscall_numbers.h"\
	

!IF  "$(CFG)" == "condor_starter - Win32 Release"


"$(INTDIR)\starter.obj" : $(SOURCE) $(DEP_CPP_START) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_starter - Win32 Debug"


"$(INTDIR)\starter.obj"	"$(INTDIR)\starter.sbr" : $(SOURCE) $(DEP_CPP_START)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_starter.V6.1\user_proc.C
DEP_CPP_USER_=\
	"..\src\condor_c++_util\string_list.h"\
	"..\src\condor_daemon_core.V6\condor_daemon_core.h"\
	"..\src\condor_daemon_core.V6\condor_ipverify.h"\
	"..\src\condor_daemon_core.V6\condor_timer_manager.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_syscall_mode.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\condor_starter.V6.1\starter.h"\
	"..\src\condor_starter.V6.1\user_proc.h"\
	"..\src\h\condor_sys.h"\
	"..\src\h\exit.h"\
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
	{$(INCLUDE)}"syscall.aix.h"\
	
NODEP_CPP_USER_=\
	"..\src\condor_includes\globus_gss_assist.h"\
	"..\src\h\syscall_numbers.h"\
	

!IF  "$(CFG)" == "condor_starter - Win32 Release"


"$(INTDIR)\user_proc.obj" : $(SOURCE) $(DEP_CPP_USER_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_starter - Win32 Debug"


"$(INTDIR)\user_proc.obj"	"$(INTDIR)\user_proc.sbr" : $(SOURCE)\
 $(DEP_CPP_USER_) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

