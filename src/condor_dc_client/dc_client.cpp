/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

 
#include "daemon.h"
#include "condor_md.h"
#include "classad/classad.h"

#include <map>
#include <limits.h>
#include <libgen.h>
#include <stdio.h>

using namespace std;

const int HASHNAMELEN = 17;

int obsolete_main(int, char **)
{
	map<string, unsigned char *> hashmap;
	const int MAXLNLEN = (PATH_MAX * 10) + 1;
	ClassAd jobad;
	MyString jobadln;
	char filelist[MAXLNLEN];
	while (jobadln.readLine(stdin) && jobadln != "***") {
		if (jobadln.size() > 1) {	// More than just newline
			printf("line = %s.\n", jobadln.Value());
		}
	}
	printf("line = %s.\n", jobadln.Value());
	printf("map size = %ld\n", hashmap.size());
	printf("classad size = %d\n", jobad.size());
}



