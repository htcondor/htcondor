# Microsoft Developer Studio Generated NMAKE File, Based on condor_startd_lib.dsp
!IF "$(CFG)" == ""
CFG=condor_startd_lib - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_startd_lib - Win32\
 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_startd_lib - Win32 Release" && "$(CFG)" !=\
 "condor_startd_lib - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_startd_lib.mak" CFG="condor_startd_lib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_startd_lib - Win32 Release" (based on\
 "Win32 (x86) Static Library")
!MESSAGE "condor_startd_lib - Win32 Debug" (based on\
 "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_startd_lib - Win32 Release"

OUTDIR=.\..\src\condor_startd.V6
INTDIR=.\..\src\condor_startd.V6
# Begin Custom Macros
OutDir=.\..\src\condor_startd.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_startd_lib.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_startd_lib.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\clinpack.obj"
	-@erase "$(INTDIR)\dhry21a.obj"
	-@erase "$(INTDIR)\dhry21b.obj"
	-@erase "$(INTDIR)\load_avg.WIN32.obj"
	-@erase "$(INTDIR)\timers_b.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_startd_lib.lib"

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_startd_lib.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_startd_lib.lib" 
LIB32_OBJS= \
	"$(INTDIR)\clinpack.obj" \
	"$(INTDIR)\dhry21a.obj" \
	"$(INTDIR)\dhry21b.obj" \
	"$(INTDIR)\load_avg.WIN32.obj" \
	"$(INTDIR)\timers_b.obj"

"$(OUTDIR)\condor_startd_lib.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_startd_lib - Win32 Debug"

OUTDIR=.\..\src\condor_startd.V6
INTDIR=.\..\src\condor_startd.V6
# Begin Custom Macros
OutDir=.\..\src\condor_startd.V6
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_startd_lib.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_startd_lib.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\clinpack.obj"
	-@erase "$(INTDIR)\dhry21a.obj"
	-@erase "$(INTDIR)\dhry21b.obj"
	-@erase "$(INTDIR)\load_avg.WIN32.obj"
	-@erase "$(INTDIR)\timers_b.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_startd_lib.lib"

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_startd_lib.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_startd_lib.lib" 
LIB32_OBJS= \
	"$(INTDIR)\clinpack.obj" \
	"$(INTDIR)\dhry21a.obj" \
	"$(INTDIR)\dhry21b.obj" \
	"$(INTDIR)\load_avg.WIN32.obj" \
	"$(INTDIR)\timers_b.obj"

"$(OUTDIR)\condor_startd_lib.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "condor_startd_lib - Win32 Release" || "$(CFG)" ==\
 "condor_startd_lib - Win32 Debug"
SOURCE=..\src\condor_startd.V6\clinpack.c

!IF  "$(CFG)" == "condor_startd_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\clinpack.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_startd_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\clinpack.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_startd.V6\dhry21a.c
DEP_CPP_DHRY2=\
	"..\src\condor_startd.V6\dhry.h"\
	

!IF  "$(CFG)" == "condor_startd_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dhry21a.obj" : $(SOURCE) $(DEP_CPP_DHRY2) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_startd_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dhry21a.obj" : $(SOURCE) $(DEP_CPP_DHRY2) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_startd.V6\dhry21b.c
DEP_CPP_DHRY21=\
	"..\src\condor_startd.V6\dhry.h"\
	

!IF  "$(CFG)" == "condor_startd_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dhry21b.obj" : $(SOURCE) $(DEP_CPP_DHRY21) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_startd_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dhry21b.obj" : $(SOURCE) $(DEP_CPP_DHRY21) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_startd.V6\load_avg.WIN32.c
DEP_CPP_LOAD_=\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_file_lock.h"\
	"..\src\condor_includes\condor_fix_assert.h"\
	"..\src\condor_includes\condor_fix_string.h"\
	"..\src\condor_includes\condor_header_features.h"\
	"..\src\condor_includes\condor_hpux_64bit_types.h"\
	"..\src\condor_includes\condor_macros.h"\
	"..\src\condor_includes\condor_sys_dux.h"\
	"..\src\condor_includes\condor_sys_hpux.h"\
	"..\src\condor_includes\condor_sys_irix.h"\
	"..\src\condor_includes\condor_sys_linux.h"\
	"..\src\condor_includes\condor_sys_nt.h"\
	"..\src\condor_includes\condor_sys_solaris.h"\
	"..\src\condor_includes\condor_system.h"\
	"..\src\h\fake_flock.h"\
	"..\src\h\file_lock.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\
	
NODEP_CPP_LOAD_=\
	"..\src\condor_includes\gssapi.h"\
	

"$(INTDIR)\load_avg.WIN32.obj" : $(SOURCE) $(DEP_CPP_LOAD_) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_startd.V6\timers_b.c
DEP_CPP_TIMER=\
	{$(INCLUDE)}"sys\timeb.h"\
	{$(INCLUDE)}"sys\types.h"\
	

!IF  "$(CFG)" == "condor_startd_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\timers_b.obj" : $(SOURCE) $(DEP_CPP_TIMER) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_startd_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I\
 "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\timers_b.obj" : $(SOURCE) $(DEP_CPP_TIMER) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 


!ENDIF 

