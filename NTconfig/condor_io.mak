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

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I\
 "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h"\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_io/
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

ALL : "$(OUTDIR)\condor_io.lib" "$(OUTDIR)\condor_io.bsc"

!ELSE 

ALL : "$(OUTDIR)\condor_io.lib" "$(OUTDIR)\condor_io.bsc"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\buffers.obj"
	-@erase "$(INTDIR)\buffers.sbr"
	-@erase "$(INTDIR)\condor_rw.obj"
	-@erase "$(INTDIR)\condor_rw.sbr"
	-@erase "$(INTDIR)\open_flags.obj"
	-@erase "$(INTDIR)\open_flags.sbr"
	-@erase "$(INTDIR)\reli_sock.obj"
	-@erase "$(INTDIR)\reli_sock.sbr"
	-@erase "$(INTDIR)\safe_sock.obj"
	-@erase "$(INTDIR)\safe_sock.sbr"
	-@erase "$(INTDIR)\sock.obj"
	-@erase "$(INTDIR)\sock.sbr"
	-@erase "$(INTDIR)\sockCache.obj"
	-@erase "$(INTDIR)\sockCache.sbr"
	-@erase "$(INTDIR)\stream.obj"
	-@erase "$(INTDIR)\stream.sbr"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\condor_io.bsc"
	-@erase "$(OUTDIR)\condor_io.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes"\
 /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /FR"$(INTDIR)\\" /Fp"..\src\condor_c++_util/condor_common.pch"\
 /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP /c 
CPP_OBJS=..\src\condor_io/
CPP_SBRS=..\src\condor_io/

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_io.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\buffers.sbr" \
	"$(INTDIR)\condor_rw.sbr" \
	"$(INTDIR)\open_flags.sbr" \
	"$(INTDIR)\reli_sock.sbr" \
	"$(INTDIR)\safe_sock.sbr" \
	"$(INTDIR)\sock.sbr" \
	"$(INTDIR)\sockCache.sbr" \
	"$(INTDIR)\stream.sbr"

"$(OUTDIR)\condor_io.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

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


!IF "$(CFG)" == "condor_io - Win32 Release" || "$(CFG)" ==\
 "condor_io - Win32 Debug"
SOURCE=..\src\condor_io\buffers.C
DEP_CPP_BUFFE=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_io - Win32 Release"


"$(INTDIR)\buffers.obj" : $(SOURCE) $(DEP_CPP_BUFFE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_io - Win32 Debug"


"$(INTDIR)\buffers.obj"	"$(INTDIR)\buffers.sbr" : $(SOURCE) $(DEP_CPP_BUFFE)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_io\condor_rw.C
DEP_CPP_CONDO=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_io - Win32 Release"


"$(INTDIR)\condor_rw.obj" : $(SOURCE) $(DEP_CPP_CONDO) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_io - Win32 Debug"


"$(INTDIR)\condor_rw.obj"	"$(INTDIR)\condor_rw.sbr" : $(SOURCE)\
 $(DEP_CPP_CONDO) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_io\open_flags.c

!IF  "$(CFG)" == "condor_io - Win32 Release"


"$(INTDIR)\open_flags.obj" : $(SOURCE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_io - Win32 Debug"


"$(INTDIR)\open_flags.obj"	"$(INTDIR)\open_flags.sbr" : $(SOURCE) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_io\reli_sock.C
DEP_CPP_RELI_=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\condor_io\condor_rw.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_expressions.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"expr.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_io - Win32 Release"


"$(INTDIR)\reli_sock.obj" : $(SOURCE) $(DEP_CPP_RELI_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_io - Win32 Debug"


"$(INTDIR)\reli_sock.obj"	"$(INTDIR)\reli_sock.sbr" : $(SOURCE)\
 $(DEP_CPP_RELI_) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_io\safe_sock.C
DEP_CPP_SAFE_=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_expressions.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"expr.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_io - Win32 Release"


"$(INTDIR)\safe_sock.obj" : $(SOURCE) $(DEP_CPP_SAFE_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_io - Win32 Debug"


"$(INTDIR)\safe_sock.obj"	"$(INTDIR)\safe_sock.sbr" : $(SOURCE)\
 $(DEP_CPP_SAFE_) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_io\sock.C
DEP_CPP_SOCK_=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\internet.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_expressions.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"expr.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_io - Win32 Release"


"$(INTDIR)\sock.obj" : $(SOURCE) $(DEP_CPP_SOCK_) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_io - Win32 Debug"


"$(INTDIR)\sock.obj"	"$(INTDIR)\sock.sbr" : $(SOURCE) $(DEP_CPP_SOCK_)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_io\sockCache.C
DEP_CPP_SOCKC=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_io - Win32 Release"


"$(INTDIR)\sockCache.obj" : $(SOURCE) $(DEP_CPP_SOCKC) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_io - Win32 Debug"


"$(INTDIR)\sockCache.obj"	"$(INTDIR)\sockCache.sbr" : $(SOURCE)\
 $(DEP_CPP_SOCKC) "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\condor_io\stream.C
DEP_CPP_STREA=\
	"..\src\condor_includes\condor_debug.h"\
	"..\src\condor_includes\condor_network.h"\
	"..\src\h\sched.h"\
	"..\src\h\startup.h"\
	{$(INCLUDE)}"buffers.h"\
	{$(INCLUDE)}"condor_commands.h"\
	{$(INCLUDE)}"condor_common.h"\
	{$(INCLUDE)}"condor_constants.h"\
	{$(INCLUDE)}"condor_io.h"\
	{$(INCLUDE)}"proc.h"\
	{$(INCLUDE)}"reli_sock.h"\
	{$(INCLUDE)}"safe_sock.h"\
	{$(INCLUDE)}"sock.h"\
	{$(INCLUDE)}"sockCache.h"\
	{$(INCLUDE)}"stream.h"\
	

!IF  "$(CFG)" == "condor_io - Win32 Release"


"$(INTDIR)\stream.obj" : $(SOURCE) $(DEP_CPP_STREA) "$(INTDIR)"\
 "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "condor_io - Win32 Debug"


"$(INTDIR)\stream.obj"	"$(INTDIR)\stream.sbr" : $(SOURCE) $(DEP_CPP_STREA)\
 "$(INTDIR)" "..\src\condor_c++_util\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

