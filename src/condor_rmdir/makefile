debug = 0
use_libctiny = 1

clcommon = /FC /Gd /GS- /D_CRT_SECURE_NO_WARNINGS /D_WIN32_WINNT=_WIN32_WINNT_WIN2K /DWIN32_LEAN_AND_MEAN
clopts = /O1

crtc = /MD
BUILD_TYPE=RelWithDebInfo
OBJ_DIR=obj
!if "$(debug)"=="1"
crtc = /MDd
clopts = /Zi /D_DEBUG /DDEBUG
BUILD_TYPE=Debug
OBJ_DIR=objd
!endif

# our local source files and includes
#
SRCS = condor_rmdir.cpp main.cpp
INCS = common.h condor_rmdir.h bprint.h harylist.h

crtlink =
!if "$(use_libctiny)"=="1"
crtc    = /DUSE_LIBCTINY=1
crtlink = /nodefaultlib:libc.lib /nodefaultlib:libcmt.lib $(OBJ_DIR)\libctiny.lib
!endif

# overall goal of the makefile, this must be the first target.
#
goal: prepare condor_rmdir.exe

# before we build, we should create necessary working directories
#
prepare: create_dirs

create_dirs:
   if not exist $(OBJ_DIR)\NUL md $(OBJ_DIR)
#   if not exist inc\NUL md inc

$(OBJ_DIR)\libctiny.lib:
   nmake -f libctiny.mak

condor_rmdir.exe : $(SRCS) $(INCS) makefile. $(OBJ_DIR)\libctiny.lib
    cl /nologo $(clcommon) $(clopts) $(crtc) $(SRCS) \
    /link /out:$*.exe /map:$*.map $(crtlink)
