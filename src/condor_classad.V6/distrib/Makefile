GCC 	= g++
LIB_DIR = ../lib
INC_DIR = ../include
LIB		= $(LIB_DIR)/libclassads.a
FLG 	= -g -Wall -I$(INC_DIR)

all: cxi 

cxi: cxi.o
	$(GCC) $(FLG) -c cxi.C 
	
cxi.o: cxi.C
	$(GCC) $(FLG) -o cxi cxi.o $(LIB)

clean:
	rm -f *.o core *~ cxi 
