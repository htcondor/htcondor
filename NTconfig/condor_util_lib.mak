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
	-@erase "$(INTDIR)\dprintf.obj"
	-@erase "$(INTDIR)\dprintf_config.obj"
	-@erase "$(INTDIR)\escapes.obj"
	-@erase "$(INTDIR)\except.obj"
	-@erase "$(INTDIR)\free_fs_blocks.obj"
	-@erase "$(INTDIR)\get_random_num.obj"
	-@erase "$(INTDIR)\internet.obj"
	-@erase "$(INTDIR)\lock_file.WIN32.obj"
	-@erase "$(INTDIR)\ltrunc.obj"
	-@erase "$(INTDIR)\match_prefix.obj"
	-@erase "$(INTDIR)\mkargv.obj"
	-@erase "$(INTDIR)\phys_mem.obj"
	-@erase "$(INTDIR)\setsyscalls.obj"
	-@erase "$(INTDIR)\signames.obj"
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
	"$(INTDIR)\dprintf.obj" \
	"$(INTDIR)\dprintf_config.obj" \
	"$(INTDIR)\escapes.obj" \
	"$(INTDIR)\except.obj" \
	"$(INTDIR)\free_fs_blocks.obj" \
	"$(INTDIR)\get_random_num.obj" \
	"$(INTDIR)\internet.obj" \
	"$(INTDIR)\lock_file.WIN32.obj" \
	"$(INTDIR)\ltrunc.obj" \
	"$(INTDIR)\match_prefix.obj" \
	"$(INTDIR)\mkargv.obj" \
	"$(INTDIR)\phys_mem.obj" \
	"$(INTDIR)\setsyscalls.obj" \
	"$(INTDIR)\signames.obj" \
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

ALL : "$(OUTDIR)\condor_util.lib" "$(OUTDIR)\condor_util_lib.bsc"

!ELSE 

ALL : "$(OUTDIR)\condor_util.lib" "$(OUTDIR)\condor_util_lib.bsc"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\basename.obj"
	-@erase "$(INTDIR)\basename.sbr"
	-@erase "$(INTDIR)\blankline.obj"
	-@erase "$(INTDIR)\blankline.sbr"
	-@erase "$(INTDIR)\ckpt_name.obj"
	-@erase "$(INTDIR)\ckpt_name.sbr"
	-@erase "$(INTDIR)\condor_common.obj"
	-@erase "$(INTDIR)\condor_common.pch"
	-@erase "$(INTDIR)\condor_common.sbr"
	-@erase "$(INTDIR)\condor_errlst.obj"
	-@erase "$(INTDIR)\condor_errlst.sbr"
	-@erase "$(INTDIR)\dprintf.obj"
	-@erase "$(INTDIR)\dprintf.sbr"
	-@erase "$(INTDIR)\dprintf_config.obj"
	-@erase "$(INTDIR)\dprintf_config.sbr"
	-@erase "$(INTDIR)\escapes.obj"
	-@erase "$(INTDIR)\escapes.sbr"
	-@erase "$(INTDIR)\except.obj"
	-@erase "$(INTDIR)\except.sbr"
	-@erase "$(INTDIR)\free_fs_blocks.obj"
	-@erase "$(INTDIR)\free_fs_blocks.sbr"
	-@erase "$(INTDIR)\get_random_num.obj"
	-@erase "$(INTDIR)\get_random_num.sbr"
	-@erase "$(INTDIR)\internet.obj"
	-@erase "$(INTDIR)\internet.sbr"
	-@erase "$(INTDIR)\lock_file.WIN32.obj"
	-@erase "$(INTDIR)\lock_file.WIN32.sbr"
	-@erase "$(INTDIR)\ltrunc.obj"
	-@erase "$(INTDIR)\ltrunc.sbr"
	-@erase "$(INTDIR)\match_prefix.obj"
	-@erase "$(INTDIR)\match_prefix.sbr"
	-@erase "$(INTDIR)\mkargv.obj"
	-@erase "$(INTDIR)\mkargv.sbr"
	-@erase "$(INTDIR)\phys_mem.obj"
	-@erase "$(INTDIR)\phys_mem.sbr"
	-@erase "$(INTDIR)\setsyscalls.obj"
	-@erase "$(INTDIR)\setsyscalls.sbr"
	-@erase "$(INTDIR)\signames.obj"
	-@erase "$(INTDIR)\signames.sbr"
	-@erase "$(INTDIR)\uids.obj"
	-@erase "$(INTDIR)\uids.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\virt_mem.obj"
	-@erase "$(INTDIR)\virt_mem.sbr"
	-@erase "$(OUTDIR)\condor_util.lib"
	-@erase "$(OUTDIR)\condor_util_lib.bsc"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR"$(INTDIR)\\" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=..\src\condor_util_lib/
