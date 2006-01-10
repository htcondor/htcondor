# Microsoft Developer Studio Generated NMAKE File, Based on condor_quill.dsp
!IF "$(CFG)" == ""
CFG=condor_quill - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_quill - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_quill - Win32 Debug" && "$(CFG)" != "condor_quill - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_quill.mak" CFG="condor_quill - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_quill - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "condor_quill - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_quill - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_quill.exe"

!ELSE 

ALL : "condor_sysapi - Win32 Debug" "condor_procapi - Win32 Debug" "condor_util_lib - Win32 Debug" "condor_io - Win32 Debug" "condor_daemon_core - Win32 Debug" "condor_cpp_util - Win32 Debug" "condor_classad - Win32 Debug" "$(OUTDIR)\condor_quill.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_classad - Win32 DebugCLEAN" "condor_cpp_util - Win32 DebugCLEAN" "condor_daemon_core - Win32 DebugCLEAN" "condor_io - Win32 DebugCLEAN" "condor_util_lib - Win32 DebugCLEAN" "condor_procapi - Win32 DebugCLEAN" "condor_sysapi - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\classadlogentry.obj"
	-@erase "$(INTDIR)\classadlogparser.obj"
	-@erase "$(INTDIR)\jobqueuecollection.obj"
	-@erase "$(INTDIR)\jobqueuedbmanager.obj"
	-@erase "$(INTDIR)\prober.obj"
	-@erase "$(INTDIR)\quill_main.obj"
	-@erase "$(INTDIR)\requestservice.obj"
	-@erase "$(INTDIR)\soap_quillC.obj"
	-@erase "$(INTDIR)\soap_quillServer.obj"
	-@erase "$(INTDIR)\soap_quillStub.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_quill.exe"
	-@erase "$(OUTDIR)\condor_quill.ilk"
	-@erase "$(OUTDIR)\condor_quill.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_quill.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=../Debug/condor_common.obj ..\Debug\condor_common_c.obj $(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) $(CONDOR_GLOBUS_LIB) $(CONDOR_GLOBUS_LIBPATH) $(CONDOR_OPENSSL_LIB) $(CONDOR_POSTGRESQL_LIB) $(CONDOR_OPENSSL_LIBPATH) $(CONDOR_POSTGRESQL_LIBPATH) /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\condor_quill.pdb" /debug /machine:I386 /out:"$(OUTDIR)\condor_quill.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\classadlogentry.obj" \
	"$(INTDIR)\classadlogparser.obj" \
	"$(INTDIR)\jobqueuecollection.obj" \
	"$(INTDIR)\jobqueuedbmanager.obj" \
	"$(INTDIR)\prober.obj" \
	"$(INTDIR)\quill_main.obj" \
	"$(INTDIR)\requestservice.obj" \
	"$(INTDIR)\soap_quillC.obj" \
	"$(INTDIR)\soap_quillServer.obj" \
	"$(INTDIR)\soap_quillStub.obj" \
	"$(OUTDIR)\condor_classad.lib" \
	"$(OUTDIR)\condor_cpp_util.lib" \
	"$(OUTDIR)\condor_daemon_core.lib" \
	"$(OUTDIR)\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib" \
	"$(OUTDIR)\condor_procapi.lib" \
	"$(OUTDIR)\condor_sysapi.lib"

"$(OUTDIR)\condor_quill.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_quill - Win32 Release"

OUTDIR=.\../Release
INTDIR=.\../Release
# Begin Custom Macros
OutDir=.\../Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\condor_quill.exe"

!ELSE 

ALL : "condor_sysapi - Win32 Release" "condor_procapi - Win32 Release" "condor_util_lib - Win32 Release" "condor_io - Win32 Release" "condor_daemon_core - Win32 Release" "condor_cpp_util - Win32 Release" "condor_classad - Win32 Release" "$(OUTDIR)\condor_quill.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"condor_classad - Win32 ReleaseCLEAN" "condor_cpp_util - Win32 ReleaseCLEAN" "condor_daemon_core - Win32 ReleaseCLEAN" "condor_io - Win32 ReleaseCLEAN" "condor_util_lib - Win32 ReleaseCLEAN" "condor_procapi - Win32 ReleaseCLEAN" "condor_sysapi - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\classadlogentry.obj"
	-@erase "$(INTDIR)\classadlogparser.obj"
	-@erase "$(INTDIR)\jobqueuecollection.obj"
	-@erase "$(INTDIR)\jobqueuedbmanager.obj"
	-@erase "$(INTDIR)\prober.obj"
	-@erase "$(INTDIR)\quill_main.obj"
	-@erase "$(INTDIR)\requestservice.obj"
	-@erase "$(INTDIR)\soap_quillC.obj"
	-@erase "$(INTDIR)\soap_quillServer.obj"
	-@erase "$(INTDIR)\soap_quillStub.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_quill.exe"
	-@erase "$(OUTDIR)\condor_quill.map"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_quill.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=../Release/condor_common.obj ../Release/condor_common_c.obj $(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) $(CONDOR_GLOBUS_LIB) $(CONDOR_GLOBUS_LIBPATH) $(CONDOR_OPENSSL_LIB) $(CONDOR_POSTGRESQL_LIB) $(CONDOR_OPENSSL_LIBPATH) $(CONDOR_POSTGRESQL_LIBPATH) /nologo /subsystem:console /pdb:none /map:"$(INTDIR)\condor_quill.map" /debug /machine:I386 /out:"$(OUTDIR)\condor_quill.exe" 
