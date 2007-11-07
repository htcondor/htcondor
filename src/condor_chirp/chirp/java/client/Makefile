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

#Chirp Java Client Makefile
#
#This public domain software is provided "as is".  See the Chirp License
#for details.

JFILES = \
ChirpConfig.java \
ChirpClient.java \
ChirpInputStream.java \
ChirpOutputStream.java

Chirp.jar: $(JFILES)
	javac $(JFILES)
	mkdir -p edu/wisc/cs/condor/chirp
	mv *.class edu/wisc/cs/condor/chirp
	jar cf Chirp.jar edu
	rm -rf edu

clean:
	rm -f Chirp.jar


