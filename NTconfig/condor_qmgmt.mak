# Microsoft Developer Studio Generated NMAKE File, Based on condor_qmgmt.dsp
!IF "$(CFG)" == ""
CFG=condor_qmgmt - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_qmgmt - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_qmgmt - Win32 Release" && "$(CFG)" !=\
 "condor_qmgmt - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_qmgmt.mak" CFG="condor_qmgmt - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_qmgmt - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_qmgmt - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_qmgmt - Win32 Release"

OUTDIR=.\../src/condor_schedd.V6
INTDIR=.\../src/condor_schedd.V6
# Begin Custom Macros
OutDir=.\../src/condor_schedd.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_qmgmt.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_qmgmt.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\qmgmt_common.obj"
	-@erase "$(INTDIR)\qmgmt_send_stubs.obj"
	-@erase "$(INTDIR)\qmgr_lib_support.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_qmgmt.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=../src/condor_schedd.V6/
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

BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_qmgmt.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_qmgmt.lib" 
LIB32_OBJS= \
	"$(INTDIR)\qmgmt_common.obj" \
	"$(INTDIR)\qmgmt_send_stubs.obj" \
	"$(INTDIR)\qmgr_lib_support.obj"

"$(OUTDIR)\condor_qmgmt.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_qmgmt - Win32 Debug"

OUTDIR=.\../src/condor_schedd.V6
INTDIR=.\../src/condor_schedd.V6
# Begin Custom Macros
OutDir=.\../src/condor_schedd.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_qmgmt.lib" "$(OUTDIR)\condor_qmgmt.bsc"

!ELSE 

ALL : "$(OUTDIR)\condor_qmgmt.lib" "$(OUTDIR)\condor_qmgmt.bsc"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\qmgmt_common.obj"
	-@erase "$(INTDIR)\qmgmt_common.sbr"
	-@erase "$(INTDIR)\qmgmt_send_stubs.obj"
	-@erase "$(INTDIR)\qmgmt_send_stubs.sbr"
	-@erase "$(INTDIR)\qmgr_lib_support.obj"
	-@erase "$(INTDIR)\qmgr_lib_support.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_qmgmt.bsc"
	-@erase "$(OUTDIR)\condor_qmgmt.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR"$(INTDIR)\\" /Fp"..\src\condor_c++_util/condor_common.pch"\
 /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=../src/condor_schedd.V6/
CPP_SBRS=../src/condor_schedd.V6/

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

BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_qmgmt.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\qmgmt_common.sbr" \
	"$(INTDIR)\qmgmt_send_stubs.sbr" \
	"$(INTDIR)\qmgr_lib_support.sbr"

"$(OUTDIR)\condor_qmgmt.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_qmgmt.lib" 
LIB32_OBJS= \
	"$(INTDIR)\qmgmt_common.obj" \
	"$(INTDIR)\qmgmt_send_stubs.obj" \
	"$(INTDIR)\qmgr_lib_support.obj"

"$(OUTDIR)\condor_qmgmt.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "condor_qmgmt - Win32 Release" || "$(CFG)" ==\
 "condor_qmgmt - Win32 Debug"
SOURCE=..\src\condor_schedd.V6\qmgmt_common.C
DEP_CPP_QMGMT=\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
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
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	

!IF  "$(CFG)" == "condor_qmgmt - Win32 Release"


"$(INTDIR)\qmgmt_common.obj" : $(SOURCE) $(DEP_CPP_QMGMT) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_qmgmt - Win32 Debug"


"$(INTDIR)\qmgmt_common.obj"	"$(INTDIR)\qmgmt_common.sbr" : $(SOURCE)\
 $(DEP_CPP_QMGMT) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_schedd.V6\qmgmt_send_stubs.C
DEP_CPP_QMGMT_=\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_schedd.V6\qmgmt_constants.h"\
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
	
NODEP_CPP_QMGMT_=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_qmgmt - Win32 Release"


"$(INTDIR)\qmgmt_send_stubs.obj" : $(SOURCE) $(DEP_CPP_QMGMT_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_qmgmt - Win32 Debug"


"$(INTDIR)\qmgmt_send_stubs.obj"	"$(INTDIR)\qmgmt_send_stubs.sbr" : $(SOURCE)\
 $(DEP_CPP_QMGMT_) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_schedd.V6\qmgr_lib_support.C
DEP_CPP_QMGR_=\
	"..\src\condor_c++_util\get_daemon_addr.h"\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_includes\condor_attributes.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\condor_qmgr.h"\
	"..\src\condor_schedd.V6\qmgr.h"\
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
	{$(INCLUDE)}"daemon_types.h"\
	{$(INCLUDE)}"fake_flock.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_QMGR_=\
	"..\src\condor_includes\globus_gss_assist.h"\
	

!IF  "$(CFG)" == "condor_qmgmt - Win32 Release"


"$(INTDIR)\qmgr_lib_support.obj" : $(SOURCE) $(DEP_CPP_QMGR_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_qmgmt - Win32 Debug"


"$(INTDIR)\qmgr_lib_support.obj"	"$(INTDIR)\qmgr_lib_support.sbr" : $(SOURCE)\
 $(DEP_CPP_QMGR_) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

