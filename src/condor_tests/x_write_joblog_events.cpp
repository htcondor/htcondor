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

struct hostent *NameEnt;
/*
**#define NUL       "\0";
*/

int writeSubmitEvent(WriteUserLog *log);
int writeExecuteEvent(WriteUserLog *log);
int writeJobTerminatedEvent(WriteUserLog *log);
/*
int writeRemoteErrorEvent();
int writeExecutableErrorEvent();
int writeCheckpointedEvent();
int writeJobAbortedEvent();
int writeJobEvictedEvent();
int writeNodeTerminatedEvent();
int writePostScriptTerminatedEvent();
int writeGlobusSubmitEvent();
int writeGlobusSubmitFailedEvent();
int writeGlobusResourceUpEvent();
int writeGlobusResourceDownEvent();
int writeJobImageSizeEvent(); 
int writeShadowExceptionEvent(); 
int writeJobSuspendedEvent(); 
int writeJobUnsuspendedEvent(); 
int writeJobHeldEvent(); 
int writeJobReleasedEvent(); 
int writeNodeExecuteEvent(); 
*/

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
			WriteUserLog log("owner", NULL, logname, cluster, 0, 0, (bool)0, NULL);
			writeSubmitEvent(&log);
		}
	} else if( strcmp(argv[2],"execute") == 0) {
		//printf("Drop execute event\n");
		for(int cluster = 1;cluster <= count;cluster++) {
			WriteUserLog log("owner", NULL, logname, cluster, 0, 0, (bool)0, NULL);
			writeExecuteEvent(&log);
		}
	} else if( strcmp(argv[2],"terminated") == 0) {
		//printf("Drop terminated event\n");
		for(int cluster = 1;cluster <= count;cluster++) {
			WriteUserLog log("owner", NULL, logname, cluster, 0, 0, (bool)0, NULL);
			writeJobTerminatedEvent(&log);
		}
	}
	/*
	***********************************
	writeSubmitEvent();
	writeRemoteErrorEvent();
	writeExecuteEvent();
	writeExecutableErrorEvent();
	writeCheckpointedEvent();
	writeJobAbortedEvent();
	writeJobEvictedEvent();
	writeJobTerminatedEvent();
	writeNodeTerminatedEvent();
	writePostScriptTerminatedEvent();
	writeGlobusSubmitEvent();
	writeGlobusSubmitFailedEvent();
	writeGlobusResourceUpEvent();
	writeGlobusResourceDownEvent();
	writeJobImageSizeEvent(); 
	writeShadowExceptionEvent(); 
	writeJobSuspendedEvent(); 
	writeJobUnsuspendedEvent(); 
	writeJobHeldEvent(); 
	writeJobReleasedEvent(); 
	writeNodeExecuteEvent(); 
	***********************************
	*/
	exit(0);
}

