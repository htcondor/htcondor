# Microsoft Developer Studio Generated NMAKE File, Based on condor_sysapi.dsp
!IF "$(CFG)" == ""
CFG=condor_sysapi - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_sysapi - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_sysapi - Win32 Debug" && "$(CFG)" != "condor_sysapi - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_sysapi.mak" CFG="condor_sysapi - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_sysapi - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_sysapi - Win32 Release" (based on "Win32 (x86) Static Library")
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

!IF  "$(CFG)" == "condor_sysapi - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_sysapi.lib"


CLEAN :
	-@erase "$(INTDIR)\arch.obj"
	-@erase "$(INTDIR)\ckptpltfrm.obj"
	-@erase "$(INTDIR)\clinpack.obj"
	-@erase "$(INTDIR)\dhry21a.obj"
	-@erase "$(INTDIR)\dhry21b.obj"
	-@erase "$(INTDIR)\free_fs_blocks.obj"
	-@erase "$(INTDIR)\idle_time.obj"
	-@erase "$(INTDIR)\kernel_memory_model.obj"
	-@erase "$(INTDIR)\kernel_version.obj"
	-@erase "$(INTDIR)\last_x_event.obj"
	-@erase "$(INTDIR)\load_avg.obj"
	-@erase "$(INTDIR)\ncpus.obj"
	-@erase "$(INTDIR)\phys_mem.obj"
	-@erase "$(INTDIR)\reconfig.obj"
	-@erase "$(INTDIR)\resource_limits.obj"
	-@erase "$(INTDIR)\timers_b.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\virt_mem.obj"
	-@erase "$(OUTDIR)\condor_sysapi.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common_c.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_sysapi.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_sysapi.lib" 
LIB32_OBJS= \
	"$(INTDIR)\arch.obj" \
	"$(INTDIR)\ckptpltfrm.obj" \
	"$(INTDIR)\clinpack.obj" \
	"$(INTDIR)\dhry21a.obj" \
	"$(INTDIR)\dhry21b.obj" \
	"$(INTDIR)\free_fs_blocks.obj" \
	"$(INTDIR)\idle_time.obj" \
	"$(INTDIR)\kernel_memory_model.obj" \
	"$(INTDIR)\kernel_version.obj" \
	"$(INTDIR)\last_x_event.obj" \
	"$(INTDIR)\load_avg.obj" \
	"$(INTDIR)\ncpus.obj" \
	"$(INTDIR)\phys_mem.obj" \
	"$(INTDIR)\reconfig.obj" \
	"$(INTDIR)\resource_limits.obj" \
	"$(INTDIR)\timers_b.obj" \
	"$(INTDIR)\virt_mem.obj"

"$(OUTDIR)\condor_sysapi.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_sysapi - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_sysapi.lib"


CLEAN :
	-@erase "$(INTDIR)\arch.obj"
	-@erase "$(INTDIR)\ckptpltfrm.obj"
	-@erase "$(INTDIR)\clinpack.obj"
	-@erase "$(INTDIR)\dhry21a.obj"
	-@erase "$(INTDIR)\dhry21b.obj"
	-@erase "$(INTDIR)\free_fs_blocks.obj"
	-@erase "$(INTDIR)\idle_time.obj"
	-@erase "$(INTDIR)\kernel_memory_model.obj"
	-@erase "$(INTDIR)\kernel_version.obj"
	-@erase "$(INTDIR)\last_x_event.obj"
	-@erase "$(INTDIR)\load_avg.obj"
	-@erase "$(INTDIR)\ncpus.obj"
	-@erase "$(INTDIR)\phys_mem.obj"
	-@erase "$(INTDIR)\reconfig.obj"
	-@erase "$(INTDIR)\resource_limits.obj"
	-@erase "$(INTDIR)\timers_b.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\virt_mem.obj"
	-@erase "$(OUTDIR)\condor_sysapi.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common_c.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_sysapi.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_sysapi.lib" 
LIB32_OBJS= \
	"$(INTDIR)\arch.obj" \
	"$(INTDIR)\ckptpltfrm.obj" \
	"$(INTDIR)\clinpack.obj" \
	"$(INTDIR)\dhry21a.obj" \
	"$(INTDIR)\dhry21b.obj" \
	"$(INTDIR)\free_fs_blocks.obj" \
	"$(INTDIR)\idle_time.obj" \
	"$(INTDIR)\kernel_memory_model.obj" \
	"$(INTDIR)\kernel_version.obj" \
	"$(INTDIR)\last_x_event.obj" \
	"$(INTDIR)\load_avg.obj" \
	"$(INTDIR)\ncpus.obj" \
	"$(INTDIR)\phys_mem.obj" \
	"$(INTDIR)\reconfig.obj" \
	"$(INTDIR)\resource_limits.obj" \
	"$(INTDIR)\timers_b.obj" \
	"$(INTDIR)\virt_mem.obj"

"$(OUTDIR)\condor_sysapi.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_sysapi.dep")
!INCLUDE "condor_sysapi.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_sysapi.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_sysapi - Win32 Debug" || "$(CFG)" == "condor_sysapi - Win32 Release"
SOURCE=..\src\condor_sysapi\arch.c

"$(INTDIR)\arch.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\ckptpltfrm.c

"$(INTDIR)\ckptpltfrm.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\clinpack.c

!IF  "$(CFG)" == "condor_sysapi - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\clinpack.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_sysapi - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\clinpack.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_sysapi\dhry21a.c

!IF  "$(CFG)" == "condor_sysapi - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\dhry21a.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_sysapi - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\dhry21a.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_sysapi\dhry21b.c

!IF  "$(CFG)" == "condor_sysapi - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\dhry21b.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_sysapi - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\dhry21b.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_sysapi\free_fs_blocks.c

"$(INTDIR)\free_fs_blocks.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\idle_time.C

"$(INTDIR)\idle_time.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\kernel_memory_model.c

"$(INTDIR)\kernel_memory_model.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\kernel_version.c

"$(INTDIR)\kernel_version.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\last_x_event.c

"$(INTDIR)\last_x_event.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\load_avg.c

"$(INTDIR)\load_avg.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\ncpus.c

"$(INTDIR)\ncpus.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\phys_mem.c

"$(INTDIR)\phys_mem.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\reconfig.C

"$(INTDIR)\reconfig.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\resource_limits.c

"$(INTDIR)\resource_limits.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_sysapi\timers_b.c

!IF  "$(CFG)" == "condor_sysapi - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\timers_b.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_sysapi - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\timers_b.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_sysapi\virt_mem.c

"$(INTDIR)\virt_mem.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

