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
