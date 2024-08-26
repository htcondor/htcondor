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
#include "classad/classad_distribution.h"
#include "my_username.h"

struct hostent *NameEnt;

static void simulateUsage(struct rusage &ru) {
	memset(&ru, 0, sizeof(ru));
	ru.ru_utime.tv_sec = 93784;
	ru.ru_stime.tv_sec = 454028;
}

int writeSubmitEvent(WriteUserLog &logFile)
{
	SubmitEvent submit;
	submit.setSubmitHost("<128.105.165.12:32779>");
	submit.submitEventLogNotes = "DAGMan info";
	submit.submitEventUserNotes = "User info";
	if ( !logFile.writeEvent(&submit) ) {
		printf("Complain about bad submit write\n");
		exit(1);
	}
	return(0);
}

int writeRemoteErrorEvent(WriteUserLog &logFile)
{
	RemoteErrorEvent remoteerror;
	remoteerror.execute_host = "<128.105.165.12:32779>";
	remoteerror.daemon_name = "<write job log test>";
	remoteerror.error_str = "this is the write test error string";
	remoteerror.setCriticalError(true);
	if ( !logFile.writeEvent(&remoteerror) ) {
	        printf("Complain about bad remoteerror write\n");
			exit(1);
	}
	return(0);
}

int writeGenericEvent(WriteUserLog & logFile)
{
	GenericEvent gen;
	gen.setInfoText("Lorem ipsum dolor sit amet, in duo prima aeque principes euismod.");
	if ( !logFile.writeEvent(&gen) ) {
		printf("Complain about bad generic write\n");
		exit(1);
	}
	return(0);
}

int writeExecuteEvent(WriteUserLog &logFile)
{
	ExecuteEvent execute;
	execute.setExecuteHost("<128.105.165.12:32779>");
	if ( !logFile.writeEvent(&execute) ) {
		printf("Complain about bad execute write\n");
		exit(1);
	}
	return(0);
}

int writeExecutableErrorEvent(WriteUserLog &logFile)
{
	ExecutableErrorEvent executeerror;
	executeerror.errType = CONDOR_EVENT_BAD_LINK;
	if ( !logFile.writeEvent(&executeerror) ) {
		printf("Complain about bad executeerror write\n");
		exit(1);
	}
	return(0);
}

int writeCheckpointedEvent(WriteUserLog &logFile)
{
	CheckpointedEvent checkpoint;
	checkpoint.sent_bytes = 11;
	rusage ru;
	simulateUsage(ru);
	checkpoint.run_local_rusage = ru;
	checkpoint.run_remote_rusage = ru;
	if ( !logFile.writeEvent(&checkpoint) ) {
		printf("Complain about bad checkpoint write\n");
		exit(1);
	}
	return(0);
}


int writeJobAbortedEvent(WriteUserLog &logFile)
{
	JobAbortedEvent jobabort;
	jobabort.setReason("cause I said so!");
	if ( !logFile.writeEvent(&jobabort) ) {
	        printf("Complain about bad jobabort write\n");
			exit(1);
	}
	return(0);
}

int writeJobEvictedEvent(WriteUserLog &logFile)
{
	JobEvictedEvent jobevicted;
	jobevicted.setReason("It misbehaved!");
	jobevicted.core_file = "corefile";
	rusage ru;
	simulateUsage(ru);
	jobevicted.run_local_rusage = ru;
	jobevicted.run_remote_rusage = ru;
	jobevicted.sent_bytes = 1;
	jobevicted.recvd_bytes = 2;
	jobevicted.terminate_and_requeued = true;
	jobevicted.normal = true;
	jobevicted.checkpointed = false;
	if ( !logFile.writeEvent(&jobevicted) ) {
	        printf("Complain about bad jobevicted write\n");
			exit(1);
	}
	return(0);
}