LINK32_OBJS= \
	"$(INTDIR)\classadlogentry.obj" \
	"$(INTDIR)\classadlogparser.obj" \
	"$(INTDIR)\jobqueuecollection.obj" \
	"$(INTDIR)\jobqueuedbmanager.obj" \
	"$(INTDIR)\prober.obj" \
	"$(INTDIR)\quill_main.obj" \
	"$(INTDIR)\requestservice.obj" \
	"$(INTDIR)\soap_quillC.obj" \
	"$(INTDIR)\soap_quillServer.obj" \
	"$(INTDIR)\soap_quillStub.obj" \
	"$(OUTDIR)\condor_classad.lib" \
	"$(OUTDIR)\condor_cpp_util.lib" \
	"$(OUTDIR)\condor_daemon_core.lib" \
	"$(OUTDIR)\condor_io.lib" \
	"..\src\condor_util_lib\condor_util.lib" \
	"$(OUTDIR)\condor_procapi.lib" \
	"$(OUTDIR)\condor_sysapi.lib"

"$(OUTDIR)\condor_quill.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_quill.dep")
!INCLUDE "condor_quill.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_quill.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_quill - Win32 Debug" || "$(CFG)" == "condor_quill - Win32 Release"

!IF  "$(CFG)" == "condor_quill - Win32 Debug"

"condor_classad - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_classad.mak CFG="condor_classad - Win32 Debug" 
   cd "."

"condor_classad - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_classad.mak CFG="condor_classad - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_quill - Win32 Release"

"condor_classad - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_classad.mak CFG="condor_classad - Win32 Release" 
   cd "."

"condor_classad - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_classad.mak CFG="condor_classad - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_quill - Win32 Debug"

"condor_cpp_util - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Debug" 
   cd "."

"condor_cpp_util - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_quill - Win32 Release"

"condor_cpp_util - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Release" 
   cd "."

"condor_cpp_util - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_cpp_util.mak CFG="condor_cpp_util - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_quill - Win32 Debug"

"condor_daemon_core - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_daemon_core.mak CFG="condor_daemon_core - Win32 Debug" 
   cd "."

"condor_daemon_core - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_daemon_core.mak CFG="condor_daemon_core - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_quill - Win32 Release"

"condor_daemon_core - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_daemon_core.mak CFG="condor_daemon_core - Win32 Release" 
   cd "."

"condor_daemon_core - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_daemon_core.mak CFG="condor_daemon_core - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_quill - Win32 Debug"

"condor_io - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Debug" 
   cd "."

"condor_io - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_quill - Win32 Release"

"condor_io - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Release" 
   cd "."

"condor_io - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_io.mak CFG="condor_io - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_quill - Win32 Debug"

"condor_util_lib - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Debug" 
   cd "."

"condor_util_lib - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_quill - Win32 Release"

"condor_util_lib - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Release" 
   cd "."

"condor_util_lib - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_util_lib.mak CFG="condor_util_lib - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_quill - Win32 Debug"

"condor_procapi - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_procapi.mak CFG="condor_procapi - Win32 Debug" 
   cd "."

"condor_procapi - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_procapi.mak CFG="condor_procapi - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_quill - Win32 Release"

"condor_procapi - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_procapi.mak CFG="condor_procapi - Win32 Release" 
   cd "."

"condor_procapi - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_procapi.mak CFG="condor_procapi - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

!IF  "$(CFG)" == "condor_quill - Win32 Debug"

"condor_sysapi - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_sysapi.mak CFG="condor_sysapi - Win32 Debug" 
   cd "."

"condor_sysapi - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_sysapi.mak CFG="condor_sysapi - Win32 Debug" RECURSE=1 CLEAN 
   cd "."

!ELSEIF  "$(CFG)" == "condor_quill - Win32 Release"

"condor_sysapi - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_sysapi.mak CFG="condor_sysapi - Win32 Release" 
   cd "."

"condor_sysapi - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\condor_sysapi.mak CFG="condor_sysapi - Win32 Release" RECURSE=1 CLEAN 
   cd "."

!ENDIF 

SOURCE=..\src\condor_quill\classadlogentry.C

"$(INTDIR)\classadlogentry.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_quill\classadlogparser.C

"$(INTDIR)\classadlogparser.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_quill\jobqueuecollection.C

"$(INTDIR)\jobqueuecollection.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_quill\jobqueuedbmanager.C

"$(INTDIR)\jobqueuedbmanager.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_quill\prober.C

"$(INTDIR)\prober.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_quill\quill_main.C

"$(INTDIR)\quill_main.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_quill\requestservice.C

"$(INTDIR)\requestservice.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_quill\soap_quillC.C

!IF  "$(CFG)" == "condor_quill - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_quillC.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_quill - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_quillC.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_quill\soap_quillServer.C

!IF  "$(CFG)" == "condor_quill - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_quillServer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_quill - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_quillServer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_quill\soap_quillStub.C

!IF  "$(CFG)" == "condor_quill - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_quillStub.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_quill - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\soap_quillStub.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 


!ENDIF 

