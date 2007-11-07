##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

PVM_ROOT = 	/p/condor/workspaces/pvm/pvm3.4.2
# ALPHA, HPPA, LINUX, SGI6, SUN4SOL2, X86SOL2, etc.
PVM_ARCH =	X86SOL2

ARCHCFLAGS = 
#Needed on Solaris
ARCHLIB	= -lsocket -lnsl
#ARCHLIB = 

CC	=	gcc
OPTIONS	=	-O 
CFLAGS	=	$(OPTIONS) -I$(PVM_ROOT)/include $(ARCHCFLAGS)
LIBS	=	-lpvm3 $(ARCHLIB)
GLIBS	=	-lgpvm3

F77	=	f77
FFLAGS	=	-g 
FLIBS	=	-lfpvm3

GFLAGS  = 	-g

LFLAGS	=	-L$(PVM_ROOT)/lib/$(PVM_ARCH)


all   : master_sum worker_sum

master_sum : master_sum.c
	$(CC) $(CFLAGS) $(LFLAGS) $(GFLAGS) master_sum.c $(LIBS) -o master_sum

worker_sum : worker_sum.c
	$(CC) $(CFLAGS) $(LFLAGS)  $(GFLAGS) worker_sum.c  $(LIBS) -o worker_sum

clean:
	rm -f master_sum worker_sum out_* err_*