int writeJobTerminatedEvent(WriteUserLog &logFile)
{
	struct rusage ru;
	simulateUsage(ru);

	JobTerminatedEvent jobterminated;
	jobterminated.normal = false;
	jobterminated.signalNumber = 9;
	jobterminated.returnValue = 4;
	jobterminated.run_remote_rusage = ru;
	jobterminated.total_remote_rusage = ru;
	// Should local usage always be 0?
	jobterminated.run_local_rusage = ru;
	jobterminated.total_local_rusage = ru;
	jobterminated.recvd_bytes = 200000;
	jobterminated.sent_bytes = 400000;
	jobterminated.total_recvd_bytes = 800000;
	jobterminated.total_sent_bytes = 900000;
	jobterminated.core_file = "badfilecore";
	jobterminated.normal = false;
	if ( !logFile.writeEvent(&jobterminated) ) {
	        printf("Complain about bad jobterminated write\n");
			exit(1);
	}
	return(0);
}

int writeNodeTerminatedEvent(WriteUserLog &logFile)
{
	struct rusage ru;
	simulateUsage(ru);

	classad::ClassAd use;
	use.InsertAttr("RequestMemory", 44);
	use.InsertAttr("Memory", 55);
	use.InsertAttr("MemoryUsage", 33);

	use.InsertAttr("RequestPets", 1);
	use.InsertAttr("Pets", 1);
	use.InsertAttr("PetsUsage", 0.5);
	use.InsertAttr("AssignedPets", "Spot");

	NodeTerminatedEvent nodeterminated;
	nodeterminated.node = 44;
	nodeterminated.normal = false;
	nodeterminated.signalNumber = 9;
	nodeterminated.returnValue = 4;
	nodeterminated.run_remote_rusage = ru;
	nodeterminated.total_remote_rusage = ru;
	// Should local usage always be 0?
	nodeterminated.run_local_rusage = ru;
	nodeterminated.total_local_rusage = ru;
	nodeterminated.recvd_bytes = 200000;
	nodeterminated.sent_bytes = 400000;
	nodeterminated.total_recvd_bytes = 800000;
	nodeterminated.total_sent_bytes = 900000;
	nodeterminated.core_file = "badfilecore";

	nodeterminated.initUsageFromAd(use);

	if ( !logFile.writeEvent(&nodeterminated) ) {
	        printf("Complain about bad nodeterminated write\n");
			exit(1);
	}
	return(0);
}

int writePostScriptTerminatedEvent(WriteUserLog &logFile)
{
	PostScriptTerminatedEvent postscriptterminated;
	postscriptterminated.normal = false;
	postscriptterminated.signalNumber = 9;
	postscriptterminated.returnValue = 4;
	if ( !logFile.writeEvent(&postscriptterminated) ) {
	        printf("Complain about bad postscriptterminated write\n");
			exit(1);
	}
	return(0);
}

int writeJobImageSizeEvent(WriteUserLog &logFile)
{
	JobImageSizeEvent jobimagesizeevent;
	jobimagesizeevent.image_size_kb = 128;
	jobimagesizeevent.resident_set_size_kb = 129;
	jobimagesizeevent.proportional_set_size_kb = 130;
	jobimagesizeevent.memory_usage_mb = 131;
	if ( !logFile.writeEvent(&jobimagesizeevent) ) {
		printf("Complain about bad jobimagesizeevent write\n");
		exit(1);
	}
	return(0);
}


int writeShadowExceptionEvent(WriteUserLog &logFile)
{
	ShadowExceptionEvent shadowexceptionevent;
	shadowexceptionevent.sent_bytes = 4096;
	shadowexceptionevent.recvd_bytes = 4096;
	shadowexceptionevent.setMessage("shadow message");
	if ( !logFile.writeEvent(&shadowexceptionevent) ) {
		printf("Complain about bad shadowexceptionevent write\n");
		exit(1);
	}
	return(0);
}


int writeJobSuspendedEvent(WriteUserLog &logFile)
{
	JobSuspendedEvent jobsuspendevent;
	jobsuspendevent.num_pids = 99;
	if ( !logFile.writeEvent(&jobsuspendevent) ) {
		printf("Complain about bad jobsuspendevent write\n");
		exit(1);
	}
	return(0);
}

int writeJobUnsuspendedEvent(WriteUserLog &logFile)
{
	JobUnsuspendedEvent jobunsuspendevent;
	//jobunsuspendevent.num_pids = 99;
	if ( !logFile.writeEvent(&jobunsuspendevent) ) {
		printf("Complain about bad jobunsuspendevent write\n");
		exit(1);
	}
	return(0);
}

