# Microsoft Developer Studio Generated NMAKE File, Based on condor_sysapi.dsp
!IF "$(CFG)" == ""
CFG=condor_sysapi - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_sysapi - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_sysapi - Win32 Release" && "$(CFG)" !=\
 "condor_sysapi - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_sysapi.mak" CFG="condor_sysapi - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_sysapi - Win32 Release" (based on\
 "Win32 (x86) Static Library")
!MESSAGE "condor_sysapi - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_sysapi - Win32 Release"

OUTDIR=.\..\src\condor_startd.V6
INTDIR=.\..\src\condor_startd.V6
# Begin Custom Macros
OutDir=.\..\src\condor_startd.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_sysapi.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_sysapi.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\arch.obj"
	-@erase "$(INTDIR)\clinpack.obj"
	-@erase "$(INTDIR)\dhry21a.obj"
	-@erase "$(INTDIR)\dhry21b.obj"
	-@erase "$(INTDIR)\free_fs_blocks.obj"
	-@erase "$(INTDIR)\idle_time.obj"
	-@erase "$(INTDIR)\last_x_event.obj"
	-@erase "$(INTDIR)\load_avg.obj"
	-@erase "$(INTDIR)\ncpus.obj"
	-@erase "$(INTDIR)\phys_mem.obj"
	-@erase "$(INTDIR)\reconfig.obj"
	-@erase "$(INTDIR)\timers_b.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\virt_mem.obj"
	-@erase "$(OUTDIR)\condor_sysapi.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
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

BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_sysapi.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_sysapi.lib" 
LIB32_OBJS= \
	"$(INTDIR)\arch.obj" \
	"$(INTDIR)\clinpack.obj" \
	"$(INTDIR)\dhry21a.obj" \
	"$(INTDIR)\dhry21b.obj" \
	"$(INTDIR)\free_fs_blocks.obj" \
	"$(INTDIR)\idle_time.obj" \
	"$(INTDIR)\last_x_event.obj" \
	"$(INTDIR)\load_avg.obj" \
	"$(INTDIR)\ncpus.obj" \
	"$(INTDIR)\phys_mem.obj" \
	"$(INTDIR)\reconfig.obj" \
	"$(INTDIR)\timers_b.obj" \
	"$(INTDIR)\virt_mem.obj"

"$(OUTDIR)\condor_sysapi.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_sysapi - Win32 Debug"

OUTDIR=.\..\src\condor_startd.V6
INTDIR=.\..\src\condor_startd.V6
# Begin Custom Macros
OutDir=.\..\src\condor_startd.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_sysapi.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_sysapi.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\arch.obj"
	-@erase "$(INTDIR)\clinpack.obj"
	-@erase "$(INTDIR)\dhry21a.obj"
	-@erase "$(INTDIR)\dhry21b.obj"
	-@erase "$(INTDIR)\free_fs_blocks.obj"
	-@erase "$(INTDIR)\idle_time.obj"
	-@erase "$(INTDIR)\last_x_event.obj"
	-@erase "$(INTDIR)\load_avg.obj"
	-@erase "$(INTDIR)\ncpus.obj"
	-@erase "$(INTDIR)\phys_mem.obj"
	-@erase "$(INTDIR)\reconfig.obj"
	-@erase "$(INTDIR)\timers_b.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\virt_mem.obj"
	-@erase "$(OUTDIR)\condor_sysapi.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
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

BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_sysapi.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_sysapi.lib" 
LIB32_OBJS= \
	"$(INTDIR)\arch.obj" \
	"$(INTDIR)\clinpack.obj" \
	"$(INTDIR)\dhry21a.obj" \
	"$(INTDIR)\dhry21b.obj" \
	"$(INTDIR)\free_fs_blocks.obj" \
	"$(INTDIR)\idle_time.obj" \
	"$(INTDIR)\last_x_event.obj" \
	"$(INTDIR)\load_avg.obj" \
	"$(INTDIR)\ncpus.obj" \
	"$(INTDIR)\phys_mem.obj" \
	"$(INTDIR)\reconfig.obj" \
	"$(INTDIR)\timers_b.obj" \
	"$(INTDIR)\virt_mem.obj"

