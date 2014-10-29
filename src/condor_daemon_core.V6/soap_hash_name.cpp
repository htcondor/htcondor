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

 
#include "condor_md.h"

#include <map>
#include <string>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

using namespace std;

const int HASHNAMELEN = 17;


namespace condor_soap {

typedef	map<string, string> hashmap;
typedef	map<string, hashmap> dirmap;


static const char *find_name_in_dir(const char *const dirname,
	const char *const hashname, dirmap &hash3dmap)
{
	const char *retval = NULL;
	DIR *dir;
	struct dirent *dir_entry;
	static char matchname[PATH_MAX + 1];
	matchname[0] = '\0';
	hashmap tmp2dmap;
	if ((dir = opendir(dirname)) != NULL) {
		while ((dir_entry = readdir(dir)) != NULL) {
			printf("file = %s/%s\n", dirname, dir_entry->d_name);
			unsigned char result[100];
			const char *const filenam = dir_entry->d_name;
			memcpy(result,
				Condor_MD_MAC::computeOnce((unsigned char *) filenam, strlen(filenam)),
				HASHNAMELEN);
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
			if (strcmp(entryhashname, hashname) == 0) {
				strcpy(matchname, filenam);
				printf("found match = %s\n", matchname);
				retval = matchname;
			}
			tmp2dmap.insert(pair<string, string>(entryhashname, filenam));
		}
	}
	printf("map size = %ld\n", tmp2dmap.size());
	if (tmp2dmap.size() > 0)
		hash3dmap.insert(pair<string, hashmap>(dirname, tmp2dmap));
	return (retval);
}


const char *get_plain_name(const char *const dirname, const char *const hashname)
{
	const char *retval = NULL;
	static dirmap hash3dmap;
	if (hash3dmap.count(dirname) > 0) {
		if (hash3dmap[dirname].count(hashname) > 0)
			retval = hash3dmap[dirname][hashname].c_str();
	} else retval = find_name_in_dir(dirname, hashname, hash3dmap);
	return (retval);
}

}

