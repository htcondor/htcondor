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

MULTI_LOG_HASH_INSTANCE; // For the multi-log-file code...
CHECK_EVENTS_HASH_INSTANCE; // For the event checking code...

int main(int argc, char **argv)
{
	bool			result = true;

	CheckEvents		ce1;
	MyString		errorMsg;

		// Test a submit event.
	SubmitEvent		se1;
	se1.cluster = 1234;
	se1.proc = 0;
	se1.subproc = 0;

	printf("Testing submit... ");
	if ( ce1.CheckAnEvent(&se1, errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...failed (%s)\n", errorMsg.Value());
		result = false;
	}

		// Re-do the same submit event -- this should generate an error.
	printf("Testing submit... ");
	if ( ce1.CheckAnEvent(&se1, errorMsg) ) {
		printf("...should have gotten an error (test failed)\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

		// Test a submit event for a new job.
	SubmitEvent		se2;
	se2.cluster = 1234;
	se2.proc = 0;
	se2.subproc = 1;

	printf("Testing submit... ");
	if ( ce1.CheckAnEvent(&se2, errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...failed (%s)\n", errorMsg.Value());
		result = false;
	}

		// Test a terminate event.  This should fail because we have
		// two submits for this job.
	JobTerminatedEvent	te1;
	te1.cluster = 1234;
	te1.proc = 0;
	te1.subproc = 0;

	printf("Testing terminate... ");
	if ( ce1.CheckAnEvent(&te1, errorMsg) ) {
		printf("...should have gotten an error (test failed)\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

		// Test a job aborted event.
	JobAbortedEvent	ae2;
	ae2.cluster = 1234;
	ae2.proc = 0;
	ae2.subproc = 1;

	printf("Testing job aborted... ");
	if ( ce1.CheckAnEvent(&ae2, errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...failed (%s)\n", errorMsg.Value());
		result = false;
	}

		// Test a terminate event.  This should fail because we already
		// got an aborted event for this job.
		// two submits for this job.
	JobTerminatedEvent	te2;
	te2.cluster = 1234;
	te2.proc = 0;
	te2.subproc = 1;

	printf("Testing terminate... ");
	if ( ce1.CheckAnEvent(&te2, errorMsg) ) {
		printf("...should have gotten an error (test failed)\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

		// Test a job aborted event.  This should fail because we
		// don't have a submit event for this job yet.
	JobAbortedEvent	ae3;
	ae3.cluster = 1236;
	ae3.proc = 0;
	ae3.subproc = 0;

	printf("Testing job aborted... ");
	if ( ce1.CheckAnEvent(&ae3, errorMsg) ) {
		printf("...should have gotten an error (test failed)\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

		// Test a submit event for the job that just aborted.  This
		// should fail because we got the abort before the submit.
	SubmitEvent		se3;
	se3.cluster = 1236;
	se3.proc = 0;
	se3.subproc = 0;

	printf("Testing submit... ");
	if ( ce1.CheckAnEvent(&se3, errorMsg) ) {
		printf("...should have gotten an error (test failed)\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	printf("\nTesting CheckAllJobs()... ");
	if ( ce1.CheckAllJobs(errorMsg) ) {
		printf("...should have gotten an error (test failed)\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	//-------------------------------------------------------------------------

		// Make a new CheckEvents object where we only insert "correct"
		// events...
	CheckEvents		ce2;
	printf("\nTesting submit... ");
	if ( ce2.CheckAnEvent(&se1, errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...failed (%s)\n", errorMsg.Value());
		result = false;
	}

	printf("Testing submit... ");
	if ( ce2.CheckAnEvent(&se2, errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...failed (%s)\n", errorMsg.Value());
		result = false;
	}

	printf("Testing terminate... ");
	if ( ce2.CheckAnEvent(&te1, errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...failed (%s)\n", errorMsg.Value());
		result = false;
	}

		// This should fail because the second job doesn't have a terminate
		// or abort event.
	printf("Testing CheckAllJobs()... ");
	if ( ce2.CheckAllJobs(errorMsg) ) {
		printf("...should have gotten an error (test failed)\n");
		result = false;
	} else {
		printf("...got expected error (%s)\n", errorMsg.Value());
	}

	ExecutableErrorEvent	ee2;
	ee2.cluster = 1234;
	ee2.proc = 0;
	ee2.subproc = 1;

	printf("Testing executable error... ");
	if ( ce2.CheckAnEvent(&ee2, errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...failed (%s)\n", errorMsg.Value());
		result = false;
	}

	printf("\nTesting CheckAllJobs()... ");
	if ( ce2.CheckAllJobs(errorMsg) ) {
		printf("...succeeded\n");
	} else {
		printf("...failed (%s)\n", errorMsg.Value());
		result = false;
	}

	if ( result ) {
		printf("\nTest succeeded\n");
		return 0;
	} else {
		printf("\nTest failed\n");
		return 1;
	}
}
