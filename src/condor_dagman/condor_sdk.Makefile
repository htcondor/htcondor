#
#  Copyright 1986-2000 by the Condor Team
#
#  Permission to use, copy, modify, and distribute this software and its
#  documentation for any purpose and without fee is hereby granted,
#  provided that the above copyright notice appear in all copies and that
#  both that copyright notice and this permission notice appear in
#  supporting documentation, and that the names of the University of
#  Wisconsin and the Condor Design Team not be used in advertising or
#  publicity pertaining to distribution of the software without specific,
#  written prior permission.  The University of Wisconsin and the Condor
#  Design Team make no representations about the suitability of this
#  software for any purpose.  It is provided "as is" without express
#  or implied warranty.
#
#  THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
#  WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
#  OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
#  WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
#  OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
#  OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
#  OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
#  OR PERFORMANCE OF THIS SOFTWARE.
#

#  This Makefile is generated for some unknown machine

CC = gcc
C_LINK = gcc
CPlusPlus = g++
CC_LINK = g++
STATIC	= -static
STAR	= *

PLATFORM_LDFLAGS = __PLATFORM__LDFLAGS__

OS_FLAG = __WHAT__OS__IS__THIS__
ARCH_FLAG = __WHAT__ARCH__IS__THIS__

DEBUG_FLAG = -g

AUTH_FLAGS =

INCLUDE_DIR =		../../include

VENDOR_C_FLAGS =   -I$(INCLUDE_DIR) -D$(ARCH_FLAG)=$(ARCH_FLAG) -D$(OS_FLAG)=$(OS_FLAG) $(DEBUG_FLAG) $(SITE_C_FLAGS) $(AUTH_FLAGS)

STD_C_FLAGS = $(VENDOR_C_FLAGS) -D__WHAT__OS__VER__IS__THIS__=__WHAT__OS__VER__IS__THIS__
STD_C_PLUS_FLAGS = $(STD_C_FLAGS) -fno-implicit-templates

ALL_LDFLAGS = -lm $(LDFLAGS) $(PLATFORM_LDFLAGS) $(SITE_LDFLAGS)

SDK_LIB = ../../lib/libcondorsdk.a

.SUFFIXES: .C

.C.o:
	$(CPlusPlus) $(C_PLUS_FLAGS) -c $<

DEMANGLE = 2>&1 | c++filt

all:  condor_dagman

C_PLUS_FLAGS = $(STD_C_PLUS_FLAGS) -Wall
CFLAGS = $(STD_C_FLAGS) -Wall
LIB = -L../../lib -lcondorsdk

OBJ = dagman_main.o submit.o dag.o job.o util.o parse.o debug.o types.o 	script.o scriptQ.o dagman_instantiate.o

condor_dagman :  $(OBJ)
	$(CC_LINK) $(C_PLUS_FLAGS) -o  condor_dagman   $(OBJ)   $(LIB)  $(ALL_LDFLAGS) $(DEMANGLE)

clean::
	rm -f  condor_dagman   $(OBJ)

dagman_instantiate.o: dagman_instantiate.C
	$(CPlusPlus) $(STD_C_FLAGS) -c dagman_instantiate.C

ALWAYS:

