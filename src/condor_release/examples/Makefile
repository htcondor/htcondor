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

CC = gcc
C++ = g++
F77 = g77

all: c++ c fortran

c++: env.remote fstream.remote 

c: io.remote loop.remote registers.remote 

fortran: printer.remote reader.remote fortIO.remote

clean:
	rm *.remote *.out *.err *.log *.output core* tmp

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

