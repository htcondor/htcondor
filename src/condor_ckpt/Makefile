.SUFFIXES: .C
CC = gcc
CPlusPlus = g++
CPlusFlags = -g -Wall -DSUNOS41 -fwritable-strings
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
PACKAGE = image.o stubs.o

.C.o:
	$(CPlusPlus) $(CPlusFlags) -c $<

all: test test2

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

process: process.o
	cc -o process process.o

foo: foo.o
	gcc -o foo foo.o

image.o: image.h
driver.o: image.h
