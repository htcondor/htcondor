.SUFFIXES: .C
CC = gcc
CFLAGS = -g -DULTRIX43
CPlusPlus = g++
CPlusFlags = -g -Wall -DULTRIX43 
LD = /usr/psup/gcc/lib/gcc-lib/sparc-sun-sunos4.1.3/2.4.5/ld
LDFLAGS = -dc -e start -Bstatic
LIB_PATH = -L/usr/psup/gcc/lib/gcc-lib/sparc-sun-sunos4.1.3/2.4.5 -L/usr/psup/gcc/lib
LIBS = -lg++ -lgcc -lc -lg -lgcc
CRT0 = /lib/crt0.o

#
# Use this to explicitly link g++ programs
#
#	$(LD) $(LDFLAGS) -o test $(CRT0) $(LIB_PATH) $(OBJ) $(LIBS)

SRC = driver.C image.C
PACKAGE = image.o stubs.o machdep.o

.C.o:
	$(CPlusPlus) $(CPlusFlags) -c $<

all: test3

test: driver.o $(PACKAGE)
	$(CPlusPlus) -o test -static driver.o $(PACKAGE)

test2: driver2.o $(PACKAGE)
	$(CPlusPlus) -o test2 -static driver2.o $(PACKAGE)

test3: driver3.o $(PACKAGE)
	$(CPlusPlus) $(CPlusFlags) -o test3 -static driver3.o $(PACKAGE)

test4: driver4.o $(PACKAGE)
	$(CPlusPlus) $(CPlusFlags) -o test4 -static driver4.o $(PACKAGE)

display: display.o $(PACKAGE)
	$(CPlusPlus) $(CPlusFlags) -o display display.o $(PACKAGE)

test_data_off: test_data_off.o
	$(CC) $(CFLAGS) -o test_data_off test_data_off.o

test_setjmp: test_setjmp.o
	$(CC) $(CFLAGS) -o test_setjmp test_setjmp.o

process: process.o
	cc -o process process.o

foo: foo.o
	gcc -o foo foo.o

image.o: image.h machdep.h
driver.o: image.h machdep.h
