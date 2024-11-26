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

all: random.condor half.condor sum.condor

random.condor: random.c
	gcc -o random.condor random.c

half.condor: half.c
	gcc -o half.condor half.c

sum.condor: sum.c
	gcc -o sum.condor sum.c

clean:
	rm -f *.out *.err *.log diamond.dag.* *.condor *~ 
