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
#include "MyString.h"
#include "directory.h"

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
	MyString NewHistoryFileName;

	FILE *OldLogFile = safe_fopen_wrapper( oldHistoryFileName, "r");
	if( !OldLogFile ) {
		fprintf(stderr, "History file (%s) not found.\n", oldHistoryFileName);
		exit(1);
	}

	NewHistoryFileName = oldHistoryFileName;
	NewHistoryFileName += ".new";

	FILE *NewLogFile = safe_fopen_wrapper( NewHistoryFileName.Value(), "w");
	if( !NewLogFile ) {
		fprintf(stderr, "Can't create new history file (%s).\n", NewHistoryFileName.Value());
		exit(1);
	}

    printf("Converting history file: %s...\n", oldHistoryFileName);

	int totalsum = 0, entrysum = 0,  entry_count = 0, tmploc = 0, found = 0;
	MyString Readbuf, Delimbuf, ClusterId, ProcId, Owner, CompletionDate;

	while( Readbuf.readLine(OldLogFile) == true ) {

		if( Readbuf.Length() == 0 ) continue;

		if( strncmp(Readbuf.Value(), HISTORY_DELIM, strlen(HISTORY_DELIM)) == 0 ) {
			// This line contains delimiter
			found = 1;

			Delimbuf.formatstr("%s Offset = %d ClusterId = %s ProcId = %s Owner = %s CompletionDate = %s\n", HISTORY_DELIM, entrysum, ClusterId.Value(), ProcId.Value(), Owner.Value(), CompletionDate.Value());
			fprintf(NewLogFile, "%s", Delimbuf.Value());

			entrysum = totalsum;
			totalsum += Delimbuf.Length();
			entry_count++;
		}else {
			totalsum += Readbuf.Length();
			fprintf(NewLogFile, "%s", Readbuf.Value());

			Readbuf.chomp();
			if( strncmp(Readbuf.Value(), CLUSTERID, strlen(CLUSTERID)) == 0 ) {
				tmploc = Readbuf.FindChar('=');
				ClusterId = Readbuf.Substr(tmploc+2, Readbuf.Length() -1);
			}else if( strncmp(Readbuf.Value(), PROCID, strlen(PROCID)) == 0 ) {
				tmploc = Readbuf.FindChar('=');
				ProcId = Readbuf.Substr(tmploc+2, Readbuf.Length() -1);
			}else if( strncmp(Readbuf.Value(), OWNER, strlen(OWNER)) == 0 ) {
				tmploc = Readbuf.FindChar('=');
				Owner = Readbuf.Substr(tmploc+2, Readbuf.Length() -1);
			}else if( strncmp(Readbuf.Value(), COMPLETIONDATE, strlen(COMPLETIONDATE)) == 0 ) {
				tmploc = Readbuf.FindChar('=');
				CompletionDate = Readbuf.Substr(tmploc+2, Readbuf.Length() -1);
			}
		}
	}

	fclose(OldLogFile);
	fclose(NewLogFile);

	if( found == 0 ) {
		fprintf(stderr, "Invalid History File: (%s)\n", oldHistoryFileName);
		unlink(NewHistoryFileName.Value());
	} else {
		MyString TmpBuf;
		TmpBuf = oldHistoryFileName;
		TmpBuf += ".oldver";

		rename( oldHistoryFileName, TmpBuf.Value());
		rename( NewHistoryFileName.Value(), oldHistoryFileName);
	}
}
