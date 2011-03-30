#**************************************************************
#
# Copyright (C) 2011, Condor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you
# may not use this file except in compliance with the License.  You may
# obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#**************************************************************#

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

condor_rmdir.exe : $(SRCS) $(INCS) makefile.mak $(OBJ_DIR)\libctiny.lib
    cl /nologo $(clcommon) $(clopts) $(crtc) $(SRCS) \
    /link /out:$*.exe /map:$*.map $(crtlink)