CPP_SBRS=..\src\condor_util_lib/

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
	"$(INTDIR)\basename.sbr" \
	"$(INTDIR)\blankline.sbr" \
	"$(INTDIR)\ckpt_name.sbr" \
	"$(INTDIR)\condor_common.sbr" \
	"$(INTDIR)\condor_errlst.sbr" \
	"$(INTDIR)\dprintf.sbr" \
	"$(INTDIR)\dprintf_config.sbr" \
	"$(INTDIR)\escapes.sbr" \
	"$(INTDIR)\except.sbr" \
	"$(INTDIR)\free_fs_blocks.sbr" \
	"$(INTDIR)\get_random_num.sbr" \
	"$(INTDIR)\internet.sbr" \
	"$(INTDIR)\lock_file.WIN32.sbr" \
	"$(INTDIR)\ltrunc.sbr" \
	"$(INTDIR)\match_prefix.sbr" \
	"$(INTDIR)\mkargv.sbr" \
	"$(INTDIR)\phys_mem.sbr" \
	"$(INTDIR)\setsyscalls.sbr" \
	"$(INTDIR)\signames.sbr" \
	"$(INTDIR)\uids.sbr" \
	"$(INTDIR)\virt_mem.sbr"

"$(OUTDIR)\condor_util_lib.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_util.lib" 
LIB32_OBJS= \
	"$(INTDIR)\basename.obj" \
	"$(INTDIR)\blankline.obj" \
	"$(INTDIR)\ckpt_name.obj" \
	"$(INTDIR)\condor_common.obj" \
	"$(INTDIR)\condor_errlst.obj" \
	"$(INTDIR)\dprintf.obj" \
	"$(INTDIR)\dprintf_config.obj" \
	"$(INTDIR)\escapes.obj" \
	"$(INTDIR)\except.obj" \
	"$(INTDIR)\free_fs_blocks.obj" \
	"$(INTDIR)\get_random_num.obj" \
	"$(INTDIR)\internet.obj" \
	"$(INTDIR)\lock_file.WIN32.obj" \
	"$(INTDIR)\ltrunc.obj" \
	"$(INTDIR)\match_prefix.obj" \
	"$(INTDIR)\mkargv.obj" \
	"$(INTDIR)\phys_mem.obj" \
	"$(INTDIR)\setsyscalls.obj" \
	"$(INTDIR)\signames.obj" \
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
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\basename.obj" : $(SOURCE) $(DEP_CPP_BASEN) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\basename.obj"	"$(INTDIR)\basename.sbr" : $(SOURCE) $(DEP_CPP_BASEN)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\blankline.c
DEP_CPP_BLANK=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\blankline.obj" : $(SOURCE) $(DEP_CPP_BLANK) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\blankline.obj"	"$(INTDIR)\blankline.sbr" : $(SOURCE)\
 $(DEP_CPP_BLANK) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\ckpt_name.c
