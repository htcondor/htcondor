############################Copyright-DO-NOT-REMOVE-THIS-LINE##
#
# Condor Software Copyright Notice
# Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# This source code is covered by the Condor Public License, which can
# be found in the accompanying LICENSE.TXT file, or online at
# www.condorproject.org.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
# FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
# HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
# MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
# ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
# PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
# RIGHT.
#
############################Copyright-DO-NOT-REMOVE-THIS-LINE##

#  This Makefile is generated for some unknown machine


# Compiler flags
CC = gcc
C_LINK = gcc
CPlusPlus = g++
CC_LINK = g++
STATIC	= -static
STAR	= *

# Kerberos -- Un-Comment out the below line to enable
# KERBEROS = yes
ifdef KERBEROS
 KERBEROS_DIR = /s/krb5-1.2.1
 KERBEROS_INCLUDE = $(KERBEROS_DIR)/include
 KERBEROS_LIBS = krb5 k5crypto com_err
 KERBEROS_LIBDIR = $(KERBEROS_DIR)/lib
 KERBEROS_LINK = -L$(KERBEROS_LIBDIR) $(addprefix -l, $(KERBEROS_LIBS) )
 KERBEROS_LIB = $(addprefix $(KERBEROS_LIBDIR)/, $(KERBEROS_LIBS) )
 CREDENTIAL_LIB = $(KERBEROS_LIB)
endif

# SSL -- Comment out the below line to disable
# SSL = yes
ifdef SSL
 OpenSSLPath = /unsup/SSLeay-0.9.0b
 OpenSSLIncludePath = $(OpenSSLPath)/include
 OpenSSLLibPath = $(OpenSSLPath)/lib
 OpenSSLLibs = ssl crypto
 OpenSSLLink = -L$(OpenSSLLibPath) $(addprefix -l, $(OpenSSLLibs) )
 OPENSSL_LIB = $(addprefix $(OpenSSLLibPath)/, $(OpenSSLibs) )
endif

PLATFORM_LDFLAGS = __PLATFORM__LDFLAGS__

OS_FLAG = __WHAT__OS__IS__THIS__
ARCH_FLAG = __WHAT__ARCH__IS__THIS__

DEBUG_FLAG = -g

AUTH_FLAGS =

INCLUDE_DIR =		../../include

VENDOR_C_FLAGS =   -I$(INCLUDE_DIR) -D$(ARCH_FLAG)=$(ARCH_FLAG) -D$(OS_FLAG)=$(OS_FLAG) $(DEBUG_FLAG) $(SITE_C_FLAGS) $(AUTH_FLAGS)

STD_C_FLAGS = $(VENDOR_C_FLAGS) -D__WHAT__OS__VER__IS__THIS__=__WHAT__OS__VER__IS__THIS__
STD_C_PLUS_FLAGS = $(STD_C_FLAGS) -fno-implicit-templates

ALL_LDFLAGS = -lm $(LDFLAGS) $(PLATFORM_LDFLAGS) $(SITE_LDFLAGS) \
	$(OpenSSLLink) $(KERBEROS_LINK)

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

