CC = gcc
C++ = g++
F77 = g77

all: c++ c fortran

c++: env.remote fstream.remote 

c: io.remote loop.remote registers.remote 

fortran: printer.remote reader.remote fortIO.remote

clean:
	/bin/rm *.remote *.out *.err *.log *.output core* tmp

env.remote: env.C
	condor_compile $(C++) -o env.remote env.C

fstream.remote: fstream.C
	condor_compile $(C++) -o fstream.remote fstream.C

io.remote: io.c
	condor_compile $(CC) -o io.remote io.c

loop.remote: loop.c
	condor_compile $(CC) -o loop.remote loop.c

registers.remote: registers.c
	condor_compile $(CC) -o registers.remote registers.c

reader.remote: reader.f
	condor_compile $(F77) -o reader.remote reader.f

printer.remote: printer.f
	condor_compile $(F77) -o printer.remote printer.f

fortIO.remote: fortIO.f
	condor_compile $(F77) -o fortIO.remote fortIO.f

