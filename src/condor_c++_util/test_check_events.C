/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

static const char * VERSION = "0.9.2";

MULTI_LOG_HASH_INSTANCE; // For the multi-log-file code...
CHECK_EVENTS_HASH_INSTANCE; // For the event checking code...

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

Status
CheckArgs(int argc, char **argv);

void
CheckThisEvent(int line, CheckEvents &ce, const ULogEvent *event,
		CheckEvents::check_event_result_t expectedResult, bool &result);

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
	CheckThisEvent(__LINE__, ce1, &se1, CheckEvents::EVENT_OKAY, result);

		// Re-do the same submit event -- this should generate an error.
	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce1, &se1, CheckEvents::EVENT_ERROR, result);

		// Test a submit event for a new job.
	SubmitEvent		se2;
	se2.cluster = 1234;
	se2.proc = 0;
	se2.subproc = 1;

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce1, &se2, CheckEvents::EVENT_OKAY, result);

		// Test a terminate event.  This should fail because we have
		// two submits for this job.
	JobTerminatedEvent	te1;
	te1.cluster = 1234;
	te1.proc = 0;
	te1.subproc = 0;

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce1, &te1, CheckEvents::EVENT_ERROR, result);

		// Test a job aborted event.
	JobAbortedEvent	ae2;
	ae2.cluster = 1234;
	ae2.proc = 0;
	ae2.subproc = 1;

	printf("Testing job aborted... ");
	CheckThisEvent(__LINE__, ce1, &ae2, CheckEvents::EVENT_OKAY, result);

		// Test an execute event.  This should fail because we already
		// got an aborted event for this job.
	ExecuteEvent exec;
	exec.cluster = 1234;
	exec.proc = 0;
	exec.subproc = 1;

	printf("Testing execute... ");
	CheckThisEvent(__LINE__, ce1, &exec, CheckEvents::EVENT_ERROR, result);

		// Test an execute event.  This should fail because we haven't
		// gotten a submit event for this job.
	exec.cluster = 1234;
	exec.proc = 5;
	exec.subproc = 1;

	printf("Testing execute... ");
	CheckThisEvent(__LINE__, ce1, &exec, CheckEvents::EVENT_ERROR, result);

		// Test a terminate event.  This should fail because we already
		// got an aborted event for this job.
	JobTerminatedEvent	te2;
	te2.cluster = 1234;
	te2.proc = 0;
	te2.subproc = 1;

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce1, &te2, CheckEvents::EVENT_ERROR, result);

		// Test a job aborted event.  This should fail because we
		// don't have a submit event for this job yet.
	JobAbortedEvent	ae3;
	ae3.cluster = 1236;
	ae3.proc = 0;
	ae3.subproc = 0;

	printf("Testing job aborted... ");
	CheckThisEvent(__LINE__, ce1, &ae3, CheckEvents::EVENT_ERROR, result);

		// Test a submit event for the job that just aborted.  This
		// should fail because we got the abort before the submit.
	SubmitEvent		se3;
	se3.cluster = 1236;
	se3.proc = 0;
	se3.subproc = 0;

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce1, &se3, CheckEvents::EVENT_ERROR, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce1.CheckAllJobs(errorMsg) != CheckEvents::EVENT_ERROR) {
		printf("...should have gotten an error (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
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
	CheckThisEvent(__LINE__, ce2, &se1, CheckEvents::EVENT_OKAY, result);

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce2, &se2, CheckEvents::EVENT_OKAY, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce2, &te1, CheckEvents::EVENT_OKAY, result);

		// This should fail because the second job doesn't have a terminate
		// or abort event.
	printf("Testing CheckAllJobs()... ");
	if ( ce2.CheckAllJobs(errorMsg) != CheckEvents::EVENT_ERROR ) {
		printf("...should have gotten an error (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	ExecutableErrorEvent	ee2;
	ee2.cluster = 1234;
	ee2.proc = 0;
	ee2.subproc = 1;

	printf("Testing executable error... ");
	CheckThisEvent(__LINE__, ce2, &ee2, CheckEvents::EVENT_OKAY, result);

	printf("Testing job aborted... ");
	CheckThisEvent(__LINE__, ce2, &ae2, CheckEvents::EVENT_OKAY, result);

	PostScriptTerminatedEvent pt1;
		// Note: special "no submit" ID.
	pt1.cluster = -1;
	pt1.proc = -1;
	pt1.subproc = -1;

	printf("Testing post script terminated...");
	CheckThisEvent(__LINE__, ce2, &pt1, CheckEvents::EVENT_OKAY, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce2.CheckAllJobs(errorMsg) == CheckEvents::EVENT_OKAY ) {
		printf("...succeeded\n");
	} else {
		printf("...FAILED (%s)\n", errorMsg.Value());
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing \"extra\" abort events\n\n");

		// Make a new CheckEvents object that allows extra abort events
		// (apparently Condor sometimes generates a terminate and an abort
		// event).
	CheckEvents		ce3(CheckEvents::ALLOW_TERM_ABORT);

	SubmitEvent		se4;
	se4.cluster = 9876;
	se4.proc = 5;
	se4.subproc = 9;

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce3, &se4, CheckEvents::EVENT_OKAY, result);

	ExecuteEvent exec2;
	exec2.cluster = 9876;
	exec2.proc = 5;
	exec2.subproc = 9;

	printf("Testing execute... ");
	CheckThisEvent(__LINE__, ce3, &exec2, CheckEvents::EVENT_OKAY, result);

	JobTerminatedEvent	te3;
	te3.cluster = 9876;
	te3.proc = 5;
	te3.subproc = 9;

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce3, &te3, CheckEvents::EVENT_OKAY, result);

	JobAbortedEvent	ae4;
	ae4.cluster = 9876;
	ae4.proc = 5;
	ae4.subproc = 9;

	printf("Testing job aborted... ");
	CheckThisEvent(__LINE__, ce3, &ae4, CheckEvents::EVENT_BAD_EVENT, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce3.CheckAllJobs(errorMsg) != CheckEvents::EVENT_BAD_EVENT) {
		printf("...should have gotten a bad event (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	} else {
		printf("...got expected bad event (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing that we catch \"extra\" abort events if we don't "
			"explicitly allow them\n\n");

	CheckEvents		ce4;

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce4, &se4, CheckEvents::EVENT_OKAY, result);

	printf("Testing execute... ");
	CheckThisEvent(__LINE__, ce4, &exec2, CheckEvents::EVENT_OKAY, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce4, &te3, CheckEvents::EVENT_OKAY, result);

	printf("Testing job aborted... ");
	CheckThisEvent(__LINE__, ce4, &ae4, CheckEvents::EVENT_ERROR, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce4.CheckAllJobs(errorMsg) != CheckEvents::EVENT_ERROR ) {
		printf("...should have gotten an error (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing multiple runs\n\n");
	CheckEvents		ce5(CheckEvents::ALLOW_RUN_AFTER_TERM);

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce5, &se4, CheckEvents::EVENT_OKAY, result);

	printf("Testing execute... ");
	CheckThisEvent(__LINE__, ce5, &exec2, CheckEvents::EVENT_OKAY, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce5, &te3, CheckEvents::EVENT_OKAY, result);

	printf("Testing execute... ");
	CheckThisEvent(__LINE__, ce5, &exec2, CheckEvents::EVENT_BAD_EVENT, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce5, &te3, CheckEvents::EVENT_BAD_EVENT, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce5.CheckAllJobs(errorMsg) != CheckEvents::EVENT_BAD_EVENT) {
		printf("...should have gotten a bad event (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	} else {
		printf("...got expected bad event (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing that we catch multiple runs if we don't "
			"explicitly allow them\n\n");

	CheckEvents		ce6;

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce6, &se4, CheckEvents::EVENT_OKAY, result);

	printf("Testing execute... ");
	CheckThisEvent(__LINE__, ce6, &exec2, CheckEvents::EVENT_OKAY, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce6, &te3, CheckEvents::EVENT_OKAY, result);

	printf("Testing execute... ");
	CheckThisEvent(__LINE__, ce6, &exec2, CheckEvents::EVENT_ERROR, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce6, &te3, CheckEvents::EVENT_ERROR, result);

	printf("Testing post script terminated...");
	CheckThisEvent(__LINE__, ce6, &pt1, CheckEvents::EVENT_OKAY, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce6.CheckAllJobs(errorMsg) != CheckEvents::EVENT_ERROR ) {
		printf("...should have gotten an error (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing garbage\n\n");

	CheckEvents		ce7(CheckEvents::ALLOW_GARBAGE);

	SubmitEvent		se5;
	se4.cluster = 1000;
	se4.proc = 0;
	se4.subproc = 0;

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce7, &se5, CheckEvents::EVENT_OKAY, result);

	ExecuteEvent exec3;
	exec3.cluster = 2000; // Note different from submit event!
	exec3.proc = 0;
	exec3.subproc = 0;

	printf("Testing execute... ");
	CheckThisEvent(__LINE__, ce7, &exec3, CheckEvents::EVENT_BAD_EVENT, result);

	JobTerminatedEvent	te4;
	te4.cluster = 3000;
	te4.proc = 0;
	te4.subproc = 0;

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce7, &te4, CheckEvents::EVENT_BAD_EVENT, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce7.CheckAllJobs(errorMsg) != CheckEvents::EVENT_BAD_EVENT) {
		printf("...should have gotten a bad event (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	} else {
		printf("...got expected bad event (%s)\n", errorMsg.Value());
	}

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce7, &te4, CheckEvents::EVENT_ERROR, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce7.CheckAllJobs(errorMsg) != CheckEvents::EVENT_ERROR ) {
		printf("...should have gotten an error (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing all bad events\n\n");

	CheckEvents		ce8(CheckEvents::ALLOW_ALL);

	ExecuteEvent ee8;
	ee8.cluster = 1234;
	ee8.proc = 0;
	ee8.subproc = 0;

	printf("Testing execute... ");
	CheckThisEvent(__LINE__, ce8, &ee8, CheckEvents::EVENT_BAD_EVENT, result);

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce8, &se1, CheckEvents::EVENT_OKAY, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce8, &te1, CheckEvents::EVENT_OKAY, result);

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce8, &se1, CheckEvents::EVENT_BAD_EVENT, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce8, &te1, CheckEvents::EVENT_BAD_EVENT, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce8.CheckAllJobs(errorMsg) != CheckEvents::EVENT_BAD_EVENT) {
		printf("...should have gotten a bad event (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	} else {
		printf("...got expected bad event (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing execute before submit\n\n");

	CheckEvents		ce9(CheckEvents::ALLOW_EXEC_BEFORE_SUBMIT);

	printf("Testing execute... ");
	ExecuteEvent ee9;
	ee9.cluster = 1234;
	ee9.proc = 0;
	ee9.subproc = 0;
	CheckThisEvent(__LINE__, ce9, &ee9, CheckEvents::EVENT_BAD_EVENT, result);

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce9, &se1, CheckEvents::EVENT_OKAY, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce9, &te1, CheckEvents::EVENT_OKAY, result);

		// This should return OKAY because at this point we don't
		// remember that the execute came before the submit.
	printf("\nTesting CheckAllJobs()... ");
	if ( ce9.CheckAllJobs(errorMsg) == CheckEvents::EVENT_OKAY ) {
		printf("...succeeded\n");
	} else {
		printf("...FAILED (%s)\n", errorMsg.Value());
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing double-terminate\n\n");

	CheckEvents		ce10(CheckEvents::ALLOW_DOUBLE_TERMINATE);

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce10, &se1, CheckEvents::EVENT_OKAY, result);

	printf("Testing execute... ");
	ExecuteEvent ee10;
	ee10.cluster = 1234;
	ee10.proc = 0;
	ee10.subproc = 0;
	CheckThisEvent(__LINE__, ce10, &ee10, CheckEvents::EVENT_OKAY, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce10, &te1, CheckEvents::EVENT_OKAY, result);
	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce10, &te1, CheckEvents::EVENT_BAD_EVENT, result);

	printf("Testing post script terminated... ");
	PostScriptTerminatedEvent pt10;
	pt10.cluster = 1234;
	pt10.proc = 0;
	pt10.subproc = 0;
	CheckThisEvent(__LINE__, ce10, &pt10, CheckEvents::EVENT_OKAY, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce10.CheckAllJobs(errorMsg) != CheckEvents::EVENT_BAD_EVENT) {
		printf("...should have gotten a bad event (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	} else {
		printf("...got expected bad event (%s)\n", errorMsg.Value());
	}

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce10, &se2, CheckEvents::EVENT_OKAY, result);

	printf("Testing execute... ");
	ee10.subproc = 1;
	CheckThisEvent(__LINE__, ce10, &ee10, CheckEvents::EVENT_OKAY, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce10, &te2, CheckEvents::EVENT_OKAY, result);

	printf("Testing execute... ");
	CheckThisEvent(__LINE__, ce10, &ee10, CheckEvents::EVENT_ERROR, result);

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce10, &te2, CheckEvents::EVENT_BAD_EVENT, result);

		// We get bad event here instead of error because we don't
		// "remember" that an execute event came after a terminated event.
	if ( ce10.CheckAllJobs(errorMsg) != CheckEvents::EVENT_BAD_EVENT) {
		printf("...should have gotten a bad event (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	} else {
		printf("...got expected bad event (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing running POST scripts after submit failures\n\n");

	CheckEvents		ce11;

	PostScriptTerminatedEvent pt2;
		// ID of POST script terminated event is -1.-1.-1 if all submit
		// attempts of the node job failed.
	pt2.cluster = -1;
	pt2.proc = -1;
	pt2.subproc = -1;

	printf("Testing post script terminated... ");
	CheckThisEvent(__LINE__, ce11, &pt2, CheckEvents::EVENT_OKAY, result);

	printf("Testing post script terminated... ");
	CheckThisEvent(__LINE__, ce11, &pt2, CheckEvents::EVENT_OKAY, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce11.CheckAllJobs(errorMsg) == CheckEvents::EVENT_OKAY ) {
		printf("...succeeded\n");
	} else {
		printf("...FAILED (%s)\n", errorMsg.Value());
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	}

		// Make sure we flag multiple post script terminates for a *real*
		// job as errors...
	SubmitEvent		se11;
	se11.cluster = 1234;
	se11.proc = 0;
	se11.subproc = 0;

	printf("Testing submit... ");
	CheckThisEvent(__LINE__, ce11, &se11, CheckEvents::EVENT_OKAY, result);

	JobTerminatedEvent	te11;
	te11.cluster = 1234;
	te11.proc = 0;
	te11.subproc = 0;

	printf("Testing terminate... ");
	CheckThisEvent(__LINE__, ce11, &te11, CheckEvents::EVENT_OKAY, result);

	pt2.cluster = 1234;
	pt2.proc = 0;
	pt2.subproc = 0;

	printf("Testing post script terminated... ");
	CheckThisEvent(__LINE__, ce11, &pt2, CheckEvents::EVENT_OKAY, result);

	printf("Testing post script terminated... ");
	CheckThisEvent(__LINE__, ce11, &pt2, CheckEvents::EVENT_ERROR, result);

	printf("\nTesting CheckAllJobs()... ");
	if ( ce11.CheckAllJobs(errorMsg) != CheckEvents::EVENT_ERROR) {
		printf("...should have gotten an error (test FAILED)\n");
		printf("########## TEST FAILED HERE (line %d) ##########\n", __LINE__);
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

	printf("\n### Testing terminate after post script terminate\n\n");

	CheckEvents		ce12;

	printf("Testing submit... ");
	SubmitEvent		se12;
	se12.cluster = 1234;
	se12.proc = 1;
	se12.subproc = 0;
	CheckThisEvent(__LINE__, ce12, &se12, CheckEvents::EVENT_OKAY, result);

	printf("Testing execute... ");
	ExecuteEvent ee12;
	ee12.cluster = 1234;
	ee12.proc = 1;
	ee12.subproc = 0;
	CheckThisEvent(__LINE__, ce12, &ee12, CheckEvents::EVENT_OKAY, result);

	printf("Testing post script terminated... ");
	PostScriptTerminatedEvent pt12;
	pt12.cluster = 1234;
	pt12.proc = 1;
	pt12.subproc = 0;
	CheckThisEvent(__LINE__, ce12, &pt12, CheckEvents::EVENT_ERROR, result);

	printf("Testing terminate... ");
	JobTerminatedEvent	te12;
	te12.cluster = 1234;
	te12.proc = 1;
	te12.subproc = 0;
	CheckThisEvent(__LINE__, ce12, &te12, CheckEvents::EVENT_ERROR, result);

	//-------------------------------------------------------------------------

	if ( result ) {
		printf("\nTest SUCCCEEDED\n");
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
CheckThisEvent(int line, CheckEvents &ce, const ULogEvent *event,
		CheckEvents::check_event_result_t expectedResult, bool &result)
{
	MyString		errorMsg;
	bool			failedHere = false;

	CheckEvents::check_event_result_t	tmpResult =
				ce.CheckAnEvent(event, errorMsg);

	if ( expectedResult == CheckEvents::EVENT_OKAY ) {
		if ( tmpResult == CheckEvents::EVENT_OKAY ) {
			printf("...succeeded\n");
		} else {
			printf("...FAILED (%d, %s)\n", tmpResult, errorMsg.Value());
			result = false;
			failedHere = true;
		}
	} else if ( expectedResult == CheckEvents::EVENT_BAD_EVENT ) {
		if ( tmpResult == CheckEvents::EVENT_BAD_EVENT ) {
			printf("...got expected bad event (%s)\n", errorMsg.Value());
		} else {
			printf("...FAILED: expected bad event, got okay or error "
						"(%d, %s)\n", tmpResult, errorMsg.Value());
			result = false;
			failedHere = true;
		}
	} else if ( expectedResult == CheckEvents::EVENT_ERROR ) {
		if ( tmpResult == CheckEvents::EVENT_ERROR ) {
			printf("...got expected error (%s)\n", errorMsg.Value());
		} else {
			printf("...FAILED: expected error, got okay or bad event "
						"(%d, %s)\n", tmpResult, errorMsg.Value());
			result = false;
			failedHere = true;
		}
	}

	if ( failedHere ) {
		printf("########## TEST FAILED HERE (line %d) ##########\n", line);
	}
}