int writeSubmitEvent(WriteUserLog *log)
{
	SubmitEvent submit;
	submit.setSubmitHost("<128.105.165.12:32779>");
	submit.submitEventLogNotes = strdup("DAGMan info");
	submit.submitEventUserNotes = strdup("User info");
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

/*
***************************************************
int writeExecutableErrorEvent(WriteUserLog *log)
{
	ExecutableErrorEvent executeerror;
	executeerror.errType = CONDOR_EVENT_BAD_LINK;
	if ( !log->writeEvent(&executeerror) ) {
		printf("Complain about bad executeerror write\n");
		exit(1);
	}
	return(0);
}

int writeCheckpointedEvent()
{
	CheckpointedEvent checkpoint;
	if ( !log.writeEvent(&checkpoint) ) {
		printf("Complain about bad checkpoint write\n");
		exit(1);
	}
	return(0);
}

int writeRemoteErrorEvent()
{
	RemoteErrorEvent remoteerror;
	remoteerror.setExecuteHost("<128.105.165.12:32779>");
	remoteerror.setDaemonName("<write job log test>");
	remoteerror.setErrorText("this is the write test error string");
	remoteerror.setCriticalError(TRUE);
	if ( !log.writeEvent(&remoteerror) ) {
	        printf("Complain about bad remoteerror write\n");
			exit(1);
	}
	return(0);
}

int writeJobAbortedEvent()
{
	JobAbortedEvent jobabort;
	jobabort.setReason("cause I said so!");
	if ( !log.writeEvent(&jobabort) ) {
	        printf("Complain about bad jobabort write\n");
			exit(1);
	}
	return(0);
}

int writeJobEvictedEvent()
{
	JobEvictedEvent jobevicted;
	jobevicted.setReason("It misbehaved!");
	jobevicted.setCoreFile("corefile");
	if ( !log.writeEvent(&jobevicted) ) {
	        printf("Complain about bad jobevicted write\n");
			exit(1);
	}
	return(0);
}

int writeNodeTerminatedEvent()
{
	struct rusage ru;

	NodeTerminatedEvent nodeterminated;
	nodeterminated.node = 44;
	nodeterminated.normal = false;
	nodeterminated.signalNumber = 9;
	nodeterminated.returnValue = 4;
	nodeterminated.run_remote_rusage = ru;
	nodeterminated.total_remote_rusage = ru;
	nodeterminated.recvd_bytes = 200000;
	nodeterminated.sent_bytes = 400000;
	nodeterminated.total_recvd_bytes = 800000;
	nodeterminated.total_sent_bytes = 900000;
	nodeterminated.setCoreFile( "badfilecore" );
	if ( !log.writeEvent(&nodeterminated) ) {
	        printf("Complain about bad nodeterminated write\n");
			exit(1);
	}
	return(0);
}

int writePostScriptTerminatedEvent()
{
	PostScriptTerminatedEvent postscriptterminated;
	postscriptterminated.normal = false;
	postscriptterminated.signalNumber = 9;
	postscriptterminated.returnValue = 4;
	if ( !log.writeEvent(&postscriptterminated) ) {
	        printf("Complain about bad postscriptterminated write\n");
			exit(1);
	}
	return(0);
}

int writeGlobusSubmitEvent()
{
	GlobusSubmitEvent globussubmitevent;
	globussubmitevent.rmContact = strdup("ResourceManager");;
	globussubmitevent.jmContact = strdup("JobManager");;
	globussubmitevent.restartableJM = true;
	if ( !log.writeEvent(&globussubmitevent) ) {
	        printf("Complain about bad globussubmitevent write\n");
			exit(1);
	}
	return(0);
}

int writeGlobusSubmitFailedEvent()
{
	GlobusSubmitFailedEvent globussubmitfailedevent;
	globussubmitfailedevent.reason = strdup("Cause it could");;
	if ( !log.writeEvent(&globussubmitfailedevent) ) {
	        printf("Complain about bad globussubmitfailedevent write\n");
			exit(1);
	}
	return(0);
}

int writeGlobusResourceUpEvent()
{
	GlobusResourceUpEvent globusresourceupevent;
	globusresourceupevent.rmContact = strdup("ResourceUp");;
	if ( !log.writeEvent(&globusresourceupevent) ) {
	        printf("Complain about bad globusresourceupevent write\n");
			exit(1);
	}
	return(0);
}

int writeGlobusResourceDownEvent()
{
	GlobusResourceDownEvent globusresourcedownevent;
	globusresourcedownevent.rmContact = strdup("ResourceDown");;
	if ( !log.writeEvent(&globusresourcedownevent) ) {
	        printf("Complain about bad globusresourcedownevent write\n");
			exit(1);
	}
	return(0);
}

int writeJobImageSizeEvent()
{
	JobImageSizeEvent jobimagesizeevent;
	jobimagesizeevent.image_size_kb = 128;
	if ( !log.writeEvent(&jobimagesizeevent) ) {
		printf("Complain about bad jobimagesizeevent write\n");
		exit(1);
	}
	return(0);
}


int writeShadowExceptionEvent() 
{
	ShadowExceptionEvent shadowexceptionevent;
	shadowexceptionevent.sent_bytes = 4096;
	shadowexceptionevent.recvd_bytes = 4096;
	shadowexceptionevent.message[0] = '\0';
	strncat(shadowexceptionevent.message,"shadow message", 15);
	if ( !log.writeEvent(&shadowexceptionevent) ) {
		printf("Complain about bad shadowexceptionevent write\n");
		exit(1);
	}
	return(0);
}


int writeJobSuspendedEvent()
{
	JobSuspendedEvent jobsuspendevent;
	jobsuspendevent.num_pids = 99;
	if ( !log.writeEvent(&jobsuspendevent) ) {
		printf("Complain about bad jobsuspendevent write\n");
		exit(1);
	}
	return(0);
}

int writeJobUnsuspendedEvent()
{
	JobUnsuspendedEvent jobunsuspendevent;
	//jobunsuspendevent.num_pids = 99;
	if ( !log.writeEvent(&jobunsuspendevent) ) {
		printf("Complain about bad jobunsuspendevent write\n");
		exit(1);
	}
	return(0);
}

int writeJobHeldEvent() 
{
	JobHeldEvent jobheldevent;
	jobheldevent.setReason("CauseWeCan");
	jobheldevent.setReasonCode(404);
	jobheldevent.setReasonSubCode(0xff);
	if ( !log.writeEvent(&jobheldevent) ) {
		printf("Complain about bad jobheldevent write\n");
		exit(1);
	}
	return(0);
}

int writeJobReleasedEvent() 
{
	JobReleasedEvent jobreleasedevent;
	jobreleasedevent.setReason("MessinWithYou");
	if ( !log.writeEvent(&jobreleasedevent) ) {
		printf("Complain about bad jobreleasedevent write\n");
		exit(1);
	}
	return(0);
}

int writeNodeExecuteEvent()
{
	NodeExecuteEvent nodeexecuteevent;
	nodeexecuteevent.node = 49;
	nodeexecuteevent.executeHost[0] = '\0';
	strcat(nodeexecuteevent.executeHost,"<128.105.165.12:32779>");
	if ( !log.writeEvent(&nodeexecuteevent) ) {
		printf("Complain about bad nodeexecuteevent write\n");
		exit(1);
	}
	return(0);
}
***************************************************
*/
