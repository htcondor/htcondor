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
#include "soap_hash_name.h"

#include <map>
#include <limits.h>
#include <libgen.h>
#include <stdio.h>

using namespace std;

const int HASHNAMELEN = 17;

int main(int, char **)
{
	map<string, unsigned char *> hashmap;
	const int MAXLNLEN = (PATH_MAX * 10) + 1;
	ClassAd jobad;
	MyString jobadln;
	char filelist[MAXLNLEN];
	while (jobadln.readLine(stdin) && jobadln != "***") {
		if (jobadln.size() > 1) {	// More than just newline
			printf("line = %s.\n", jobadln.Value());
			if (jobadln[0] != '#' && jobad.Insert(jobadln.Value()) == false) {
				printf("ClassAd Insert failed. Exiting.\n");
				return (1);
			}
			if (strncmp(jobadln.Value(), "TransferInput =", 15) == 0) {
				if (sscanf(jobadln.Value(), "TransferInput = \"%s", filelist) == 1) {
					if (filelist[strlen(filelist) - 1] == '"')
						filelist[strlen(filelist) - 1] = '\0';
					printf("filelist = %s.\n", filelist);
					char filenam[PATH_MAX + 1], restlist[MAXLNLEN];
					restlist[0] = '\0';
					while(sscanf(filelist, "%[^,],%s", filenam, restlist) == 2 ||
						sscanf(filelist, "%[^,]", filenam) == 1) {
						printf("file = %s, rest = %s.\n", filenam, restlist);
						strcpy(filelist, restlist);
						restlist[0] = '\0';
						unsigned char result[100];
						char *basefile = basename(filenam);
						memcpy(result,
							Condor_MD_MAC::computeOnce((unsigned char *) basefile,
								strlen(basefile)),
							17);
						printf("output = ");
						char entryhashname[HASHNAMELEN * 2]; // 2 chars per hex byte
						entryhashname[0] = '\0';
						char letter[3];
						for (int ind = 0; ind < HASHNAMELEN - 1; ++ind) {
							printf("%x", result[ind]);
							sprintf(letter, "%x", result[ind]);
							strcat(entryhashname, letter);
						}
						printf("\n");
						hashmap.insert(pair<string, unsigned char *>(basefile, result));
						printf("match? = %s.\n",
							condor_soap::get_plain_name(dirname(filenam), entryhashname));
					}
					printf("file = %s.\n", filenam);
				}
			}
		}
	}
	printf("line = %s.\n", jobadln.Value());
	printf("map size = %ld\n", hashmap.size());
	printf("classad size = %d\n", jobad.size());
}



