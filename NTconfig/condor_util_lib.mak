# Microsoft Developer Studio Generated NMAKE File, Based on condor_util_lib.dsp
!IF "$(CFG)" == ""
CFG=condor_util_lib - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_util_lib - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_util_lib - Win32 Release" && "$(CFG)" !=\
 "condor_util_lib - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_util_lib.mak" CFG="condor_util_lib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_util_lib - Win32 Release" (based on\
 "Win32 (x86) Static Library")
!MESSAGE "condor_util_lib - Win32 Debug" (based on\
 "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

OUTDIR=.\..\src\condor_util_lib
INTDIR=.\..\src\condor_util_lib
# Begin Custom Macros
OutDir=.\..\src\condor_util_lib
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_util.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_util.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\basename.obj"
	-@erase "$(INTDIR)\blankline.obj"
	-@erase "$(INTDIR)\ckpt_name.obj"
	-@erase "$(INTDIR)\condor_common.obj"
	-@erase "$(INTDIR)\condor_common.pch"
	-@erase "$(INTDIR)\condor_errlst.obj"
	-@erase "$(INTDIR)\cronos.obj"
	-@erase "$(INTDIR)\dprintf.obj"
	-@erase "$(INTDIR)\dprintf_config.obj"
	-@erase "$(INTDIR)\email.obj"
	-@erase "$(INTDIR)\email_file.obj"
	-@erase "$(INTDIR)\escapes.obj"
	-@erase "$(INTDIR)\except.obj"
	-@erase "$(INTDIR)\fdprintf.obj"
	-@erase "$(INTDIR)\free_fs_blocks.obj"
	-@erase "$(INTDIR)\get_random_num.obj"
	-@erase "$(INTDIR)\internet.obj"
	-@erase "$(INTDIR)\lock_file.WIN32.obj"
	-@erase "$(INTDIR)\ltrunc.obj"
	-@erase "$(INTDIR)\match_prefix.obj"
	-@erase "$(INTDIR)\mkargv.obj"
	-@erase "$(INTDIR)\phys_mem.obj"
	-@erase "$(INTDIR)\rotate_file.obj"
	-@erase "$(INTDIR)\setsyscalls.obj"
	-@erase "$(INTDIR)\signames.obj"
	-@erase "$(INTDIR)\strcmp_until.obj"
	-@erase "$(INTDIR)\uids.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\virt_mem.obj"
	-@erase "$(OUTDIR)\condor_util.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=..\src\condor_util_lib/
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_util_lib.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_util.lib" 
LIB32_OBJS= \
	"$(INTDIR)\basename.obj" \
	"$(INTDIR)\blankline.obj" \
	"$(INTDIR)\ckpt_name.obj" \
	"$(INTDIR)\condor_common.obj" \
	"$(INTDIR)\condor_errlst.obj" \
	"$(INTDIR)\cronos.obj" \
	"$(INTDIR)\dprintf.obj" \
	"$(INTDIR)\dprintf_config.obj" \
	"$(INTDIR)\email.obj" \
	"$(INTDIR)\email_file.obj" \
	"$(INTDIR)\escapes.obj" \
	"$(INTDIR)\except.obj" \
	"$(INTDIR)\fdprintf.obj" \
	"$(INTDIR)\free_fs_blocks.obj" \
	"$(INTDIR)\get_random_num.obj" \
	"$(INTDIR)\internet.obj" \
	"$(INTDIR)\lock_file.WIN32.obj" \
	"$(INTDIR)\ltrunc.obj" \
	"$(INTDIR)\match_prefix.obj" \
	"$(INTDIR)\mkargv.obj" \
	"$(INTDIR)\phys_mem.obj" \
	"$(INTDIR)\rotate_file.obj" \
	"$(INTDIR)\setsyscalls.obj" \
	"$(INTDIR)\signames.obj" \
	"$(INTDIR)\strcmp_until.obj" \
	"$(INTDIR)\uids.obj" \
	"$(INTDIR)\virt_mem.obj"

"$(OUTDIR)\condor_util.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

OUTDIR=.\..\src\condor_util_lib
INTDIR=.\..\src\condor_util_lib
# Begin Custom Macros
OutDir=.\..\src\condor_util_lib
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_util.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_util.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\basename.obj"
	-@erase "$(INTDIR)\blankline.obj"
	-@erase "$(INTDIR)\ckpt_name.obj"
	-@erase "$(INTDIR)\condor_common.obj"
	-@erase "$(INTDIR)\condor_common.pch"
	-@erase "$(INTDIR)\condor_errlst.obj"
	-@erase "$(INTDIR)\cronos.obj"
	-@erase "$(INTDIR)\dprintf.obj"
	-@erase "$(INTDIR)\dprintf_config.obj"
	-@erase "$(INTDIR)\email.obj"
	-@erase "$(INTDIR)\email_file.obj"
	-@erase "$(INTDIR)\escapes.obj"
	-@erase "$(INTDIR)\except.obj"
	-@erase "$(INTDIR)\fdprintf.obj"
	-@erase "$(INTDIR)\free_fs_blocks.obj"
	-@erase "$(INTDIR)\get_random_num.obj"
	-@erase "$(INTDIR)\internet.obj"
	-@erase "$(INTDIR)\lock_file.WIN32.obj"
	-@erase "$(INTDIR)\ltrunc.obj"
	-@erase "$(INTDIR)\match_prefix.obj"
	-@erase "$(INTDIR)\mkargv.obj"
	-@erase "$(INTDIR)\phys_mem.obj"
	-@erase "$(INTDIR)\rotate_file.obj"
	-@erase "$(INTDIR)\setsyscalls.obj"
	-@erase "$(INTDIR)\signames.obj"
	-@erase "$(INTDIR)\strcmp_until.obj"
	-@erase "$(INTDIR)\uids.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\virt_mem.obj"
	-@erase "$(OUTDIR)\condor_util.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=..\src\condor_util_lib/
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_util_lib.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_util.lib" 
LIB32_OBJS= \
	"$(INTDIR)\basename.obj" \
	"$(INTDIR)\blankline.obj" \
	"$(INTDIR)\ckpt_name.obj" \
	"$(INTDIR)\condor_common.obj" \
	"$(INTDIR)\condor_errlst.obj" \
	"$(INTDIR)\cronos.obj" \
	"$(INTDIR)\dprintf.obj" \
	"$(INTDIR)\dprintf_config.obj" \
	"$(INTDIR)\email.obj" \
	"$(INTDIR)\email_file.obj" \
	"$(INTDIR)\escapes.obj" \
	"$(INTDIR)\except.obj" \
	"$(INTDIR)\fdprintf.obj" \
	"$(INTDIR)\free_fs_blocks.obj" \
	"$(INTDIR)\get_random_num.obj" \
	"$(INTDIR)\internet.obj" \
	"$(INTDIR)\lock_file.WIN32.obj" \
	"$(INTDIR)\ltrunc.obj" \
	"$(INTDIR)\match_prefix.obj" \
	"$(INTDIR)\mkargv.obj" \
	"$(INTDIR)\phys_mem.obj" \
	"$(INTDIR)\rotate_file.obj" \
	"$(INTDIR)\setsyscalls.obj" \
	"$(INTDIR)\signames.obj" \
	"$(INTDIR)\strcmp_until.obj" \
	"$(INTDIR)\uids.obj" \
	"$(INTDIR)\virt_mem.obj"

"$(OUTDIR)\condor_util.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "condor_util_lib - Win32 Release" || "$(CFG)" ==\
 "condor_util_lib - Win32 Debug"
SOURCE=..\src\condor_util_lib\basename.c
DEP_CPP_BASEN=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\basename.obj" : $(SOURCE) $(DEP_CPP_BASEN) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\blankline.c
DEP_CPP_BLANK=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\blankline.obj" : $(SOURCE) $(DEP_CPP_BLANK) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\ckpt_name.c
DEP_CPP_CKPT_=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	

"$(INTDIR)\ckpt_name.obj" : $(SOURCE) $(DEP_CPP_CKPT_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\condor_common.C
DEP_CPP_CONDO=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"$(INTDIR)\condor_common.pch" /Yc"condor_common.h" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\condor_common.obj"	"$(INTDIR)\condor_common.pch" : $(SOURCE)\
 $(DEP_CPP_CONDO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yc"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\condor_common.obj"	"$(INTDIR)\condor_common.pch" : $(SOURCE)\
 $(DEP_CPP_CONDO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\condor_errlst.c
DEP_CPP_CONDOR=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\condor_errlst.obj" : $(SOURCE) $(DEP_CPP_CONDOR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\cronos.c
DEP_CPP_CRONO=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\cronos.obj" : $(SOURCE) $(DEP_CPP_CRONO) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\dprintf.c
DEP_CPP_DPRIN=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\clib.h"\
	"..\src\h\condor_sys.h"\
	"..\src\h\except.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\syscall_numbers.h"\
	

"$(INTDIR)\dprintf.obj" : $(SOURCE) $(DEP_CPP_DPRIN) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\dprintf_config.c
DEP_CPP_DPRINT=\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\condor_ckpt_name.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_getmnt.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_status.h"\
	"..\src\condor_includes\condor_string.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\clib.h"\
	"..\src\h\condor_sys.h"\
	"..\src\h\condor_types.h"\
	"..\src\h\debug.h"\
	"..\src\h\except.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\proc.h"\
	"..\src\h\syscall_numbers.h"\
	"..\src\h\util_lib_proto.h"\
	

"$(INTDIR)\dprintf_config.obj" : $(SOURCE) $(DEP_CPP_DPRINT) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\email.c
DEP_CPP_EMAIL=\
	"..\src\condor_c++_util\my_hostname.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_email.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\debug.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\email.obj" : $(SOURCE) $(DEP_CPP_EMAIL) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\email_file.c
DEP_CPP_EMAIL_=\
	"..\src\condor_includes\basename.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_email.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\debug.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\email_file.obj" : $(SOURCE) $(DEP_CPP_EMAIL_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\escapes.c
DEP_CPP_ESCAP=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\escapes.obj" : $(SOURCE) $(DEP_CPP_ESCAP) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\escapes.obj" : $(SOURCE) $(DEP_CPP_ESCAP) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\except.c
DEP_CPP_EXCEP=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\condor_sys.h"\
	"..\src\h\exit.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\syscall_numbers.h"\
	

"$(INTDIR)\except.obj" : $(SOURCE) $(DEP_CPP_EXCEP) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\fdprintf.c
DEP_CPP_FDPRI=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\fdprintf.obj" : $(SOURCE) $(DEP_CPP_FDPRI) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\free_fs_blocks.c
DEP_CPP_FREE_=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\free_fs_blocks.obj" : $(SOURCE) $(DEP_CPP_FREE_) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\free_fs_blocks.obj" : $(SOURCE) $(DEP_CPP_FREE_) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\get_random_num.c
DEP_CPP_GET_R=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\get_random_num.obj" : $(SOURCE) $(DEP_CPP_GET_R) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\internet.c
DEP_CPP_INTER=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\except.h"\
	"..\src\h\expr.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	"..\src\h\proc.h"\
	

"$(INTDIR)\internet.obj" : $(SOURCE) $(DEP_CPP_INTER) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\lock_file.WIN32.c
DEP_CPP_LOCK_=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\lock_file.WIN32.obj" : $(SOURCE) $(DEP_CPP_LOCK_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\ltrunc.c
DEP_CPP_LTRUN=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\ltrunc.obj" : $(SOURCE) $(DEP_CPP_LTRUN) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\match_prefix.c
DEP_CPP_MATCH=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\match_prefix.obj" : $(SOURCE) $(DEP_CPP_MATCH) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\mkargv.c
DEP_CPP_MKARG=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\mkargv.obj" : $(SOURCE) $(DEP_CPP_MKARG) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\phys_mem.c
DEP_CPP_PHYS_=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\phys_mem.obj" : $(SOURCE) $(DEP_CPP_PHYS_) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\phys_mem.obj" : $(SOURCE) $(DEP_CPP_PHYS_) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\rotate_file.c
DEP_CPP_ROTAT=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\rotate_file.obj" : $(SOURCE) $(DEP_CPP_ROTAT) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\setsyscalls.c

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\setsyscalls.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\setsyscalls.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\signames.c

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\signames.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\signames.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\strcmp_until.c

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\"\
 /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\strcmp_until.obj" : $(SOURCE) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\strcmp_until.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\uids.c
DEP_CPP_UIDS_=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\uids.obj" : $(SOURCE) $(DEP_CPP_UIDS_) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\uids.obj" : $(SOURCE) $(DEP_CPP_UIDS_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\virt_mem.c
DEP_CPP_VIRT_=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	

"$(INTDIR)\virt_mem.obj" : $(SOURCE) $(DEP_CPP_VIRT_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

