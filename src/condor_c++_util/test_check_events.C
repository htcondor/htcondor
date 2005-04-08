/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include <stdio.h>
#include <string>
#include "string_list.h"
#include "check_events.h"

static const char * VERSION = "0.9.1";

MULTI_LOG_HASH_INSTANCE; // For the multi-log-file code...
CHECK_EVENTS_HASH_INSTANCE; // For the event checking code...

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

Status
CheckArgs(int argc, char **argv);

void
CheckThisEvent(CheckEvents &ce, const ULogEvent *event,
		bool expectedResult, bool expectedGoodEvent, bool &result);

int main(int argc, char **argv)
{
		// Set up the dprintf stuff...
	Termlog = true;
	dprintf_config("test_check_events", 2);
	DebugFlags = D_ALWAYS;

	bool			result = true;

	Status tmpStatus = CheckArgs(argc, argv);
	if ( tmpStatus == STATUS_CANCEL ) {
		return 0;
	} else if ( tmpStatus == STATUS_ERROR ) {
		return 1;
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing various error conditions (multiple "
			"submits, etc.)\n\n");

	CheckEvents		ce1;
	MyString		errorMsg;

		// Test a submit event.
	SubmitEvent		se1;
	se1.cluster = 1234;
	se1.proc = 0;
	se1.subproc = 0;

	printf("Testing submit... ");
	CheckThisEvent(ce1, &se1, true, true, result);

		// Re-do the same submit event -- this should generate an error.
	printf("Testing submit... ");
	CheckThisEvent(ce1, &se1, false, false, result);

		// Test a submit event for a new job.
	SubmitEvent		se2;
	se2.cluster = 1234;
	se2.proc = 0;
	se2.subproc = 1;

	printf("Testing submit... ");
	CheckThisEvent(ce1, &se2, true, true, result);

		// Test a terminate event.  This should fail because we have
		// two submits for this job.
	JobTerminatedEvent	te1;
	te1.cluster = 1234;
	te1.proc = 0;
	te1.subproc = 0;

	printf("Testing terminate... ");
	CheckThisEvent(ce1, &te1, false, false, result);

		// Test a job aborted event.
	JobAbortedEvent	ae2;
	ae2.cluster = 1234;
	ae2.proc = 0;
	ae2.subproc = 1;

	printf("Testing job aborted... ");
	CheckThisEvent(ce1, &ae2, true, true, result);

		// Test an execute event.  This should fail because we already
		// got an aborted event for this job.
	ExecuteEvent exec;
	exec.cluster = 1234;
	exec.proc = 0;
	exec.subproc = 1;

	printf("Testing execute... ");
	CheckThisEvent(ce1, &exec, false, false, result);

		// Test an execute event.  This should fail because we haven't
		// gotten a submit event for this job.
	exec.cluster = 1234;
	exec.proc = 5;
	exec.subproc = 1;

	printf("Testing execute... ");
	CheckThisEvent(ce1, &exec, false, false, result);

		// Test a terminate event.  This should fail because we already
		// got an aborted event for this job.
	JobTerminatedEvent	te2;
	te2.cluster = 1234;
	te2.proc = 0;
	te2.subproc = 1;

	printf("Testing terminate... ");
	CheckThisEvent(ce1, &te2, false, false, result);

		// Test a job aborted event.  This should fail because we
		// don't have a submit event for this job yet.
	JobAbortedEvent	ae3;
	ae3.cluster = 1236;
	ae3.proc = 0;
	ae3.subproc = 0;

	printf("Testing job aborted... ");
	CheckThisEvent(ce1, &ae3, false, false, result);

		// Test a submit event for the job that just aborted.  This
		// should fail because we got the abort before the submit.
	SubmitEvent		se3;
	se3.cluster = 1236;
	se3.proc = 0;
	se3.subproc = 0;

	printf("Testing submit... ");
	CheckThisEvent(ce1, &se3, false, false, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce1.CheckAllJobs(errorMsg) ) {
		printf("...should have gotten an error (test FAILED)\n");
		printf("########## TEST FAILED HERE ##########\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing good events\n\n");

		// Make a new CheckEvents object where we only insert "correct"
		// events...
	CheckEvents		ce2;
	printf("Testing submit... ");
	CheckThisEvent(ce2, &se1, true, true, result);

	printf("Testing submit... ");
	CheckThisEvent(ce2, &se2, true, true, result);

	printf("Testing terminate... ");
	CheckThisEvent(ce2, &te1, true, true, result);

		// This should fail because the second job doesn't have a terminate
		// or abort event.
	printf("Testing CheckAllJobs()... ");
	if ( ce2.CheckAllJobs(errorMsg) ) {
		printf("...should have gotten an error (test FAILED)\n");
		printf("########## TEST FAILED HERE ##########\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	ExecutableErrorEvent	ee2;
	ee2.cluster = 1234;
	ee2.proc = 0;
	ee2.subproc = 1;

	printf("Testing executable error... ");
	CheckThisEvent(ce2, &ee2, true, true, result);

	printf("Testing job aborted... ");
	CheckThisEvent(ce2, &ae2, true, true, result);

	PostScriptTerminatedEvent pe1;
		// Note that ID doesn't match anything else -- this simulates
		// running a post script where node submit failed.
	pe1.cluster = 998866;
	pe1.proc = 0;
	pe1.subproc = 0;

	printf("Testing post script terminated...");
	CheckThisEvent(ce2, &pe1, true, true, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce2.CheckAllJobs(errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...FAILED (%s)\n", errorMsg.Value());
		printf("########## TEST FAILED HERE ##########\n");
		result = false;
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing \"extra\" abort events\n\n");

		// Make a new CheckEvents object that allows extra abort events
		// (apparently Condor sometimes generates a terminate and an abort
		// event).
	CheckEvents		ce3(true, false);

	SubmitEvent		se4;
	se4.cluster = 9876;
	se4.proc = 5;
	se4.subproc = 9;

	printf("Testing submit... ");
	CheckThisEvent(ce3, &se4, true, true, result);

	ExecuteEvent exec2;
	exec2.cluster = 9876;
	exec2.proc = 5;
	exec2.subproc = 9;

	printf("Testing execute... ");
	CheckThisEvent(ce3, &exec2, true, true, result);

	JobTerminatedEvent	te3;
	te3.cluster = 9876;
	te3.proc = 5;
	te3.subproc = 9;

	printf("Testing terminate... ");
	CheckThisEvent(ce3, &te3, true, true, result);

	JobAbortedEvent	ae4;
	ae4.cluster = 9876;
	ae4.proc = 5;
	ae4.subproc = 9;

	printf("Testing job aborted... ");
	CheckThisEvent(ce3, &ae4, true, false, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce3.CheckAllJobs(errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...FAILED (%s)\n", errorMsg.Value());
		printf("########## TEST FAILED HERE ##########\n");
		result = false;
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing that we catch \"extra\" abort events if we don't "
			"explicitly allow them\n\n");

	CheckEvents		ce4;

	printf("Testing submit... ");
	CheckThisEvent(ce4, &se4, true, true, result);

	printf("Testing execute... ");
	CheckThisEvent(ce4, &exec2, true, true, result);

	printf("Testing terminate... ");
	CheckThisEvent(ce4, &te3, true, true, result);

	printf("Testing job aborted... ");
	CheckThisEvent(ce4, &ae4, false, false, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce4.CheckAllJobs(errorMsg) ) {
		printf("...should have gotten an error (test FAILED)\n");
		printf("########## TEST FAILED HERE ##########\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing multiple runs\n\n");
	CheckEvents		ce5(false, true);

	printf("Testing submit... ");
	CheckThisEvent(ce5, &se4, true, true, result);

	printf("Testing execute... ");
	CheckThisEvent(ce5, &exec2, true, true, result);

	printf("Testing terminate... ");
	CheckThisEvent(ce5, &te3, true, true, result);

	printf("Testing execute... ");
	CheckThisEvent(ce5, &exec2, true, false, result);

	printf("Testing terminate... ");
	CheckThisEvent(ce5, &te3, true, false, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce5.CheckAllJobs(errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...FAILED (%s)\n", errorMsg.Value());
		printf("########## TEST FAILED HERE ##########\n");
		result = false;
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing that we catch multiple runs if we don't "
			"explicitly allow them\n\n");

	CheckEvents		ce6;

	printf("Testing submit... ");
	CheckThisEvent(ce6, &se4, true, true, result);

	printf("Testing execute... ");
	CheckThisEvent(ce6, &exec2, true, true, result);

	printf("Testing terminate... ");
	CheckThisEvent(ce6, &te3, true, true, result);

	printf("Testing execute... ");
	CheckThisEvent(ce6, &exec2, false, false, result);

	printf("Testing terminate... ");
	CheckThisEvent(ce6, &te3, false, false, result);

	printf("Testing post script terminated...");
	CheckThisEvent(ce6, &pe1, true, true, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce6.CheckAllJobs(errorMsg) ) {
		printf("...should have gotten an error (test FAILED)\n");
		printf("########## TEST FAILED HERE ##########\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing garbage\n\n");

	CheckEvents		ce7(false, false, true);

	SubmitEvent		se5;
	se4.cluster = 1000;
	se4.proc = 0;
	se4.subproc = 0;

	printf("Testing submit... ");
	CheckThisEvent(ce7, &se5, true, true, result);

	ExecuteEvent exec3;
	exec3.cluster = 2000; // Note different from submit event!
	exec3.proc = 0;
	exec3.subproc = 0;

	printf("Testing execute... ");
	CheckThisEvent(ce7, &exec3, true, false, result);

	JobTerminatedEvent	te4;
	te4.cluster = 3000;
	te4.proc = 0;
	te4.subproc = 0;

	printf("Testing terminate... ");
	CheckThisEvent(ce7, &te4, true, false, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce7.CheckAllJobs(errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...FAILED (%s)\n", errorMsg.Value());
		printf("########## TEST FAILED HERE ##########\n");
		result = false;
	}

	printf("Testing terminate... ");
	CheckThisEvent(ce7, &te4, false, false, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce7.CheckAllJobs(errorMsg) ) {
		printf("...should have gotten an error (test FAILED)\n");
		printf("########## TEST FAILED HERE ##########\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	if ( result ) {
		printf("\nTest succeeded\n");
		return 0;
	} else {
		printf("\nTest FAILED!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		return 1;
	}
}

Status
CheckArgs(int argc, char **argv)
{
	Status	status = STATUS_OK;

	const char *	usage = "Usage: test_check_events [options]\n"
			"  -debug <level>: debug level (e.g., D_FULLDEBUG)\n"
			"  -usage: print this message and exit\n"
			"  -version: print the version number and compile date\n";

	for ( int index = 1; index < argc; ++index ) {
		if ( !strcmp(argv[index], "-debug") ) {
			if ( ++index >= argc ) {
				fprintf(stderr, "Value needed for -debug argument\n");
				printf("%s", usage);
				status = STATUS_ERROR;
			} else {
				set_debug_flags( argv[index] );
			}

		} else if ( !strcmp(argv[index], "-usage") ) {
			printf("%s", usage);
			status = STATUS_CANCEL;

		} else if ( !strcmp(argv[index], "-version") ) {
			printf("test_check_events: %s, %s\n", VERSION, __DATE__);
			status = STATUS_CANCEL;

		} else {
			fprintf(stderr, "Unrecognized argument: <%s>\n", argv[index]);
			printf("%s", usage);
			status = STATUS_ERROR;
		}
	}

	return status;
}

void
CheckThisEvent(CheckEvents &ce, const ULogEvent *event,
		bool expectedResult, bool expectedGoodEvent, bool &result)
{
	MyString		errorMsg;
	bool			eventIsGood;
	bool			failedHere = false;

	bool	tmpResult = ce.CheckAnEvent(event, errorMsg,
				eventIsGood);

	if ( expectedResult ) {
		if ( tmpResult ) {
			printf("...succeeded\n");
		} else {
			printf("...FAILED (%s)\n", errorMsg.Value());
			result = false;
			failedHere = true;
		}
	} else {
		if ( tmpResult ) {
			printf("...should have gotten an error (test FAILED)\n");
			result = false;
			failedHere = true;
		} else {
			printf("...got expected error (%s)\n", errorMsg.Value());
		}
	}

	if ( expectedGoodEvent ) {
		if ( eventIsGood ) {
			// No op.
		} else {
			printf("FAILED: expected good event, got bad event\n");
			result = false;
			failedHere = true;
		}
	} else {
		if ( eventIsGood ) {
			printf("FAILED: expected bad event, got good event\n");
			result = false;
			failedHere = true;
		} else {
			// No op.
		}
	}

	if ( failedHere ) {
		printf("########## TEST FAILED HERE ##########\n");
	}
}