DEP_CPP_CKPT_=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	{$(INCLUDE)}"proc.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\ckpt_name.obj" : $(SOURCE) $(DEP_CPP_CKPT_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\ckpt_name.obj"	"$(INTDIR)\ckpt_name.sbr" : $(SOURCE)\
 $(DEP_CPP_CKPT_) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\condor_common.C
DEP_CPP_CONDO=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

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
 "_WINDOWS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\condor_common.pch"\
 /Yc"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\condor_common.obj"	"$(INTDIR)\condor_common.sbr"\
	"$(INTDIR)\condor_common.pch" : $(SOURCE) $(DEP_CPP_CONDO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\condor_errlst.c
DEP_CPP_CONDOR=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\condor_errlst.obj" : $(SOURCE) $(DEP_CPP_CONDOR) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\condor_errlst.obj"	"$(INTDIR)\condor_errlst.sbr" : $(SOURCE)\
 $(DEP_CPP_CONDOR) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\dprintf.c
DEP_CPP_DPRIN=\
	"..\src\condor_includes\condor_uid.h"\
	"..\src\h\clib.h"\
	"..\src\h\condor_sys.h"\
	"..\src\h\debug.h"\
	"..\src\h\except.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\dprintf.obj" : $(SOURCE) $(DEP_CPP_DPRIN) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\dprintf.obj"	"$(INTDIR)\dprintf.sbr" : $(SOURCE) $(DEP_CPP_DPRIN)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\dprintf_config.c
DEP_CPP_DPRINT=\
	"..\src\condor_includes\condor_string.h"\
	"..\src\h\clib.h"\
	"..\src\h\condor_sys.h"\
	"..\src\h\debug.h"\
	"..\src\h\except.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\dprintf_config.obj" : $(SOURCE) $(DEP_CPP_DPRINT) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\dprintf_config.obj"	"$(INTDIR)\dprintf_config.sbr" : $(SOURCE)\
 $(DEP_CPP_DPRINT) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\escapes.c
DEP_CPP_ESCAP=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

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
 "_WINDOWS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\condor_common.pch"\
 /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\escapes.obj"	"$(INTDIR)\escapes.sbr" : $(SOURCE) $(DEP_CPP_ESCAP)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\except.c
DEP_CPP_EXCEP=\
	"..\src\h\condor_sys.h"\
	"..\src\h\debug.h"\
	"..\src\h\exit.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\except.obj" : $(SOURCE) $(DEP_CPP_EXCEP) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\except.obj"	"$(INTDIR)\except.sbr" : $(SOURCE) $(DEP_CPP_EXCEP)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\free_fs_blocks.c
DEP_CPP_FREE_=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

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
 "_WINDOWS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\free_fs_blocks.obj"	"$(INTDIR)\free_fs_blocks.sbr" : $(SOURCE)\
 $(DEP_CPP_FREE_) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\get_random_num.c
DEP_CPP_GET_R=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\get_random_num.obj" : $(SOURCE) $(DEP_CPP_GET_R) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\get_random_num.obj"	"$(INTDIR)\get_random_num.sbr" : $(SOURCE)\
 $(DEP_CPP_GET_R) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\internet.c
DEP_CPP_INTER=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\h\except.h"\
	"..\src\h\file_lock.h"\
	"..\src\h\internet.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_expressions.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"expr.h"\
	{$(INCLUDE)}"fake_flock.h"\
	{$(INCLUDE)}"proc.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\internet.obj" : $(SOURCE) $(DEP_CPP_INTER) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\internet.obj"	"$(INTDIR)\internet.sbr" : $(SOURCE) $(DEP_CPP_INTER)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\lock_file.WIN32.c
DEP_CPP_LOCK_=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\lock_file.WIN32.obj" : $(SOURCE) $(DEP_CPP_LOCK_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\lock_file.WIN32.obj"	"$(INTDIR)\lock_file.WIN32.sbr" : $(SOURCE)\
 $(DEP_CPP_LOCK_) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\ltrunc.c
DEP_CPP_LTRUN=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\ltrunc.obj" : $(SOURCE) $(DEP_CPP_LTRUN) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\ltrunc.obj"	"$(INTDIR)\ltrunc.sbr" : $(SOURCE) $(DEP_CPP_LTRUN)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\match_prefix.c
DEP_CPP_MATCH=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\match_prefix.obj" : $(SOURCE) $(DEP_CPP_MATCH) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\match_prefix.obj"	"$(INTDIR)\match_prefix.sbr" : $(SOURCE)\
 $(DEP_CPP_MATCH) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\mkargv.c
DEP_CPP_MKARG=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\mkargv.obj" : $(SOURCE) $(DEP_CPP_MKARG) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\mkargv.obj"	"$(INTDIR)\mkargv.sbr" : $(SOURCE) $(DEP_CPP_MKARG)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_util_lib\phys_mem.c
DEP_CPP_PHYS_=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

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
 "_WINDOWS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\phys_mem.obj"	"$(INTDIR)\phys_mem.sbr" : $(SOURCE) $(DEP_CPP_PHYS_)\
 "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

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
 "_WINDOWS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\setsyscalls.obj"	"$(INTDIR)\setsyscalls.sbr" : $(SOURCE) "$(INTDIR)"
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
 "_WINDOWS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\signames.obj"	"$(INTDIR)\signames.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\uids.c
DEP_CPP_UIDS_=\
	"..\src\condor_includes\condor_uid.h"\
	

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
 "_WINDOWS" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\uids.obj"	"$(INTDIR)\uids.sbr" : $(SOURCE) $(DEP_CPP_UIDS_)\
 "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\virt_mem.c
DEP_CPP_VIRT_=\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_file_lock.h"\
	{$(INCLUDE)}"condor_fix_assert.h"\
	{$(INCLUDE)}"condor_header_features.h"\
	{$(INCLUDE)}"condor_macros.h"\
	{$(INCLUDE)}"condor_sys_nt.h"\
	{$(INCLUDE)}"condor_system.h"\
	{$(INCLUDE)}"fake_flock.h"\
	

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"


"$(INTDIR)\virt_mem.obj" : $(SOURCE) $(DEP_CPP_VIRT_) "$(INTDIR)"\
 "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"


"$(INTDIR)\virt_mem.obj"	"$(INTDIR)\virt_mem.sbr" : $(SOURCE) $(DEP_CPP_VIRT_)\
 "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

