# Microsoft Developer Studio Generated NMAKE File, Based on condor_io.dsp
!IF "$(CFG)" == ""
CFG=condor_io - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_io - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_io - Win32 Release" && "$(CFG)" != "condor_io - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_io.mak" CFG="condor_io - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_io - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_io - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_io - Win32 Release"

OUTDIR=.\..\src\condor_io
INTDIR=.\..\src\condor_io
# Begin Custom Macros
OutDir=.\..\src\condor_io
# End Custom Macros

ALL : "$(OUTDIR)\condor_io.lib"


CLEAN :
	-@erase "$(INTDIR)\authentication.obj"
	-@erase "$(INTDIR)\buffers.obj"
	-@erase "$(INTDIR)\condor_rw.obj"
	-@erase "$(INTDIR)\open_flags.obj"
	-@erase "$(INTDIR)\reli_sock.obj"
	-@erase "$(INTDIR)\safe_sock.obj"
	-@erase "$(INTDIR)\sock.obj"
	-@erase "$(INTDIR)\sockCache.obj"
	-@erase "$(INTDIR)\stream.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_io.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_io.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_io.lib" 
LIB32_OBJS= \
	"$(INTDIR)\authentication.obj" \
	"$(INTDIR)\buffers.obj" \
	"$(INTDIR)\condor_rw.obj" \
	"$(INTDIR)\open_flags.obj" \
	"$(INTDIR)\reli_sock.obj" \
	"$(INTDIR)\safe_sock.obj" \
	"$(INTDIR)\sock.obj" \
	"$(INTDIR)\sockCache.obj" \
	"$(INTDIR)\stream.obj"

"$(OUTDIR)\condor_io.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_io - Win32 Debug"

OUTDIR=.\..\src\condor_io
INTDIR=.\..\src\condor_io
# Begin Custom Macros
OutDir=.\..\src\condor_io
# End Custom Macros

ALL : "$(OUTDIR)\condor_io.lib"


CLEAN :
	-@erase "$(INTDIR)\authentication.obj"
	-@erase "$(INTDIR)\buffers.obj"
	-@erase "$(INTDIR)\condor_rw.obj"
	-@erase "$(INTDIR)\open_flags.obj"
	-@erase "$(INTDIR)\reli_sock.obj"
	-@erase "$(INTDIR)\safe_sock.obj"
	-@erase "$(INTDIR)\sock.obj"
	-@erase "$(INTDIR)\sockCache.obj"
	-@erase "$(INTDIR)\stream.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_io.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 

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

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_io.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_io.lib" 
LIB32_OBJS= \
	"$(INTDIR)\authentication.obj" \
	"$(INTDIR)\buffers.obj" \
	"$(INTDIR)\condor_rw.obj" \
	"$(INTDIR)\open_flags.obj" \
	"$(INTDIR)\reli_sock.obj" \
	"$(INTDIR)\safe_sock.obj" \
	"$(INTDIR)\sock.obj" \
	"$(INTDIR)\sockCache.obj" \
	"$(INTDIR)\stream.obj"

"$(OUTDIR)\condor_io.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_io.dep")
!INCLUDE "condor_io.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_io.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_io - Win32 Release" || "$(CFG)" == "condor_io - Win32 Debug"
SOURCE=..\src\condor_io\authentication.C

"$(INTDIR)\authentication.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\buffers.C

"$(INTDIR)\buffers.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\condor_rw.C

"$(INTDIR)\condor_rw.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\open_flags.c

"$(INTDIR)\open_flags.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\reli_sock.C

"$(INTDIR)\reli_sock.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\safe_sock.C

"$(INTDIR)\safe_sock.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\sock.C

"$(INTDIR)\sock.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\sockCache.C

"$(INTDIR)\sockCache.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\stream.C

"$(INTDIR)\stream.obj" : $(SOURCE) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