"$(OUTDIR)\condor_sysapi.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "condor_sysapi - Win32 Release" || "$(CFG)" ==\
 "condor_sysapi - Win32 Debug"
SOURCE=..\src\condor_sysapi\arch.c
DEP_CPP_ARCH_=\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\match_prefix.h"\
	"..\src\condor_sysapi\sysapi.h"\
	

"$(INTDIR)\arch.obj" : $(SOURCE) $(DEP_CPP_ARCH_) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\clinpack.c

!IF  "$(CFG)" == "condor_sysapi - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\clinpack.obj" : $(SOURCE) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_sysapi - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /O2 /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\clinpack.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_sysapi\dhry21a.c
DEP_CPP_DHRY2=\
	"..\src\condor_sysapi\dhry.h"\
	

!IF  "$(CFG)" == "condor_sysapi - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dhry21a.obj" : $(SOURCE) $(DEP_CPP_DHRY2) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_sysapi - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /O2 /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dhry21a.obj" : $(SOURCE) $(DEP_CPP_DHRY2) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_sysapi\dhry21b.c
DEP_CPP_DHRY21=\
	"..\src\condor_sysapi\dhry.h"\
	

!IF  "$(CFG)" == "condor_sysapi - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dhry21b.obj" : $(SOURCE) $(DEP_CPP_DHRY21) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_sysapi - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /O2 /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dhry21b.obj" : $(SOURCE) $(DEP_CPP_DHRY21) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_sysapi\free_fs_blocks.c
DEP_CPP_FREE_=\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_sysapi\sysapi.h"\
	"..\src\condor_sysapi\sysapi_externs.h"\
	

"$(INTDIR)\free_fs_blocks.obj" : $(SOURCE) $(DEP_CPP_FREE_) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\idle_time.C
DEP_CPP_IDLE_=\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_sysapi\sysapi.h"\
	"..\src\condor_sysapi\sysapi_externs.h"\
	

"$(INTDIR)\idle_time.obj" : $(SOURCE) $(DEP_CPP_IDLE_) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\last_x_event.c
DEP_CPP_LAST_=\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_sysapi\sysapi.h"\
	"..\src\condor_sysapi\sysapi_externs.h"\
	

"$(INTDIR)\last_x_event.obj" : $(SOURCE) $(DEP_CPP_LAST_) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\load_avg.c
DEP_CPP_LOAD_=\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_sysapi\sysapi.h"\
	

"$(INTDIR)\load_avg.obj" : $(SOURCE) $(DEP_CPP_LOAD_) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\ncpus.c
DEP_CPP_NCPUS=\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_sysapi\sysapi.h"\
	

"$(INTDIR)\ncpus.obj" : $(SOURCE) $(DEP_CPP_NCPUS) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\phys_mem.c
DEP_CPP_PHYS_=\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_sysapi\sysapi.h"\
	

"$(INTDIR)\phys_mem.obj" : $(SOURCE) $(DEP_CPP_PHYS_) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\reconfig.C
DEP_CPP_RECON=\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_sysapi\sysapi.h"\
	"..\src\condor_sysapi\sysapi_externs.h"\
	

"$(INTDIR)\reconfig.obj" : $(SOURCE) $(DEP_CPP_RECON) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\timers_b.c

!IF  "$(CFG)" == "condor_sysapi - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\timers_b.obj" : $(SOURCE) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_sysapi - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /O2 /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\timers_b.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_sysapi\virt_mem.c
DEP_CPP_VIRT_=\
	"..\src\condor_includes\condor_config.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_sysapi\sysapi.h"\
	

"$(INTDIR)\virt_mem.obj" : $(SOURCE) $(DEP_CPP_VIRT_) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

