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

#Chirp Library Makefile
#
#This public domain software is provided "as is".  See the Chirp License
#for details.

.PHONY: all client java-client release clean dist-clean

client:
	$(MAKE) -C client
	mkdir -p lib
	cp client/libchirp_client.a lib

	mkdir -p include
	cp client/chirp_client.h include

java-client:
	$(MAKE) -C java/client

clean:
	$(MAKE) -C client clean
	$(MAKE) -C java/client clean

dist-clean: clean
	rm -f include/chirp_client.h
	rm -f lib/libchirp_client.a
	if [ -d include ]; then rmdir include ; fi
	if [ -d lib ]; then rmdir lib ; fi