int writeJobHeldEvent(WriteUserLog &logFile)
{
	JobHeldEvent jobheldevent;
	jobheldevent.setReason("CauseWeCan");
	jobheldevent.code = 404;
	jobheldevent.subcode = 0xff;
	if ( !logFile.writeEvent(&jobheldevent) ) {
		printf("Complain about bad jobheldevent write\n");
		exit(1);
	}
	return(0);
}

int writeJobReleasedEvent(WriteUserLog &logFile)
{
	JobReleasedEvent jobreleasedevent;
	jobreleasedevent.setReason("MessinWithYou");
	if ( !logFile.writeEvent(&jobreleasedevent) ) {
		printf("Complain about bad jobreleasedevent write\n");
		exit(1);
	}
	return(0);
}

int writeNodeExecuteEvent(WriteUserLog &logFile)
{
	NodeExecuteEvent nodeexecuteevent;
	nodeexecuteevent.node = 49;
	nodeexecuteevent.executeHost = "<128.105.165.12:32779>";
	if ( !logFile.writeEvent(&nodeexecuteevent) ) {
		printf("Complain about bad nodeexecuteevent write\n");
		exit(1);
	}
	return(0);
}

int writeJobDisconnectedEvent(WriteUserLog &logFile)
{
	JobDisconnectedEvent evt;
	evt.startd_name = "ThatMachine";
	evt.setDisconnectReason("TL;DR");
	evt.startd_addr = "<128.105.165.12:32780>";
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad JobDisconnectedEvent write\n");
		exit(1);
	}
	return(0);
}

int writeJobReconnectedEvent(WriteUserLog &logFile)
{
	JobReconnectedEvent evt;
	evt.startd_addr = "<128.105.165.12:32779>";
	evt.startd_name = "ThatMachine";
	evt.starter_addr = "<128.105.165.12:32780>";
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad JobReconnectedEvent write\n");
		exit(1);
	}
	return(0);
}

int writeJobReconnectFailedEvent(WriteUserLog &logFile)
{
	JobReconnectFailedEvent evt;
	evt.startd_name = "ThatMachine";
	evt.setReason("The're just not into you");
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad JobReconnectFailedEvent write\n");
		exit(1);
	}
	return(0);
}

int writeGridResourceUpEvent(WriteUserLog &logFile)
{
	GridResourceUpEvent evt;
	evt.resourceName = "Resource Name";
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad GridResourceUpEvent write\n");
		exit(1);
	}
	return(0);
}

int writeGridResourceDownEvent(WriteUserLog &logFile)
{
	GridResourceDownEvent evt;
	evt.resourceName = "Resource Name";
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad GridResourceDownEvent write\n");
		exit(1);
	}
	return(0);
}

int writeGridSubmitEvent(WriteUserLog &logFile)
{
	GridSubmitEvent evt;
	evt.resourceName = "Resource Name";
	evt.jobId = "100.1";
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad GridSubmitEvent write\n");
		exit(1);
	}
	return(0);
}

int writeJobAdInformationEvent(WriteUserLog &logFile)
{
	JobAdInformationEvent evt;
	evt.Assign("JobStatus", 2);
	evt.Assign("BILLBool", true);
	evt.Assign("BILLInt", 1000);
	evt.Assign("BILLReal", 66.66);
	evt.Assign("BillString", "lorem ipsum dolor");
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad JobAdInformationEvent write\n");
		exit(1);
	}
	return(0);
}

int writeJobStatusUnknownEvent(WriteUserLog &logFile)
{
	JobStatusUnknownEvent evt;
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad JobStatusUnknownEvent write\n");
		exit(1);
	}
	return(0);
}

int writeJobStatusKnownEvent(WriteUserLog &logFile)
{
	JobStatusKnownEvent evt;
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad JobStatusKnownEvent write\n");
		exit(1);
	}
	return(0);
}

int writeJobStageInEvent(WriteUserLog &logFile)
{
	JobStageInEvent evt;
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad JobStageInEvent write\n");
		exit(1);
	}
	return(0);
}

int writeJobStageOutEvent(WriteUserLog &logFile)
{
	JobStageOutEvent evt;
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad JobStageOutEvent write\n");
		exit(1);
	}
	return(0);
}

