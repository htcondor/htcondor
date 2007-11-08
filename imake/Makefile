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
CFLAGS = -g -I/usr/X/include -I/usr/X11/include -I/usr/X11R6/include -I/usr/include/X11 -I/usr/include/X -I/s/X11/include

imake: imake.o
	$(CC) -o imake imake.o

imake.o: ccimake imake.c
	$(CC) -c $(CFLAGS) `./ccimake` imake.c

ccimake: ccimake.c
	$(CC) -o ccimake $(CFLAGS) ccimake.c

clean:
	rm -f core *.o *~ ccimake imake.o imake
