# Microsoft Developer Studio Generated NMAKE File, Based on condor_io.dsp
!IF "$(CFG)" == ""
CFG=condor_io - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_io - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_io - Win32 Release" && "$(CFG)" !=\
 "condor_io - Win32 Debug"
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

CPP=cl.exe

!IF  "$(CFG)" == "condor_io - Win32 Release"

OUTDIR=.\..\src\condor_io
INTDIR=.\..\src\condor_io
# Begin Custom Macros
OutDir=.\..\src\condor_io
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_io.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_io.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\buffers.obj"
	-@erase "$(INTDIR)\condor_rw.obj"
	-@erase "$(INTDIR)\open_flags.obj"
	-@erase "$(INTDIR)\reli_sock.obj"
	-@erase "$(INTDIR)\safe_sock.obj"
	-@erase "$(INTDIR)\sock.obj"
	-@erase "$(INTDIR)\sockCache.obj"
	-@erase "$(INTDIR)\stream.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_io.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_io/
CPP_SBRS=.
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_io.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_io.lib" 
LIB32_OBJS= \
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

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_io.lib"

!ELSE 

ALL : "$(OUTDIR)\condor_io.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\buffers.obj"
	-@erase "$(INTDIR)\condor_rw.obj"
	-@erase "$(INTDIR)\open_flags.obj"
	-@erase "$(INTDIR)\reli_sock.obj"
	-@erase "$(INTDIR)\safe_sock.obj"
	-@erase "$(INTDIR)\sock.obj"
	-@erase "$(INTDIR)\sockCache.obj"
	-@erase "$(INTDIR)\stream.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_io.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_io/
CPP_SBRS=.
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_io.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\condor_io.lib" 
LIB32_OBJS= \
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


!IF "$(CFG)" == "condor_io - Win32 Release" || "$(CFG)" ==\
 "condor_io - Win32 Debug"
SOURCE=..\src\condor_io\buffers.C
DEP_CPP_BUFFE=\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\buffers.obj" : $(SOURCE) $(DEP_CPP_BUFFE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\condor_rw.C
DEP_CPP_CONDO=\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\condor_rw.obj" : $(SOURCE) $(DEP_CPP_CONDO) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\open_flags.c

"$(INTDIR)\open_flags.obj" : $(SOURCE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\reli_sock.C
DEP_CPP_RELI_=\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\condor_io\condor_rw.h"\
	"..\src\h\expr.h"\
	"..\src\h\internet.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\reli_sock.obj" : $(SOURCE) $(DEP_CPP_RELI_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\safe_sock.C
DEP_CPP_SAFE_=\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\expr.h"\
	"..\src\h\internet.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\safe_sock.obj" : $(SOURCE) $(DEP_CPP_SAFE_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\sock.C
DEP_CPP_SOCK_=\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_expressions.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\expr.h"\
	"..\src\h\internet.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\sock.obj" : $(SOURCE) $(DEP_CPP_SOCK_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\sockCache.C
DEP_CPP_SOCKC=\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\sockCache.obj" : $(SOURCE) $(DEP_CPP_SOCKC) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\stream.C
DEP_CPP_STREA=\
	"..\src\condor_includes\buffers.h"\
	"..\src\condor_includes\condor_commands.h"\
	"..\src\condor_includes\condor_common.h"\
	"..\src\condor_includes\condor_constants.h"\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_io.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_includes\reli_sock.h"\
	"..\src\condor_includes\safe_sock.h"\
	"..\src\condor_includes\sock.h"\
	"..\src\condor_includes\sockCache.h"\
	"..\src\condor_includes\stream.h"\
	"..\src\h\proc.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	

"$(INTDIR)\stream.obj" : $(SOURCE) $(DEP_CPP_STREA) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