int writeAttributeUpdateEvent(WriteUserLog &logFile)
{
	AttributeUpdate evt;
	evt.setName("PrivateAttr");
	evt.setValue("1");
	evt.setOldValue("0");
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad AttributeUpdate write\n");
		exit(1);
	}
	return(0);
}

int writePreSkipEvent(WriteUserLog &logFile)
{
	PreSkipEvent evt;
	evt.skipEventLogNotes = "DAGMan info";
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad PreSkipEvent write\n");
		exit(1);
	}
	return(0);
}

int writeClusterSubmitEvent(WriteUserLog &logFile)
{
	ClusterSubmitEvent evt;
	evt.setSubmitHost("<128.105.165.12:32779>");
	evt.submitEventLogNotes = "DAGMan info";
	evt.submitEventUserNotes = "User info";
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad Cluster Submit Event write\n");
		exit(1);
	}
	return(0);
}

int writeClusterRemoveEvent(WriteUserLog &logFile)
{
	ClusterRemoveEvent evt;
	evt.next_proc_id = 100;
	evt.next_row = 10;
	evt.completion = ClusterRemoveEvent::CompletionCode::Complete;
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad Cluster Remove Event write\n");
		exit(1);
	}
	return(0);
}

int writeFactoryPausedEvent(WriteUserLog &logFile)
{
	FactoryPausedEvent evt;
	evt.setReason("Hang on a second");
	evt.setPauseCode(42);
	evt.setHoldCode(24);
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad FactoryPausedEvent write\n");
		exit(1);
	}
	return(0);
}

int writeFactoryResumedEvent(WriteUserLog &logFile)
{
	FactoryResumedEvent evt;
	evt.setReason("just messin wit' ya");
	if ( !logFile.writeEvent(&evt) ) {
		printf("Complain about bad FactoryResumedEvent write\n");
		exit(1);
	}
	return(0);
}

int
main(int argc, const char * argv[])
{
	const char * logname = "local.log";
	if (argc > 1) { logname = argv[1]; }

	// The cluster and proc IDs are arbitrary but should be non-zero.  The
	// subproc ID is always (and must be) zero.
	WriteUserLog logFile;
	logFile.initialize(logname, 14, 55, 0);
	// FIXME: at some point we'll have to actually test all the various
	// options and make sure that they work.
	// logFile.initialize(logname, 14, 55, 0, ULogEvent::formatOpt::ISO_DATE | ULogEvent::formatOpt::UTC | ULogEvent::formatOpt::SUB_SECOND );

	writeSubmitEvent(logFile);
	writeExecuteEvent(logFile);
	writeExecutableErrorEvent(logFile);
	writeCheckpointedEvent(logFile);
	writeJobEvictedEvent(logFile);
	writeJobTerminatedEvent(logFile);
	writeJobImageSizeEvent(logFile);
	writeShadowExceptionEvent(logFile);
	writeGenericEvent(logFile);
	writeJobAbortedEvent(logFile);
	writeJobSuspendedEvent(logFile);
	writeJobUnsuspendedEvent(logFile);
	writeJobHeldEvent(logFile);
	writeJobReleasedEvent(logFile);
	writeNodeExecuteEvent(logFile);
	writeNodeTerminatedEvent(logFile);
	writePostScriptTerminatedEvent(logFile);
	writeRemoteErrorEvent(logFile);

	writeJobDisconnectedEvent(logFile);
	writeJobReconnectedEvent(logFile);
	writeJobReconnectFailedEvent(logFile);
	writeGridResourceUpEvent(logFile);
	writeGridResourceDownEvent(logFile);
	writeGridSubmitEvent(logFile);
	writeJobAdInformationEvent(logFile);
	writeJobStatusUnknownEvent(logFile);
	writeJobStatusKnownEvent(logFile);
	writeJobStageInEvent(logFile);
	writeJobStageOutEvent(logFile);
	writeAttributeUpdateEvent(logFile);
	writePreSkipEvent(logFile);
	writeClusterSubmitEvent(logFile);
	writeClusterRemoveEvent(logFile);
	writeFactoryPausedEvent(logFile);
	writeFactoryResumedEvent(logFile);

	exit(0);
}
