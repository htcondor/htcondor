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
#include <stdio.h>
#include <string>
#include "string_list.h"
#include "check_events.h"

static const char * VERSION = "0.9.4";

MULTI_LOG_HASH_INSTANCE; // For the multi-log-file code...
CHECK_EVENTS_HASH_INSTANCE; // For the event checking code...

enum Status { STATUS_OK, STATUS_CANCEL, STATUS_ERROR };

Status
CheckArgs(int argc, char **argv);

void
CheckThisEvent(int line, CheckEvents &ce, const ULogEvent *event,
		CheckEvents::check_event_result_t expectedResult, bool &result);
void
CheckResult(int line, CheckEvents::check_event_result_t expectedResult,
		CheckEvents::check_event_result_t tmpResult, MyString &errorMsg,
		bool &result);

int main(int argc, char **argv)
{
		// Set up the dprintf stuff...
	dprintf_set_tool_debug("test_check_events", 0);
	set_debug_flags(NULL, D_ALWAYS);

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

	CheckThisEvent(__LINE__, ce1, &se1, CheckEvents::EVENT_OKAY, result);

		// Re-do the same submit event -- this should generate an error.
	CheckThisEvent(__LINE__, ce1, &se1, CheckEvents::EVENT_ERROR, result);

		// Test a submit event for a new job.
	SubmitEvent		se2;
	se2.cluster = 1234;
	se2.proc = 1;
	se2.subproc = 0;

	CheckThisEvent(__LINE__, ce1, &se2, CheckEvents::EVENT_OKAY, result);

		// Test a terminate event.  This should no longer fail because
		// of better support for "write at least once" semantics.
	JobTerminatedEvent	te1;
	te1.cluster = 1234;
	te1.proc = 0;
	te1.subproc = 0;

	CheckThisEvent(__LINE__, ce1, &te1, CheckEvents::EVENT_OKAY, result);

		// Test a job aborted event.
	JobAbortedEvent	ae2;
	ae2.cluster = 1234;
	ae2.proc = 1;
	ae2.subproc = 0;

	CheckThisEvent(__LINE__, ce1, &ae2, CheckEvents::EVENT_OKAY, result);

		// Test an execute event.  This should fail because we already
		// got an aborted event for this job.
	ExecuteEvent exec;
	exec.cluster = 1234;
	exec.proc = 1;
	exec.subproc = 0;

	CheckThisEvent(__LINE__, ce1, &exec, CheckEvents::EVENT_ERROR, result);

		// Test an execute event.  This should fail because we haven't
		// gotten a submit event for this job.
	exec.cluster = 1234;
	exec.proc = 5;
	exec.subproc = 0;

	CheckThisEvent(__LINE__, ce1, &exec, CheckEvents::EVENT_ERROR, result);

		// Test a terminate event.  This should fail because we already
		// got an aborted event for this job.
	JobTerminatedEvent	te2;
	te2.cluster = 1234;
	te2.proc = 1;
	te2.subproc = 0;

	CheckThisEvent(__LINE__, ce1, &te2, CheckEvents::EVENT_ERROR, result);

		// Test a job aborted event.  This should fail because we
		// don't have a submit event for this job yet.
	JobAbortedEvent	ae3;
	ae3.cluster = 1236;
	ae3.proc = 0;
	ae3.subproc = 0;

	CheckThisEvent(__LINE__, ce1, &ae3, CheckEvents::EVENT_ERROR, result);

		// Test a submit event for the job that just aborted.  This
		// should fail because we got the abort before the submit.
	SubmitEvent		se3;
	se3.cluster = 1236;
	se3.proc = 0;
	se3.subproc = 0;

	CheckThisEvent(__LINE__, ce1, &se3, CheckEvents::EVENT_ERROR, result);

	printf("\nTesting CheckAllJobs()... ");
	CheckEvents::check_event_result_t tmpResult = ce1.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_ERROR, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing good events\n\n");

		// Make a new CheckEvents object where we only insert "correct"
		// events...
	CheckEvents		ce2;
	CheckThisEvent(__LINE__, ce2, &se1, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce2, &se2, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce2, &te1, CheckEvents::EVENT_OKAY, result);

		// This should fail because the second job doesn't have a terminate
		// or abort event.
	printf("Testing CheckAllJobs()... ");
	tmpResult = ce2.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_ERROR, tmpResult, errorMsg,
			result);

	ExecutableErrorEvent	ee2;
	ee2.cluster = 1234;
	ee2.proc = 1;
	ee2.subproc = 0;

	CheckThisEvent(__LINE__, ce2, &ee2, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce2, &ae2, CheckEvents::EVENT_OKAY, result);

	PostScriptTerminatedEvent pt1;
		// Note: special "no submit" ID.
	pt1.cluster = -1;
	pt1.proc = 0;
	pt1.subproc = 0;

	CheckThisEvent(__LINE__, ce2, &pt1, CheckEvents::EVENT_OKAY, result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce2.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_OKAY, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing \"extra\" abort events\n\n");

		// Make a new CheckEvents object that allows extra abort events
		// (apparently Condor sometimes generates a terminate and an abort
		// event).
	CheckEvents		ce3(CheckEvents::ALLOW_TERM_ABORT);

	SubmitEvent		se4;
	se4.cluster = 9876;
	se4.proc = 5;
	se4.subproc = 0;

	CheckThisEvent(__LINE__, ce3, &se4, CheckEvents::EVENT_OKAY, result);

	ExecuteEvent exec2;
	exec2.cluster = 9876;
	exec2.proc = 5;
	exec2.subproc = 0;

	CheckThisEvent(__LINE__, ce3, &exec2, CheckEvents::EVENT_OKAY, result);

	JobTerminatedEvent	te3;
	te3.cluster = 9876;
	te3.proc = 5;
	te3.subproc = 0;

	CheckThisEvent(__LINE__, ce3, &te3, CheckEvents::EVENT_OKAY, result);

	JobAbortedEvent	ae4;
	ae4.cluster = 9876;
	ae4.proc = 5;
	ae4.subproc = 0;

	CheckThisEvent(__LINE__, ce3, &ae4, CheckEvents::EVENT_BAD_EVENT, result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce3.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_BAD_EVENT, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing that we catch \"extra\" abort events if we don't "
			"explicitly allow them\n\n");

	CheckEvents		ce4;

	CheckThisEvent(__LINE__, ce4, &se4, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce4, &exec2, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce4, &te3, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce4, &ae4, CheckEvents::EVENT_ERROR, result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce4.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_ERROR, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing multiple runs\n\n");
	CheckEvents		ce5(CheckEvents::ALLOW_RUN_AFTER_TERM);

	CheckThisEvent(__LINE__, ce5, &se4, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce5, &exec2, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce5, &te3, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce5, &exec2, CheckEvents::EVENT_BAD_EVENT, result);

	CheckThisEvent(__LINE__, ce5, &te3, CheckEvents::EVENT_BAD_EVENT, result);

	printf("\nTesting CheckAllJobs()... ");
		// Note that here we're no longer explicitly allowing "run after
		// terminate", but this check doesn't fail because we don't know
		// whether any of the terminates came before the executes.
	ce5.SetAllowEvents(CheckEvents::ALLOW_DOUBLE_TERMINATE);
	tmpResult = ce5.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_BAD_EVENT, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing that we catch multiple runs if we don't "
			"explicitly allow them\n\n");

	CheckEvents		ce6;

	CheckThisEvent(__LINE__, ce6, &se4, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce6, &exec2, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce6, &te3, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce6, &exec2, CheckEvents::EVENT_ERROR, result);

	CheckThisEvent(__LINE__, ce6, &te3, CheckEvents::EVENT_ERROR, result);

	CheckThisEvent(__LINE__, ce6, &pt1, CheckEvents::EVENT_OKAY, result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce6.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_ERROR, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing garbage\n\n");

	CheckEvents		ce7(CheckEvents::ALLOW_GARBAGE);

	SubmitEvent		se5;
	se4.cluster = 1000;
	se4.proc = 0;
	se4.subproc = 0;

	CheckThisEvent(__LINE__, ce7, &se5, CheckEvents::EVENT_OKAY, result);

	ExecuteEvent exec3;
	exec3.cluster = 2000; // Note different from submit event!
	exec3.proc = 0;
	exec3.subproc = 0;

	CheckThisEvent(__LINE__, ce7, &exec3, CheckEvents::EVENT_WARNING, result);

	JobTerminatedEvent	te4;
	te4.cluster = 3000;
	te4.proc = 0;
	te4.subproc = 0;

	CheckThisEvent(__LINE__, ce7, &te4, CheckEvents::EVENT_WARNING, result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce7.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_BAD_EVENT, tmpResult, errorMsg,
			result);

	CheckThisEvent(__LINE__, ce7, &te4, CheckEvents::EVENT_ERROR, result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce7.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_ERROR, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing almost all bad events\n\n");

	CheckEvents		ce8(CheckEvents::ALLOW_ALMOST_ALL);

	ExecuteEvent exec8;
	exec8.cluster = 1234;
	exec8.proc = 0;
	exec8.subproc = 0;

	CheckThisEvent(__LINE__, ce8, &exec8, CheckEvents::EVENT_WARNING, result);

	CheckThisEvent(__LINE__, ce8, &se1, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce8, &te1, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce8, &se1, CheckEvents::EVENT_BAD_EVENT, result);

		// ALLOW_ALMOST_ALL should *not* allow execute after terminate.
	CheckThisEvent(__LINE__, ce8, &exec8, CheckEvents::EVENT_ERROR, result);

	CheckThisEvent(__LINE__, ce8, &te1, CheckEvents::EVENT_BAD_EVENT, result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce8.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_BAD_EVENT, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing execute before submit\n\n");

	CheckEvents		ce9(CheckEvents::ALLOW_EXEC_BEFORE_SUBMIT);

	ExecuteEvent exec9;
	exec9.cluster = 1234;
	exec9.proc = 0;
	exec9.subproc = 0;
	CheckThisEvent(__LINE__, ce9, &exec9, CheckEvents::EVENT_WARNING, result);

	CheckThisEvent(__LINE__, ce9, &se1, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce9, &te1, CheckEvents::EVENT_OKAY, result);

		// This should return OKAY because at this point we don't
		// remember that the execute came before the submit.
	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce9.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_OKAY, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing double-terminate\n\n");

	CheckEvents		ce10(CheckEvents::ALLOW_DOUBLE_TERMINATE);

	CheckThisEvent(__LINE__, ce10, &se1, CheckEvents::EVENT_OKAY, result);

	ExecuteEvent exec10;
	exec10.cluster = 1234;
	exec10.proc = 0;
	exec10.subproc = 0;
	CheckThisEvent(__LINE__, ce10, &exec10, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce10, &te1, CheckEvents::EVENT_OKAY, result);
	CheckThisEvent(__LINE__, ce10, &te1, CheckEvents::EVENT_BAD_EVENT, result);

	PostScriptTerminatedEvent pt10;
	pt10.cluster = 1234;
	pt10.proc = 0;
	pt10.subproc = 0;
	CheckThisEvent(__LINE__, ce10, &pt10, CheckEvents::EVENT_OKAY, result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce10.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_BAD_EVENT, tmpResult, errorMsg,
			result);

	CheckThisEvent(__LINE__, ce10, &se2, CheckEvents::EVENT_OKAY, result);

	exec10.proc = 1;
	CheckThisEvent(__LINE__, ce10, &exec10, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce10, &te2, CheckEvents::EVENT_OKAY, result);

		// Terminate then execute should still be an error.
	CheckThisEvent(__LINE__, ce10, &exec10, CheckEvents::EVENT_ERROR, result);

	CheckThisEvent(__LINE__, ce10, &te2, CheckEvents::EVENT_BAD_EVENT, result);

		// We get bad event here instead of error because we don't
		// "remember" that an execute event came after a terminated event.
	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce10.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_BAD_EVENT, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing running POST scripts after submit failures\n\n");

	CheckEvents		ce11;

	PostScriptTerminatedEvent pt2;
		// ID of POST script terminated event is -1.0.0 if all submit
		// attempts of the node job failed.
	pt2.cluster = -1;
	pt2.proc = 0;
	pt2.subproc = 0;

	CheckThisEvent(__LINE__, ce11, &pt2, CheckEvents::EVENT_OKAY, result);

		// This is okay because we specifically allow duplicates with the
		// special "submit attempts failed" ID.
	CheckThisEvent(__LINE__, ce11, &pt2, CheckEvents::EVENT_OKAY, result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce11.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_OKAY, tmpResult, errorMsg,
			result);

		// Make sure we flag multiple post script terminates for a *real*
		// job as errors...
	SubmitEvent		se11;
	se11.cluster = 1234;
	se11.proc = 0;
	se11.subproc = 0;

	CheckThisEvent(__LINE__, ce11, &se11, CheckEvents::EVENT_OKAY, result);

	JobTerminatedEvent	te11;
	te11.cluster = 1234;
	te11.proc = 0;
	te11.subproc = 0;

	CheckThisEvent(__LINE__, ce11, &te11, CheckEvents::EVENT_OKAY, result);

	pt2.cluster = 1234;
	pt2.proc = 0;
	pt2.subproc = 0;

	CheckThisEvent(__LINE__, ce11, &pt2, CheckEvents::EVENT_OKAY, result);

	CheckThisEvent(__LINE__, ce11, &pt2, CheckEvents::EVENT_ERROR, result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce11.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_ERROR, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing terminate after post script terminate\n\n");

	CheckEvents		ce12;

	SubmitEvent		se12;
	se12.cluster = 1234;
	se12.proc = 1;
	se12.subproc = 0;
	CheckThisEvent(__LINE__, ce12, &se12, CheckEvents::EVENT_OKAY, result);

	ExecuteEvent exec12;
	exec12.cluster = 1234;
	exec12.proc = 1;
	exec12.subproc = 0;
	CheckThisEvent(__LINE__, ce12, &exec12, CheckEvents::EVENT_OKAY, result);

	PostScriptTerminatedEvent pt12;
	pt12.cluster = 1234;
	pt12.proc = 1;
	pt12.subproc = 0;
	CheckThisEvent(__LINE__, ce12, &pt12, CheckEvents::EVENT_ERROR, result);

	JobTerminatedEvent	te12;
	te12.cluster = 1234;
	te12.proc = 1;
	te12.subproc = 0;
	CheckThisEvent(__LINE__, ce12, &te12, CheckEvents::EVENT_ERROR, result);

	//-------------------------------------------------------------------------

	printf("\n### Testing parallel universe\n\n");

	CheckEvents		ce13;

	SubmitEvent		se13;
	se13.cluster = 101176;
	se13.proc = 0;
	se13.subproc = 0;
	CheckThisEvent(__LINE__, ce13, &se13, CheckEvents::EVENT_OKAY, result);

	NodeExecuteEvent	ne13;
	ne13.cluster = 101176;
	ne13.proc = 0;
	ne13.subproc = 0;
	CheckThisEvent(__LINE__, ce13, &ne13, CheckEvents::EVENT_OKAY, result);

	ne13.subproc = 1;
	CheckThisEvent(__LINE__, ce13, &ne13, CheckEvents::EVENT_OKAY, result);

	ExecuteEvent	exec13;
	exec13.cluster = 101176;
	exec13.proc = 0;
	exec13.subproc = 0;
	CheckThisEvent(__LINE__, ce13, &exec13, CheckEvents::EVENT_OKAY, result);

	NodeExecuteEvent	nt13;
	nt13.cluster = 101176;
	nt13.proc = 0;
	nt13.subproc = 0;
	CheckThisEvent(__LINE__, ce13, &nt13, CheckEvents::EVENT_OKAY, result);

	nt13.subproc = 1;
	CheckThisEvent(__LINE__, ce13, &nt13, CheckEvents::EVENT_OKAY, result);

	JobTerminatedEvent	te13;
	te13.cluster = 101176;
	te13.proc = 0;
	te13.subproc = 0;
	CheckThisEvent(__LINE__, ce13, &te13, CheckEvents::EVENT_OKAY, result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce13.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_OKAY, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing duplicate events\n\n");

	CheckEvents		ce14(CheckEvents::ALLOW_DUPLICATE_EVENTS);

	SubmitEvent		se14;
	se14.cluster = 123;
	se14.proc = 0;
	se14.subproc = 0;
	CheckThisEvent(__LINE__, ce14, &se14, CheckEvents::EVENT_OKAY, result);
	CheckThisEvent(__LINE__, ce14, &se14, CheckEvents::EVENT_BAD_EVENT,
			result);

	ExecuteEvent exec14;
	exec14.cluster = 123;
	exec14.proc = 0;
	exec14.subproc = 0;
	CheckThisEvent(__LINE__, ce14, &exec14, CheckEvents::EVENT_OKAY, result);
	CheckThisEvent(__LINE__, ce14, &exec14, CheckEvents::EVENT_OKAY, result);

	JobTerminatedEvent	te14;
	te14.cluster = 123;
	te14.proc = 0;
	te14.subproc = 0;
	CheckThisEvent(__LINE__, ce14, &te14, CheckEvents::EVENT_OKAY, result);
	CheckThisEvent(__LINE__, ce14, &te14, CheckEvents::EVENT_BAD_EVENT,
			result);

	PostScriptTerminatedEvent pt14;
	pt14.cluster = 123;
	pt14.proc = 0;
	pt14.subproc = 0;
	CheckThisEvent(__LINE__, ce14, &pt14, CheckEvents::EVENT_OKAY, result);
	CheckThisEvent(__LINE__, ce14, &pt14, CheckEvents::EVENT_BAD_EVENT,
			result);

	CheckThisEvent(__LINE__, ce14, &te14, CheckEvents::EVENT_BAD_EVENT,
			result);

	//~~~~~~~~~~~~~~~~~~~~~

	se14.cluster = 124;
	CheckThisEvent(__LINE__, ce14, &se14, CheckEvents::EVENT_OKAY, result);
	CheckThisEvent(__LINE__, ce14, &se14, CheckEvents::EVENT_BAD_EVENT,
			result);

	exec14.cluster = 124;
	CheckThisEvent(__LINE__, ce14, &exec14, CheckEvents::EVENT_OKAY, result);
	CheckThisEvent(__LINE__, ce14, &exec14, CheckEvents::EVENT_OKAY, result);

	JobAbortedEvent	ae14;
	ae14.cluster = 124;
	ae14.proc = 0;
	ae14.subproc = 0;
	CheckThisEvent(__LINE__, ce14, &ae14, CheckEvents::EVENT_OKAY, result);
	CheckThisEvent(__LINE__, ce14, &ae14, CheckEvents::EVENT_BAD_EVENT,
			result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce14.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_BAD_EVENT, tmpResult, errorMsg,
			result);

	//-------------------------------------------------------------------------

	printf("\n### Testing allowing terminated before submit\n\n");

	CheckEvents		ce15(CheckEvents::ALLOW_EXEC_BEFORE_SUBMIT);

	ExecuteEvent exec15;
	exec15.cluster = 123;
	exec15.proc = 0;
	exec15.subproc = 0;
	CheckThisEvent(__LINE__, ce15, &exec15, CheckEvents::EVENT_WARNING,
			result);

	JobTerminatedEvent	te15;
	te15.cluster = 123;
	te15.proc = 0;
	te15.subproc = 0;
	CheckThisEvent(__LINE__, ce15, &te15, CheckEvents::EVENT_WARNING,
			result);

	SubmitEvent		se15;
	se15.cluster = 123;
	se15.proc = 0;
	se15.subproc = 0;
	CheckThisEvent(__LINE__, ce15, &se15, CheckEvents::EVENT_BAD_EVENT,
			result);

	printf("\nTesting CheckAllJobs()... ");
	tmpResult = ce15.CheckAllJobs(errorMsg);
	CheckResult(__LINE__, CheckEvents::EVENT_OKAY, tmpResult, errorMsg,
			result);

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
				set_debug_flags( argv[index], 0 );
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

	printf("Testing %s (%d.%d.%d)... ", event->eventName(), event->cluster,
			event->proc, event->subproc);

	CheckEvents::check_event_result_t	tmpResult =
				ce.CheckAnEvent(event, errorMsg);

	CheckResult(line, expectedResult, tmpResult, errorMsg, result);
}

void
CheckResult(int line, CheckEvents::check_event_result_t expectedResult,
		CheckEvents::check_event_result_t tmpResult, MyString &errorMsg,
		bool &result)
{
	bool			failedHere = false;

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
			printf("...FAILED: expected bad event (%d), got something else "
						"(%d, %s)\n", CheckEvents::EVENT_BAD_EVENT,
						tmpResult, errorMsg.Value());
			result = false;
			failedHere = true;
		}
	} else if ( expectedResult == CheckEvents::EVENT_ERROR ) {
		if ( tmpResult == CheckEvents::EVENT_ERROR ) {
			printf("...got expected error (%s)\n", errorMsg.Value());
		} else {
			printf("...FAILED: expected error (%d), got something else "
						"(%d, %s)\n", CheckEvents::EVENT_ERROR, tmpResult,
						errorMsg.Value());
			result = false;
			failedHere = true;
		}
	} else if ( expectedResult == CheckEvents::EVENT_WARNING ) {
		if ( tmpResult == CheckEvents::EVENT_WARNING ) {
			printf("...got expected warning (%s)\n", errorMsg.Value());
		} else {
			printf("...FAILED: expected warning (%d), got something else "
						"(%d, %s)\n", CheckEvents::EVENT_WARNING, tmpResult,
						errorMsg.Value());
			result = false;
			failedHere = true;
		}
	}

	if ( failedHere ) {
		printf("########## TEST FAILED HERE (line %d) ##########\n", line);
	}
}
