# Microsoft Developer Studio Generated NMAKE File, Based on condor_io.dsp
!IF "$(CFG)" == ""
CFG=condor_io - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_io - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_io - Win32 Debug" && "$(CFG)" != "condor_io - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_io.mak" CFG="condor_io - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_io - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_io - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "condor_io - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug
# Begin Custom Macros
OutDir=.\..\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_io.lib"


CLEAN :
	-@erase "$(INTDIR)\authentication.obj"
	-@erase "$(INTDIR)\buffers.obj"
	-@erase "$(INTDIR)\cedar_no_ckpt.obj"
	-@erase "$(INTDIR)\condor_auth.obj"
	-@erase "$(INTDIR)\condor_auth_anonymous.obj"
	-@erase "$(INTDIR)\condor_auth_claim.obj"
	-@erase "$(INTDIR)\condor_auth_kerberos.obj"
	-@erase "$(INTDIR)\condor_auth_sspi.obj"
	-@erase "$(INTDIR)\condor_crypt.obj"
	-@erase "$(INTDIR)\condor_crypt_3des.obj"
	-@erase "$(INTDIR)\condor_crypt_blowfish.obj"
	-@erase "$(INTDIR)\condor_rw.obj"
	-@erase "$(INTDIR)\condor_secman.obj"
	-@erase "$(INTDIR)\CryptKey.obj"
	-@erase "$(INTDIR)\errno_num.obj"
	-@erase "$(INTDIR)\open_flags.obj"
	-@erase "$(INTDIR)\reli_sock.obj"
	-@erase "$(INTDIR)\safe_sock.obj"
	-@erase "$(INTDIR)\SafeMsg.obj"
	-@erase "$(INTDIR)\sock.obj"
	-@erase "$(INTDIR)\sockCache.obj"
	-@erase "$(INTDIR)\stream.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_io.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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
	"$(INTDIR)\cedar_no_ckpt.obj" \
	"$(INTDIR)\condor_auth.obj" \
	"$(INTDIR)\condor_auth_anonymous.obj" \
	"$(INTDIR)\condor_auth_claim.obj" \
	"$(INTDIR)\condor_auth_kerberos.obj" \
	"$(INTDIR)\condor_auth_sspi.obj" \
	"$(INTDIR)\condor_crypt.obj" \
	"$(INTDIR)\condor_crypt_3des.obj" \
	"$(INTDIR)\condor_crypt_blowfish.obj" \
	"$(INTDIR)\condor_rw.obj" \
	"$(INTDIR)\condor_secman.obj" \
	"$(INTDIR)\CryptKey.obj" \
	"$(INTDIR)\errno_num.obj" \
	"$(INTDIR)\open_flags.obj" \
	"$(INTDIR)\reli_sock.obj" \
	"$(INTDIR)\safe_sock.obj" \
	"$(INTDIR)\SafeMsg.obj" \
	"$(INTDIR)\sock.obj" \
	"$(INTDIR)\sockCache.obj" \
	"$(INTDIR)\stream.obj"

"$(OUTDIR)\condor_io.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_io - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release
# Begin Custom Macros
OutDir=.\..\Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_io.lib"


CLEAN :
	-@erase "$(INTDIR)\authentication.obj"
	-@erase "$(INTDIR)\buffers.obj"
	-@erase "$(INTDIR)\cedar_no_ckpt.obj"
	-@erase "$(INTDIR)\condor_auth.obj"
	-@erase "$(INTDIR)\condor_auth_anonymous.obj"
	-@erase "$(INTDIR)\condor_auth_claim.obj"
	-@erase "$(INTDIR)\condor_auth_kerberos.obj"
	-@erase "$(INTDIR)\condor_auth_sspi.obj"
	-@erase "$(INTDIR)\condor_crypt.obj"
	-@erase "$(INTDIR)\condor_crypt_3des.obj"
	-@erase "$(INTDIR)\condor_crypt_blowfish.obj"
	-@erase "$(INTDIR)\condor_rw.obj"
	-@erase "$(INTDIR)\condor_secman.obj"
	-@erase "$(INTDIR)\CryptKey.obj"
	-@erase "$(INTDIR)\errno_num.obj"
	-@erase "$(INTDIR)\open_flags.obj"
	-@erase "$(INTDIR)\reli_sock.obj"
	-@erase "$(INTDIR)\safe_sock.obj"
	-@erase "$(INTDIR)\SafeMsg.obj"
	-@erase "$(INTDIR)\sock.obj"
	-@erase "$(INTDIR)\sockCache.obj"
	-@erase "$(INTDIR)\stream.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_io.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

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
	"$(INTDIR)\cedar_no_ckpt.obj" \
	"$(INTDIR)\condor_auth.obj" \
	"$(INTDIR)\condor_auth_anonymous.obj" \
	"$(INTDIR)\condor_auth_claim.obj" \
	"$(INTDIR)\condor_auth_kerberos.obj" \
	"$(INTDIR)\condor_auth_sspi.obj" \
	"$(INTDIR)\condor_crypt.obj" \
	"$(INTDIR)\condor_crypt_3des.obj" \
	"$(INTDIR)\condor_crypt_blowfish.obj" \
	"$(INTDIR)\condor_rw.obj" \
	"$(INTDIR)\condor_secman.obj" \
	"$(INTDIR)\CryptKey.obj" \
	"$(INTDIR)\errno_num.obj" \
	"$(INTDIR)\open_flags.obj" \
	"$(INTDIR)\reli_sock.obj" \
	"$(INTDIR)\safe_sock.obj" \
	"$(INTDIR)\SafeMsg.obj" \
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


!IF "$(CFG)" == "condor_io - Win32 Debug" || "$(CFG)" == "condor_io - Win32 Release"
SOURCE=..\src\condor_io\authentication.C

"$(INTDIR)\authentication.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\buffers.C

"$(INTDIR)\buffers.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\cedar_no_ckpt.C

"$(INTDIR)\cedar_no_ckpt.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\condor_auth.C

"$(INTDIR)\condor_auth.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\condor_auth_anonymous.C

"$(INTDIR)\condor_auth_anonymous.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\condor_auth_claim.C

"$(INTDIR)\condor_auth_claim.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\condor_auth_kerberos.C

!IF  "$(CFG)" == "condor_io - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_auth_kerberos.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_io - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_auth_kerberos.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_io\condor_auth_sspi.C

"$(INTDIR)\condor_auth_sspi.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\condor_crypt.C

"$(INTDIR)\condor_crypt.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\condor_crypt_3des.C

!IF  "$(CFG)" == "condor_io - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_crypt_3des.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_io - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\condor_common.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_crypt_3des.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_io\condor_crypt_blowfish.C

!IF  "$(CFG)" == "condor_io - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_crypt_blowfish.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_io - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c 

"$(INTDIR)\condor_crypt_blowfish.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_io\condor_rw.C

"$(INTDIR)\condor_rw.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\condor_secman.C

"$(INTDIR)\condor_secman.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\CryptKey.C

"$(INTDIR)\CryptKey.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\errno_num.C

"$(INTDIR)\errno_num.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\open_flags.c

"$(INTDIR)\open_flags.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\reli_sock.C

"$(INTDIR)\reli_sock.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\safe_sock.C

"$(INTDIR)\safe_sock.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\SafeMsg.C

"$(INTDIR)\SafeMsg.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\sock.C

"$(INTDIR)\sock.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\sockCache.C

"$(INTDIR)\sockCache.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_io\stream.C

"$(INTDIR)\stream.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

