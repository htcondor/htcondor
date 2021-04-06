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

#include "condor_common.h"
#include "directory.h"
#include "stl_string_utils.h"

#define HISTORY_DELIM	"***"
#define CLUSTERID	"ClusterId"
#define PROCID		"ProcId"
#define OWNER		"Owner"
#define COMPLETIONDATE	"CompletionDate"

static void usage(char* name);
static void convertHistoryFile(const char *oldHistoryFileName);

int
main(int argc, char* argv[])
{
	int i;

    if (argc < 2) {
        usage(argv[0]);
        exit(1);
    }
	for( i=1; i < argc; i++) {
        if (strcmp(argv[i],"-help")==0) {
			usage(argv[0]);
            exit(0);
		}
	}

	for( i=1; i < argc; i++) {
        convertHistoryFile(argv[i]);
	}

    printf("\nThe original history file%s renamed to end in '.oldver'\n",
           (argc < 3) ? " was" : "s were");
    printf("We recommend not leaving these in the Condor spool directory.\n");
    
	return 0;
}


static void 
usage(char* name) 
{
	printf("Usage: %s [-help] <list of history files>\n",name);
}

static void
convertHistoryFile(const char *oldHistoryFileName)
{
	std::string NewHistoryFileName;

	FILE *OldLogFile = safe_fopen_wrapper( oldHistoryFileName, "r");
	if( !OldLogFile ) {
		fprintf(stderr, "History file (%s) not found.\n", oldHistoryFileName);
		exit(1);
	}

	NewHistoryFileName = oldHistoryFileName;
	NewHistoryFileName += ".new";

	FILE *NewLogFile = safe_fopen_wrapper( NewHistoryFileName.c_str(), "w");
	if( !NewLogFile ) {
		fprintf(stderr, "Can't create new history file (%s).\n", NewHistoryFileName.c_str());
		exit(1);
	}

    printf("Converting history file: %s...\n", oldHistoryFileName);

	int totalsum = 0, entrysum = 0,  entry_count = 0, tmploc = 0, found = 0;
	std::string Readbuf, Delimbuf, ClusterId, ProcId, Owner, CompletionDate;

	while( readLine(Readbuf, OldLogFile) == true ) {

		if( Readbuf.length() == 0 ) continue;

		if( strncmp(Readbuf.c_str(), HISTORY_DELIM, strlen(HISTORY_DELIM)) == 0 ) {
			// This line contains delimiter
			found = 1;

			formatstr(Delimbuf,"%s Offset = %d ClusterId = %s ProcId = %s Owner = %s CompletionDate = %s\n", HISTORY_DELIM, entrysum, ClusterId.c_str(), ProcId.c_str(), Owner.c_str(), CompletionDate.c_str());
			fprintf(NewLogFile, "%s", Delimbuf.c_str());

			entrysum = totalsum;
			totalsum += Delimbuf.length();
			entry_count++;
		}else {
			totalsum += Readbuf.length();
			fprintf(NewLogFile, "%s", Readbuf.c_str());

			chomp(Readbuf);
			if( strncmp(Readbuf.c_str(), CLUSTERID, strlen(CLUSTERID)) == 0 ) {
				tmploc = Readbuf.find('=');
				ClusterId = Readbuf.substr(tmploc+2, Readbuf.length());
			}else if( strncmp(Readbuf.c_str(), PROCID, strlen(PROCID)) == 0 ) {
				tmploc = Readbuf.find('=');
				ProcId = Readbuf.substr(tmploc+2, Readbuf.length());
			}else if( strncmp(Readbuf.c_str(), OWNER, strlen(OWNER)) == 0 ) {
				tmploc = Readbuf.find('=');
				Owner = Readbuf.substr(tmploc+2, Readbuf.length());
			}else if( strncmp(Readbuf.c_str(), COMPLETIONDATE, strlen(COMPLETIONDATE)) == 0 ) {
				tmploc = Readbuf.find('=');
				CompletionDate = Readbuf.substr(tmploc+2, Readbuf.length());
			}
		}
	}

	fclose(OldLogFile);
	fclose(NewLogFile);

	if( found == 0 ) {
		fprintf(stderr, "Invalid History File: (%s)\n", oldHistoryFileName);
		unlink(NewHistoryFileName.c_str());
	} else {
		std::string TmpBuf;
		TmpBuf = oldHistoryFileName;
		TmpBuf += ".oldver";

		rename( oldHistoryFileName, TmpBuf.c_str());
		rename( NewHistoryFileName.c_str(), oldHistoryFileName);
	}
}
