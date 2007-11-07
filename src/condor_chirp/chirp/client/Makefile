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

#Chirp C Client Makefile
#
#This public domain software is provided "as is".  See the Chirp License
#for details.

OFILES = chirp_client.o

CPPFLAGS = -I ../common
CFLAGS = -fPIC

ifdef DEBUG_CHIRP
	CFLAGS += -D DEBUG_CHIRP
endif

libchirp_client.a: $(OFILES)
	$(AR) -rcs libchirp_client.a $(OFILES)

clean:
	rm -f *.o libchirp_client.a

%.o: %.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $< -o $@
