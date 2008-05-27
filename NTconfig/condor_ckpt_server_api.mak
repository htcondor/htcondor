# Microsoft Developer Studio Generated NMAKE File, Based on condor_ckpt_server_api.dsp
!IF "$(CFG)" == ""
CFG=condor_ckpt_server_api - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_ckpt_server_api -\
 Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_ckpt_server_api - Win32 Release" && "$(CFG)" !=\
 "condor_ckpt_server_api - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_ckpt_server_api.mak"\
 CFG="condor_ckpt_server_api - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_ckpt_server_api - Win32 Release" (based on\
 "Win32 (x86) Static Library")
!MESSAGE "condor_ckpt_server_api - Win32 Debug" (based on\
 "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_ckpt_server_api - Win32 Release"

OUTDIR=..\src\condor_ckpt_server
INTDIR=..\src\condor_ckpt_server
# Begin Custom Macros
OutDir=..\src\condor_ckpt_server
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_ckpt_server_api.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_ckpt_server_api.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\network2.obj"
	-@erase "$(INTDIR)\server_interface.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_ckpt_server_api.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=..\src\condor_ckpt_server/
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_ckpt_server_api.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_ckpt_server_api.lib" 
LIB32_OBJS= \
	"$(INTDIR)\network2.obj" \
	"$(INTDIR)\server_interface.obj"

"$(OUTDIR)\condor_ckpt_server_api.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_ckpt_server_api - Win32 Debug"

OUTDIR=..\src\condor_ckpt_server
INTDIR=..\src\condor_ckpt_server
# Begin Custom Macros
OutDir=..\src\condor_ckpt_server
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_ckpt_server_api.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_ckpt_server_api.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\network2.obj"
	-@erase "$(INTDIR)\server_interface.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_ckpt_server_api.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=..\src\condor_ckpt_server/
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_ckpt_server_api.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_ckpt_server_api.lib" 
LIB32_OBJS= \
	"$(INTDIR)\network2.obj" \
	"$(INTDIR)\server_interface.obj"

"$(OUTDIR)\condor_ckpt_server_api.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "condor_ckpt_server_api - Win32 Release" || "$(CFG)" ==\
 "condor_ckpt_server_api - Win32 Debug"
SOURCE=..\src\condor_ckpt_server\network2.c
DEP_CPP_NETWO=\
	"..\src\condor_ckpt_server\constants2.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\h\debug.h"\
	

"$(INTDIR)\network2.obj" : $(SOURCE) $(DEP_CPP_NETWO) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_ckpt_server\server_interface.c
DEP_CPP_SERVE=\
	"..\src\condor_ckpt_server\constants2.h"\
	"..\src\condor_ckpt_server\network2.h"\
	"..\src\condor_ckpt_server\server_interface.h"\
	"..\src\condor_ckpt_server\typedefs2.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\h\sig_install.h"\
	

"$(INTDIR)\server_interface.obj" : $(SOURCE) $(DEP_CPP_SERVE) "$(INTDIR)"\
 "..\src\condor_util_lib\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

