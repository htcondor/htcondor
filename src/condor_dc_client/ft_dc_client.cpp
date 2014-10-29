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
#include "file_transfer.h"


#include <map>
#include <limits.h>
#include <stdio.h>

using namespace std;

typedef map<string, unsigned char *> stringMap;


void getJobAd(ClassAd *const jobad, stringMap *const hashmap)
{
	const int MAXLNLEN = (PATH_MAX * 10) + 1;
	MyString jobadln;
	char filelist[MAXLNLEN];
	while (jobadln.readLine(stdin) && jobadln != "***") {
		if (jobadln.size() > 1) {	// More than just newline
			printf("line = %s.\n", jobadln.Value());
			if (jobadln[0] != '#' && jobad->Insert(jobadln.Value()) == false) {
				printf("ClassAd Insert failed. Exiting.\n");
				return;
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
						memcpy(result,
							Condor_MD_MAC::computeOnce((unsigned char *) filenam, strlen(filenam)),
							17);
						printf("output = ");
						for (int ind = 0; ind < 16; ++ind)
							printf("%x", result[ind]);
						printf("\n");
						hashmap->insert(pair<string, unsigned char *>(filenam, result));
					}
					printf("file = %s.\n", filenam);
				}
			}
		}
	}
	printf("line = %s.\n", jobadln.Value());
	printf("map size = %ld\n", hashmap->size());
	printf("classad size = %d\n", jobad->size());
}


int main( int argc, char **argv )
{
	ReliSock sock;
	if (argc != 2) {
		printf("Need sinful string\n");
		return (1);
	} else printf("opening = %s \n", argv[1]);
	sock.timeout(100);
	int retval = sock.connect(argv[1]);
	printf("connect = %d \n", retval);
	if (retval != 1)
		return (1);
	
	ClassAd jobad;
	stringMap hashmap;
	getJobAd(&jobad, &hashmap);

	/*
	Daemon server(DT_GENERIC, argv[1]); 
	printf("daemon state = %s \n", server.error());
	printf("startCommand = %d \n", server.startCommand(53,
		(Sock *) &s));
	printf("daemon state = %s \n", server.error());
*/

	FileTransfer filetrans;
	if (filetrans.SimpleInit(&jobad, false, false, &sock) == 0) {
		printf("Can't init filetrans.\n");
		return (1);
	}
	if (filetrans.UploadFiles(true, false) == 0) { // blocking, not final xfer
		printf("Can't upload.\n");
		return (1);
	}
/*
	sock.encode();
	printf("code = %d \n", sock.code(jobadsz));
	printf("eom = %d \n", sock.end_of_message());
	// Receive the result 
	sock.decode();
	printf("code = %d \n", sock.code(result));
	printf("eom = %d \n", sock.end_of_message());
	printf("Result = %d \n", result);
*/
	sock.close();
	return (0);
}
