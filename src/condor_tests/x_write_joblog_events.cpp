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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#ifdef WIN32
#include "condor_header_features.h"
#include "condor_sys_nt.h"
#include "condor_sys_types.h"
#else
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#endif
#include "write_user_log.h"
#include "my_username.h"


extern char *strnewp(const char *); // can't include this w/o including the world
struct hostent *NameEnt;

int writeSubmitEvent(WriteUserLog *log);
int writeExecuteEvent(WriteUserLog *log);
int writeJobTerminatedEvent(WriteUserLog *log);

int
main(int argc, char **argv)
{

	if(argc != 4) {
		printf("ussage: x_write_joblog_events log event count\n");
		exit(1);
	}

	char *logname = argv[1];
	int count = atoi(argv[3]);

	if( strcmp(argv[2],"submit") == 0) {
		//printf("Drop submit events\n");
		for(int cluster = 1;cluster <= count;cluster++) {
			WriteUserLog log;
			log.initialize(logname, cluster, 0, 0);
			writeSubmitEvent(&log);
		}
	} else if( strcmp(argv[2],"execute") == 0) {
		//printf("Drop execute event\n");
		for(int cluster = 1;cluster <= count;cluster++) {
			WriteUserLog log;
			log.initialize(logname, cluster, 0, 0);
			writeExecuteEvent(&log);
		}
	} else if( strcmp(argv[2],"terminated") == 0) {
		//printf("Drop terminated event\n");
		for(int cluster = 1;cluster <= count;cluster++) {
			WriteUserLog log;
			log.initialize(logname, cluster, 0, 0);
			writeJobTerminatedEvent(&log);
		}
	}
	exit(0);
}

int writeSubmitEvent(WriteUserLog *log)
{
	SubmitEvent submit;
	submit.setSubmitHost("<128.105.165.12:32779>");
	submit.submitEventLogNotes = strnewp("DAGMan info");
	submit.submitEventUserNotes = strnewp("User info");
	if ( !log->writeEvent(&submit) ) {
		printf("Bad submit write\n");
		exit(1);
	}
	return(0);
}

int writeExecuteEvent(WriteUserLog *log)
{
	ExecuteEvent execute;
	execute.setExecuteHost("<128.105.165.12:32779>");
	if ( !log->writeEvent(&execute) ) {
		printf("Bad execute write\n");
		exit(1);
	}
	return(0);
}

int writeJobTerminatedEvent(WriteUserLog *log)
{
	struct rusage ru;
	memset(&ru, 0, sizeof(ru));

	JobTerminatedEvent jobterminated;
	jobterminated.normal = true;
	jobterminated.signalNumber = 0;
	jobterminated.returnValue = 0;
	jobterminated.run_remote_rusage = ru;
	jobterminated.total_remote_rusage = ru;
	jobterminated.recvd_bytes = 200000;
	jobterminated.sent_bytes = 400000;
	jobterminated.total_recvd_bytes = 800000;
	jobterminated.total_sent_bytes = 900000;
	if ( !log->writeEvent(&jobterminated) ) {
	        printf("Bad jobterminate write\n");
			exit(1);
	}
	return(0);
}
